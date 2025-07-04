#ifndef TICKET3_H
#define TICKET3_H
#include <atomic>
#include "shielding_array.h"

class TicketLock {
private:
    std::atomic<int> ticket{0};
    std::atomic<int> serving{0};

    static void lock_body(void* l) {
        auto* lock = static_cast<TicketLock*>(l);
        int my_ticket = lock->ticket.fetch_add(1, std::memory_order_relaxed);
        while (lock->serving.load(std::memory_order_acquire) != my_ticket) {
            // spin
        }
    }

    static void unlock_body(void* l) {
        auto* lock = static_cast<TicketLock*>(l);
        lock->serving.fetch_add(1, std::memory_order_release);
    }

public:
    void Acquire() {
        if (LS_ACQUIRE(this, false, lock_body) != LS_Status::LS_ACQUIRE_NOW) {
            return;
        }
    }

    void Release() {
        if (LS_RELEASE(this, false, unlock_body) != LS_Status::LS_RELEASE_NOW) {
            return;
        }
    }
};

#endif // TICKET3_H