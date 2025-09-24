#ifndef CNA1_H
#define CNA1_H
#include <atomic>
#include <cstdint>
#include <random>
#include <cassert>

#ifdef __linux__
#include <unistd.h>
#include <sched.h>
#endif

// CNA Lock Implementation
struct cna_node {
    std::atomic<uintptr_t> spin;
    int socket;
    cna_node* secTail;
    std::atomic<cna_node*> next;
    
    cna_node() : spin(0), socket(-1), secTail(nullptr), next(nullptr) {}
};

// Thread-local node for CNA lock
thread_local cna_node* cna_thread_node = nullptr;

class CNALock {
private:
    std::atomic<cna_node*> tail;
    
    // Helper function to get current NUMA node
    static int current_numa_node() {
#ifdef __linux__
        return sched_getcpu() / 2;  // Simplified NUMA node detection
#else
        return 0;  // Default to node 0 on non-Linux systems
#endif
    }
    
    // Pseudo-random number generator for fairness threshold
    static uint32_t pseudo_rand() {
        static thread_local uint32_t seed = std::hash<std::thread::id>{}(std::this_thread::get_id());
        seed = seed * 1103515245 + 12345;
        return seed;
    }
    
    // Long-term fairness threshold
    static constexpr uint32_t THRESHOLD = 0xffff;
    
    static bool keep_lock_local() {
        return pseudo_rand() & THRESHOLD;
    }
    
    cna_node* find_successor(cna_node* me) {
        cna_node* next = me->next.load(std::memory_order_acquire);
        int mySocket = me->socket;
        if (mySocket == -1) mySocket = current_numa_node();
        
        // Check if my immediate successor is on the same socket
        if (next->socket == mySocket) return next;
        
        cna_node* secHead = next;
        cna_node* secTail = next;
        cna_node* cur = next->next.load(std::memory_order_acquire);
        
        // Traverse the main queue
        while (cur) {
            // Check if cur is running on my socket
            if (cur->socket == mySocket) {
                if (me->spin.load(std::memory_order_relaxed) > 1) {
                    ((cna_node*)(me->spin.load(std::memory_order_relaxed)))->secTail->next.store(secHead, std::memory_order_release);
                } else {
                    me->spin.store((uintptr_t)secHead, std::memory_order_release);
                }
                secTail->next.store(nullptr, std::memory_order_release);
                ((cna_node*)(me->spin.load(std::memory_order_relaxed)))->secTail = secTail;
                return cur;
            }
            secTail = cur;
            cur = cur->next.load(std::memory_order_acquire);
        }
        return nullptr;
    }
    
public:
    CNALock() : tail(nullptr) {}
    
    void Acquire(cna_node* me) {
        me->next.store(nullptr, std::memory_order_relaxed);
        me->socket = -1;
        me->spin.store(0, std::memory_order_relaxed);
        
        // Add myself to the main queue
        cna_node* tail_node = tail.exchange(me, std::memory_order_acq_rel);
        
        // No one there?
        if (!tail_node) {
            me->spin.store(1, std::memory_order_release);
            return;
        }
        
        // Someone there, need to link in
        me->socket = current_numa_node();
        tail_node->next.store(me, std::memory_order_release);
        
        // Wait for the lock to become available
        while (!me->spin.load(std::memory_order_acquire)) {
            // CPU_PAUSE equivalent
            std::this_thread::yield();
        }
    }
    // Thread-local node management versions
    void Acquire() {
        if (cna_thread_node == nullptr) {
            cna_thread_node = new cna_node();
        }
        Acquire(cna_thread_node);
    }

    
    void Release(cna_node* me) {
        // Is there a successor in the main queue?
        if (!me->next.load(std::memory_order_acquire)) {
            // Is there a node in the secondary queue?
            if (me->spin.load(std::memory_order_relaxed) == 1) {
                // If not, try to set tail to NULL, indicating that
                // both main and secondary queues are empty
                cna_node* expected = me;
                if (tail.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel)) {
                    return;
                }
            } else {
                // Otherwise, try to set tail to the last node in
                // the secondary queue
                cna_node* secHead = (cna_node*)me->spin.load(std::memory_order_relaxed);
                cna_node* expected = me;
                if (tail.compare_exchange_strong(expected, secHead->secTail, std::memory_order_acq_rel)) {
                    // If successful, pass the lock to the head of
                    // the secondary queue
                    secHead->spin.store(1, std::memory_order_release);
                    return;
                }
            }
            // Wait for successor to appear
            while (me->next.load(std::memory_order_acquire) == nullptr) {
                std::this_thread::yield();
            }
        }
        
        // Determine the next lock holder and pass the lock by
        // setting its spin field
        cna_node* succ = nullptr;
        if (keep_lock_local() && (succ = find_successor(me))) {
            succ->spin.store(me->spin.load(std::memory_order_relaxed), std::memory_order_release);
        } else if (me->spin.load(std::memory_order_relaxed) > 1) {
            succ = (cna_node*)me->spin.load(std::memory_order_relaxed);
            succ->secTail->next.store(me->next.load(std::memory_order_acquire), std::memory_order_release);
            succ->spin.store(1, std::memory_order_release);
        } else {
            me->next.load(std::memory_order_acquire)->spin.store(1, std::memory_order_release);
        }
    }

    // Thread-local node management version
    void Release() {
        assert(cna_thread_node != nullptr);
        Release(cna_thread_node);
    }
};

#endif // CNA1_H