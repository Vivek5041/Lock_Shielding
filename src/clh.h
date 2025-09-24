#ifndef CLH1_H
#define CLH1_H

#include <atomic>

struct QNode {
    std::atomic<bool> locked{false};
};

thread_local QNode* myNode = new QNode();
thread_local QNode* myPred = nullptr;

class CLHLock {
private:
    std::atomic<QNode*> tail{new QNode()};

public:
    void Acquire() {
        myNode->locked.store(true, std::memory_order_relaxed);
        myPred = tail.exchange(myNode, std::memory_order_acq_rel);
        while (myPred->locked.load(std::memory_order_acquire));
    }
    
    void Release() {
        myNode->locked.store(false, std::memory_order_release);
        std::swap(myNode, myPred);
    }
};

#endif