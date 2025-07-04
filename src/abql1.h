#ifndef ABQL1_H
#define ABQL1_H

#include <atomic>

constexpr int ABQL_MAX_THREADS = 256;

class ABQLLock {
private:
    std::atomic<bool> flags[ABQL_MAX_THREADS];
    std::atomic<int> tail;

    // Each thread gets its own slot index
    thread_local static int slot;

public:
    ABQLLock() {
        for (int i = 0; i < ABQL_MAX_THREADS; ++i) {
            flags[i].store(false, std::memory_order_relaxed);
        }
        flags[0].store(true, std::memory_order_relaxed); // First thread can acquire
        tail.store(0, std::memory_order_relaxed);
    }

    void Acquire() {
        int mySlot = tail.fetch_add(1, std::memory_order_relaxed) % ABQL_MAX_THREADS;
        slot = mySlot;

        while (!flags[mySlot].load(std::memory_order_acquire)) {
            // Spin until it's my turn
        }
    }

    void Release() {
        flags[slot].store(false, std::memory_order_relaxed);                // Clear my flag
        flags[(slot + 1) % ABQL_MAX_THREADS].store(true, std::memory_order_release); // Wake next
    }
};

// Define the thread_local slot variable
thread_local int ABQLLock::slot = 0;

#endif // ABQL1_H

