#ifndef MCS1_H
#define MCS1_H

#include <atomic>

struct Node {
    std::atomic<Node*> next{nullptr};
    std::atomic<bool> locked{true};
};

thread_local Node myNode;

class MCSLock {
private:
    std::atomic<Node*> tail{nullptr};
public:
    void Acquire() {
        myNode.next.store(nullptr);
        Node* prev = tail.exchange(&myNode);
        if (prev != nullptr) {
            prev->next.store(&myNode);
            while (myNode.locked.load(std::memory_order_acquire));
        }
    }

    void Release() {
        if (!myNode.next.load()) {
            Node* expected = &myNode;
            if (tail.compare_exchange_strong(expected, nullptr)) {
                return;
            }
            while (myNode.next.load() == nullptr);
        }
        myNode.next.load()->locked.store(false, std::memory_order_release);
    }
};

#endif // MCS1_H
