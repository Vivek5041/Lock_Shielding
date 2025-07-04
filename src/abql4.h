#ifndef ABQL4_H
#define ABQL4_H

#include <atomic>
#include <thread>
#include "shielding_hash.h"

constexpr int ABQL_MAX_THREADS = 256;

class ABQLLock {
private:
    std::atomic<bool> flags[ABQL_MAX_THREADS];
    std::atomic<int> tail;

    static thread_local int slot;

    static void lock_body(void* l, void* dummy) {
        auto* lock = static_cast<ABQLLock*>(l);

        int mySlot = lock->tail.fetch_add(1, std::memory_order_relaxed) % ABQL_MAX_THREADS;
        slot = mySlot;

        // Wait for my turn
        while (!lock->flags[mySlot].load(std::memory_order_acquire)) {
            // spin
        }
    }

    static void unlock_body(void* l, void* dummy) {
        auto* lock = static_cast<ABQLLock*>(l);
        int next = (slot + 1) % ABQL_MAX_THREADS;
        lock->flags[slot].store(false, std::memory_order_relaxed);  // Clear my flag
        lock->flags[next].store(true, std::memory_order_release);   // Enable next
    }

public:
    ABQLLock() {
        for (int i = 0; i < ABQL_MAX_THREADS; ++i) {
            flags[i].store(false, std::memory_order_relaxed);
        }
        flags[0].store(true, std::memory_order_relaxed); // First slot is free
        tail.store(0, std::memory_order_relaxed);
    }

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

// Thread-local slot ID
thread_local int ABQLLock::slot = 0;

#endif // ABQL4_H

