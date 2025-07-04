#ifndef MCS4_H
#define MCS4_H

#include <atomic>
#include "shielding_hash.h"

struct Node {
    std::atomic<Node*> next{nullptr};  // Pointer to the next node in the queue
    std::atomic<bool> locked{true};   // Indicates if the thread is waiting
};

// Define a thread-local node for each thread
thread_local Node myNode;

class MCSLock {
private:
    std::atomic<Node*> tail{nullptr};  // Global queue tail

    static void lock_body(void* l, void* me) {
        Node* myNode = static_cast<Node*>(me);
        auto* lock = static_cast<MCSLock*>(l);

        myNode->next.store(nullptr);
        Node* prev = lock->tail.exchange(myNode);

        if (prev != nullptr) {
            prev->next.store(myNode);
            while (myNode->locked.load(std::memory_order_acquire)); // Spin
        }
    }

    static void unlock_body(void* l, void* me) {
        Node* myNode = static_cast<Node*>(me);
        auto* lock = static_cast<MCSLock*>(l);

        if (!myNode->next.load()) {
            Node* expected = myNode;
            if (lock->tail.compare_exchange_strong(expected, nullptr)) return;

            while (myNode->next.load() == nullptr); // Spin
        }

        myNode->next.load()->locked.store(false, std::memory_order_release);
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

#endif // MCS4_H