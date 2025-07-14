#ifndef TAS1_H
#define TAS1_H
#include <atomic>

// int num_threads;

class TASLock {
private:
    std::atomic_flag locked = ATOMIC_FLAG_INIT;  // Atomic flag initialized to false
public:
    void Acquire() {
        while (locked.test_and_set(std::memory_order_acquire)) {
            // Spin until lock is available
        }
    }

    void Release() {
        locked.clear(std::memory_order_release);  // Release lock
    }
};


#endif // TAS1_H
