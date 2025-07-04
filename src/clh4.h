#ifndef CLH4_H
#define CLH4_H

#include <atomic>
#include "shielding_hash.h"

struct CLHNode {
    std::atomic<bool> locked{true};
};

// Thread-local QNode pointer and instance
thread_local CLHNode* myNode = nullptr;
thread_local CLHNode* myPred = nullptr;
thread_local CLHNode myNodeStorage;

class CLHLock {
private:
    std::atomic<CLHNode*> tail{nullptr};

    static void lock_body(void* l, void* me) {
        auto* lock = static_cast<CLHLock*>(l);
        auto** myNodePtr = static_cast<CLHNode**>(me);

        // Ensure thread-local QNode is initialized
        if (*myNodePtr == nullptr) {
            *myNodePtr = &myNodeStorage;
        }

        CLHNode* node = *myNodePtr;
        node->locked.store(true, std::memory_order_relaxed);

        CLHNode* pred = lock->tail.exchange(node, std::memory_order_acq_rel);
        myPred = pred;

        if (pred != nullptr) {
            while (pred->locked.load(std::memory_order_acquire)); // Spin
        }
    }

    static void unlock_body(void* l, void* me) {
        CLHNode* node = *static_cast<CLHNode**>(me);

        node->locked.store(false, std::memory_order_release);
        myNode = myPred; // Move pointer reuse forward
    }

public:
    void Acquire() {
        if (LS_ACQUIRE(this, false, lock_body, &myNode) != LS_Status::LS_ACQUIRE_NOW) {
            return;
        }
    }

    void Release() {
        if (LS_RELEASE(this, false, unlock_body, &myNode) != LS_Status::LS_RELEASE_NOW) {
            return;
        }
    }
};

#endif // CLH4_H

