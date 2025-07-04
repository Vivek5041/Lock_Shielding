#ifndef CLH1_H
#define CLH1_H

#include <atomic>

struct QNode {
    std::atomic<bool> locked{false};
};

thread_local QNode* myNode = new QNode();
thread_local QNode* myPred = nullptr;

class CLHLock {
    std::atomic<QNode*> tail{new QNode()};
public:
    void Acquire() {
        myNode->locked.store(true);
        myPred = tail.exchange(myNode);
        while (myPred->locked.load());
    }
    void Release() {
        myNode->locked.store(false);
        std::swap(myNode, myPred);
    }
};

#endif