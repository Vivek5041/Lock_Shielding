#ifndef HEM1_H
#define HEM1_H
#include <atomic>
#include <cassert>

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

public:
    HemLock() : Tail(nullptr) {}
    
    void Acquire() {
        // Initialize thread-local Self if not already done
        if (Self == nullptr) {
            Self = new Thread();
        }
        
        assert(Self->Grant.load(std::memory_order_relaxed) == nullptr);
        
        // Enqueue self at tail of implicit queue
        Thread* pred = Tail.exchange(Self, std::memory_order_acq_rel);
        
        if (pred != nullptr) {
            // Contention: must wait
            // Wait until predecessor's Grant points to this lock
            while (pred->Grant.load(std::memory_order_acquire) != this) {
                // Pause/busy wait
            }
            // Clear predecessor's Grant
            pred->Grant.store(nullptr, std::memory_order_release);
        }
        
        assert(Tail.load(std::memory_order_relaxed) != nullptr);
    }
    
    void Release() {
        assert(Self->Grant.load(std::memory_order_relaxed) == nullptr);
        
        // Try to atomically remove self from tail
        Thread* expected = Self;
        Thread* v = nullptr;
        bool cas_success = Tail.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
        
        if (cas_success) {
            v = Self;  // CAS succeeded, v = Self
        } else {
            v = expected;  // CAS failed, v = current tail value
        }
        
        assert(v != nullptr);
        
        if (v != Self) {
            // One or more waiters exist -- convey ownership to successor
            Self->Grant.store(this, std::memory_order_release);
            
            // Wait until successor clears our Grant
            while (Self->Grant.load(std::memory_order_acquire) != nullptr) {
                // Pause/busy wait
            }
        }
    }
};

#endif // HEM1_H