#ifndef K424_H
#define K424_H

#include <atomic>
#include "shielding_hash.h"

class K42Lock {
private:
    struct Node {
        std::atomic<Node*> next;
        union {
            std::atomic<bool> locked;
            std::atomic<Node*> tail;
        };

        Node() : next(nullptr), locked(false) {}
        explicit Node(bool is_lock_head) : next(nullptr) {
            if (is_lock_head)
                tail.store(nullptr, std::memory_order_relaxed);
            else
                locked.store(false, std::memory_order_relaxed);
        }
    };

    Node lock_head;

    static void lock_body(void* l, void* /*unused*/) {
        auto* lock = static_cast<K42Lock*>(l);
        Node* I = new Node();
        I->next.store(nullptr, std::memory_order_relaxed);

        Node* pred = lock->lock_head.tail.exchange(I, std::memory_order_acq_rel);
        if (pred != nullptr) {
            I->locked.store(true, std::memory_order_relaxed);
            pred->next.store(I, std::memory_order_release);

            while (I->locked.load(std::memory_order_acquire)) {
                // Spin
            }
        }

        Node* succ = I->next.load(std::memory_order_acquire);
        if (succ == nullptr) {
            lock->lock_head.next.store(nullptr, std::memory_order_relaxed);
            Node* expected = I;
            if (!lock->lock_head.tail.compare_exchange_strong(expected, &lock->lock_head,
                                                              std::memory_order_acq_rel)) {
                while ((succ = I->next.load(std::memory_order_acquire)) == nullptr) {
                    // Spin
                }
                lock->lock_head.next.store(succ, std::memory_order_relaxed);
            }
        } else {
            lock->lock_head.next.store(succ, std::memory_order_relaxed);
        }

        delete I;
    }

    static void unlock_body(void* l, void* /*unused*/) {
        auto* lock = static_cast<K42Lock*>(l);
        Node* succ = lock->lock_head.next.load(std::memory_order_acquire);

        if (succ == nullptr) {
            Node* expected = &lock->lock_head;
            if (lock->lock_head.tail.compare_exchange_strong(expected, nullptr,
                                                              std::memory_order_acq_rel)) {
                return;
            }

            while ((succ = lock->lock_head.next.load(std::memory_order_acquire)) == nullptr) {
                // Spin
            }
        }

        succ->locked.store(false, std::memory_order_release);
    }

public:
    K42Lock() : lock_head(true) {}

    void Acquire(bool reentrant = false) {
        LS_Status stat = LS_ACQUIRE(this, reentrant, lock_body, nullptr);
        if (stat != LS_Status::LS_ACQUIRE_NOW && stat != LS_Status::LS_SKIP_ACQUISITION) {
            return;
        }
    }

    void Release(bool reentrant = false) {
        LS_Status stat = LS_RELEASE(this, reentrant, unlock_body, nullptr);
        if (stat != LS_Status::LS_RELEASE_NOW && stat != LS_Status::LS_SKIP_RELEASE) {
            return;
        }
    }
};

#endif // K424_H