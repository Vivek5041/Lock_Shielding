#ifndef HEM3_H
#define HEM3_H

#include <atomic>
#include <cassert>
#include "shielding_array.h"

// Thread class representing each thread's state
class Thread {
public:
    std::atomic<void*> Grant;  // Grant field for each thread
    Thread() : Grant(nullptr) {}
};

// Thread-local thread object for each thread
thread_local Thread* Self = nullptr;

class HemLock {
private:
    std::atomic<Thread*> Tail;  // Shared lock tail

    static void lock_body(void* l) {
        auto* lock = static_cast<HemLock*>(l);
        
        // Initialize thread-local Self if not already done
        if (Self == nullptr) {
            Self = new Thread();
        }
        
        assert(Self->Grant.load(std::memory_order_relaxed) == nullptr);
        
        // Enqueue self at tail of implicit queue
        Thread* pred = lock->Tail.exchange(Self, std::memory_order_acq_rel);
        
        if (pred != nullptr) {
            // Contention: must wait
            // Wait until predecessor's Grant points to this lock
            while (pred->Grant.load(std::memory_order_acquire) != lock) {
                // Pause/busy wait
            }
            // Clear predecessor's Grant
            pred->Grant.store(nullptr, std::memory_order_release);
        }
        
        assert(lock->Tail.load(std::memory_order_relaxed) != nullptr);
    }
    
    static void unlock_body(void* l) {
        auto* lock = static_cast<HemLock*>(l);
        
        assert(Self->Grant.load(std::memory_order_relaxed) == nullptr);
        
        // Try to atomically remove self from tail
        Thread* expected = Self;
        Thread* v = nullptr;
        bool cas_success = lock->Tail.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
        
        if (cas_success) {
            v = Self;  // CAS succeeded, v = Self
        } else {
            v = expected;  // CAS failed, v = current tail value
        }
        
        assert(v != nullptr);
        
        if (v != Self) {
            // One or more waiters exist -- convey ownership to successor
            Self->Grant.store(lock, std::memory_order_release);
            
            // Wait until successor clears our Grant
            while (Self->Grant.load(std::memory_order_acquire) != nullptr) {
                // Pause/busy wait
            }
        }
    }
    
public:
    HemLock() : Tail(nullptr) {}
    
    void Acquire() {
        if (LS_ACQUIRE(this, false, lock_body) != LS_Status::LS_ACQUIRE_NOW) {
            return;
        }
    }
    
    void Release() {
        if (LS_RELEASE(this, false, unlock_body) != LS_Status::LS_RELEASE_NOW) {
            return;
        }
    }
};

#endif // HEM3_H