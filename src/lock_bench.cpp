#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <pthread.h>
#include "lock_type.h"  // Choose lock via -DLOCK_DEF macro

#define CPU_RANGE1_START 64
#define CPU_RANGE1_END 127
#define NUM_ITERATION 100000000
#define WARMUP_ITERATIONS 10000

LockType lock;
pthread_barrier_t my_barrier;
int num_threads; 
long int iterations_per_thread;

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
    for (long int i = 0; i < WARMUP_ITERATIONS; i++) {
        lock.Acquire();
        lock.Release();
    }
}

void test_thread(int id) {
    // pin_thread_to_cpu(id);    
    warmup_thread(id);
    pthread_barrier_wait(&my_barrier);

    // Each thread performs its assigned portion of iterations
    long int start_iter = id * iterations_per_thread;
    long int end_iter = (id == num_threads - 1) ? NUM_ITERATION : (id + 1) * iterations_per_thread;

    auto start = std::chrono::high_resolution_clock::now();
    
    for (long int i = start_iter; i < end_iter; i++) {
        lock.Acquire();
        do_work(1000);
        lock.Release();
    }
    
    pthread_barrier_wait(&my_barrier);
    auto end = std::chrono::high_resolution_clock::now();
    
    if (id == 0) {
        std::chrono::duration<double> total_time = end - start;
        double throughput = static_cast<double>(NUM_ITERATION) / total_time.count();
        std::cout << num_threads << "," << NUM_ITERATION << "," << total_time.count() << "," << throughput << "\n";        
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: ./<exe> <num_threads>\n";
        return 1;
    }
    
    num_threads = std::stoi(argv[1]);
    iterations_per_thread = NUM_ITERATION / num_threads;
    
    pthread_barrier_init(&my_barrier, NULL, num_threads);
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(test_thread, i);
    }
    
    for (auto& t : threads) t.join();
    pthread_barrier_destroy(&my_barrier);    
    return 0;
}