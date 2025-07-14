#ifndef CLH3_H
#define CLH3_H

#include <atomic>
#include "shielding_array.h"

struct CLHNode {
    std::atomic<bool> locked{false};
};

// Thread-local nodes - similar to working version
thread_local CLHNode* myNode = nullptr;
thread_local CLHNode* myPred = nullptr;

class CLHLock {
private:
    std::atomic<CLHNode*> tail{nullptr};
    
    static void lock_body(void* l, void* /*unused*/) {
        auto* lock = static_cast<CLHLock*>(l);
        
        // Initialize myNode if needed (first time)
        if (myNode == nullptr) {
            myNode = new CLHNode();
        }
        
        myNode->locked.store(true, std::memory_order_relaxed);
        CLHNode* pred = lock->tail.exchange(myNode, std::memory_order_acq_rel);
        myPred = pred;
        
        if (pred != nullptr) {
            while (pred->locked.load(std::memory_order_acquire)) {
                // spin
            }
        }
    }
    
    static void unlock_body(void* l, void* /*unused*/) {
        myNode->locked.store(false, std::memory_order_release);
        // Swap nodes like in the working version
        std::swap(myNode, myPred);
    }
    
public:
    void Acquire() {
        if (LS_ACQUIRE(this, false, lock_body, nullptr) != LS_Status::LS_ACQUIRE_NOW) {
            return;
        }
    }
    
    void Release() {
        if (LS_RELEASE(this, false, unlock_body, nullptr) != LS_Status::LS_RELEASE_NOW) {
            return;
        }
    }
};

#endif // CLH3_H