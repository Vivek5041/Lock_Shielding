#ifndef K421_H
#define K421_H
#include <atomic>

// MCS-K42 Lock implementation
class K42Lock {
private:
    // Lock node structure - used for both queue nodes and the lock itself
    struct Node {
        std::atomic<Node*> next;
        union {
            std::atomic<bool> locked;    // for queue nodes
            std::atomic<Node*> tail;    // for locks
        };
        
        // Constructor for queue nodes
        Node() : next(nullptr), locked(false) {}
        
        // Constructor for lock head
        explicit Node(bool is_lock_head) : next(nullptr) {
            if (is_lock_head) {
                tail.store(nullptr, std::memory_order_relaxed);
            } else {
                locked.store(false, std::memory_order_relaxed);
            }
        }
    };
    
    Node lock_head;  // The lock structure itself
    
public:
    K42Lock() : lock_head(true) {}
    
    void Acquire() {
        // Create a new queue node for this thread
        Node* I = new Node();
        I->next.store(nullptr, std::memory_order_relaxed);
        
        // Atomically add ourselves to the tail of the queue
        Node* predecessor = lock_head.tail.exchange(I, std::memory_order_acq_rel);
        
        if (predecessor != nullptr) {
            // Queue was non-empty, so we need to wait
            I->locked.store(true, std::memory_order_relaxed);
            predecessor->next.store(I, std::memory_order_release);
            
            // Spin wait until we get the lock
            while (I->locked.load(std::memory_order_acquire)) {
                // Busy wait
            }
        }
        
        // We now have the lock
        // Check if there's a successor
        Node* successor = I->next.load(std::memory_order_acquire);
        if (successor == nullptr) {
            // No immediate successor, try to set lock head to point to us
            lock_head.next.store(nullptr, std::memory_order_relaxed);
            
            // Try to atomically update tail from I to &lock_head.next
            Node* expected = I;
            if (!lock_head.tail.compare_exchange_strong(expected, &lock_head, 
                                                       std::memory_order_acq_rel)) {
                // Someone got into the timing window, wait for successor
                while ((successor = I->next.load(std::memory_order_acquire)) == nullptr) {
                    // Busy wait for successor to appear
                }
                lock_head.next.store(successor, std::memory_order_relaxed);
            }
        } else {
            // There is a successor
            lock_head.next.store(successor, std::memory_order_relaxed);
        }
        
        // Clean up our queue node
        delete I;
    }
    
    void Release() {
        Node* successor = lock_head.next.load(std::memory_order_acquire);
        
        if (successor == nullptr) {
            // No known successor
            Node* expected = &lock_head;
            if (lock_head.tail.compare_exchange_strong(expected, nullptr, 
                                                      std::memory_order_acq_rel)) {
                // Successfully released with no waiting threads
                return;
            }
            
            // Someone is in the process of queuing, wait for them
            while ((successor = lock_head.next.load(std::memory_order_acquire)) == nullptr) {
                // Busy wait for successor
            }
        }
        
        // Wake up the successor
        successor->locked.store(false, std::memory_order_release);
    }
};    
#endif // K421_H