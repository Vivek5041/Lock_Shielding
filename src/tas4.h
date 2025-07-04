#ifndef TAS4_H
#define TAS4_H

#include <atomic>
#include "shielding_hash.h"
class TASLock {
private:
    std::atomic_flag locked = ATOMIC_FLAG_INIT;  // Atomic flag

    static void tas_lock(void* l) {
        TASLock* lock = static_cast<TASLock*>(l);
        while (lock->locked.test_and_set(std::memory_order_acquire)) {
            // spin
        }
    }

    static void tas_unlock(void* l) {
        TASLock* lock = static_cast<TASLock*>(l);
        lock->locked.clear(std::memory_order_release);
    }

public:
    void Acquire(bool reentrant = false) {
        LS_Status stat = LS_ACQUIRE(this, reentrant, tas_lock);
        if (stat != LS_Status::LS_ACQUIRE_NOW && stat != LS_Status::LS_SKIP_ACQUISITION) {
            return;
        }
    }

    void Release(bool reentrant = false) {
        LS_Status stat = LS_RELEASE(this, reentrant, tas_unlock);
        if (stat != LS_Status::LS_RELEASE_NOW && stat != LS_Status::LS_SKIP_RELEASE) {
            return;
        }
    }
};


#endif // TAS4_H