#ifndef TICKET1_H
#define TICKET1_H
#include <atomic>

class TicketLock {
private:
    std::atomic<int> next_ticket{0};
    std::atomic<int> now_serving{0};

public:
    void Acquire() {
        int my_ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
        while (now_serving.load(std::memory_order_acquire) != my_ticket) {
            // spin
        }
    }

    void Release() {
        now_serving.fetch_add(1, std::memory_order_release);
    }
};

#endif // TICKET1_H