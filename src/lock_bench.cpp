#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <pthread.h>
#include "lock_type.h"  // Choose lock via -DLOCK_DEF macro

#define CPU_RANGE1_START 64
#define CPU_RANGE1_END 127

#define NUM_ITERATION 1000000
#define WARMUP_ITERATIONS 1000

LockType lock;

void do_work(int amount) {
    volatile int dummy = 0;
    for (int i = 0; i < amount; i++) {
        dummy += i;
    }
}

void pin_thread_to_cpu(int thread_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int range_size = CPU_RANGE1_END - CPU_RANGE1_START + 1;
    int cpu_id = CPU_RANGE1_START + (thread_id % range_size);
    CPU_SET(cpu_id, &cpuset);
    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "Error setting thread affinity for thread " << thread_id
                  << " to CPU " << cpu_id << std::endl;
    }
}

void warmup_thread(int id) {
    //pin_thread_to_cpu(id);
    for (long int i = 0; i < WARMUP_ITERATIONS; i++) {
        lock.Acquire();
        lock.Release();
    }
}

void test_thread(int id) {
    pin_thread_to_cpu(id);
    for (long int i = 0; i < NUM_ITERATION; i++) {
        lock.Acquire();
        do_work(10000);
        lock.Release();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: ./<exe> <num_threads>\n";
        return 1;
    }

    int num_threads = std::stoi(argv[1]);

    std::vector<std::thread> warmup_threads;
    for (int i = 0; i < num_threads; i++) {
        warmup_threads.emplace_back(warmup_thread, i);
    }
    for (auto& t : warmup_threads) t.join();

    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(test_thread, i);
    }
    for (auto& t : threads) t.join();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> total_time = end - start;
    double throughput = (static_cast<double>(NUM_ITERATION) * num_threads) / total_time.count();

    std::cout << num_threads << "," << NUM_ITERATION << "," << total_time.count() << "," << throughput << "\n";
    return 0;
}
