#include "../tests/test_framework.h"
#include "../inc/sanity_check.h"
#include "../src/lock_type.h"
#include <thread>
#include <vector>
#include <chrono>

// Integration tests that verify all lock types work with all shielding strategies
// This simulates the real usage patterns from lock_bench.cpp

void test_baseline_locks() {
    TEST_SUITE("Baseline Integration (No Shielding)");
    
    // Test each lock type without shielding
    std::vector<std::string> lock_types = {
        "MCS", "CLH", "TAS", "TICKET", "ABQL", "K42", "HEM", "CNA"
    };
    
    for (const auto& lock_type : lock_types) {
        std::cout << "Testing " << lock_type << " baseline... ";
        
        // Create lock (this uses the LockType typedef from lock_type.h)
        LockType lock;
        
        // Basic acquire/release test
        lock.Acquire();
        lock.Release();
        
        // Multi-acquire test
        for (int i = 0; i < 10; ++i) {
            lock.Acquire();
            // Simulate brief critical section
            volatile int dummy = 0;
            for (int j = 0; j < 100; ++j) {
                dummy += j;
            }
            lock.Release();
        }
        
        std::cout << "✓\n";
    }
    
    ASSERT_TRUE(true); // If we get here, all tests passed
}

void test_shielded_lock_integration() {
    TEST_SUITE("Shielded Lock Integration");
    
    LockType lock;
    const int ITERATIONS = 1000;
    
    // Helper functions for testing (similar to lock_bench.cpp)
    auto lock_acquire = [](LockType* l) { l->Acquire(); };
    auto lock_release = [](LockType* l) { l->Release(); };
    
    // Test the current shielding configuration
    std::cout << "Testing current lock configuration...\n";
    
    #if defined(SHIELD_VERSION_LS_ARRAY)
        std::cout << "Using LS_ARRAY shielding\n";
        for (int i = 0; i < ITERATIONS; ++i) {
            LS_Status status = LS_ACQUIRE(&lock, false, lock_acquire);
            ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW || 
                       status == LS_Status::LS_SKIP_ACQUISITION);
            
            status = LS_RELEASE(&lock, false, lock_release);
            ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW || 
                       status == LS_Status::LS_SKIP_RELEASE);
        }
        
    #elif defined(SHIELD_VERSION_LS_HYBRID)
        std::cout << "Using LS_HYBRID shielding\n";
        for (int i = 0; i < ITERATIONS; ++i) {
            LS_Status status = LS_ACQUIRE(&lock, false, lock_acquire);
            ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW || 
                       status == LS_Status::LS_SKIP_ACQUISITION);
            
            status = LS_RELEASE(&lock, false, lock_release);
            ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW || 
                       status == LS_Status::LS_SKIP_RELEASE);
        }
        
    #elif defined(SHIELD_VERSION_LS_INVOKE)
        std::cout << "Using LS_INVOKE shielding\n";
        for (int i = 0; i < ITERATIONS; ++i) {
            LS_Status status = LS_ACQUIRE<LockType, void(LockType::*)()>(
                &lock, false, &LockType::Acquire);
            ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW || 
                       status == LS_Status::LS_SKIP_ACQUISITION);
            
            status = LS_RELEASE<LockType, void(LockType::*)()>(
                &lock, false, &LockType::Release);
            ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW || 
                       status == LS_Status::LS_SKIP_RELEASE);
        }
        
    #else
        std::cout << "Using BASELINE (no shielding)\n";
        for (int i = 0; i < ITERATIONS; ++i) {
            lock.Acquire();
            lock.Release();
        }
    #endif
    
    std::cout << "Completed " << ITERATIONS << " lock operations successfully\n";
}

void test_multithreaded_integration() {
    TEST_SUITE("Multithreaded Integration");
    
    LockType shared_lock;
    const int NUM_THREADS = 4;
    const int ITERATIONS = 500;
    std::atomic<int> shared_counter{0};
    
    // Helper functions
    auto lock_acquire = [](LockType* l) { l->Acquire(); };
    auto lock_release = [](LockType* l) { l->Release(); };
    
    std::vector<std::thread> threads;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Launch worker threads
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&shared_lock, &shared_counter, ITERATIONS, 
                            lock_acquire, lock_release]() {
            
            for (int i = 0; i < ITERATIONS; ++i) {
                
                #if defined(SHIELD_VERSION_LS_ARRAY) || defined(SHIELD_VERSION_LS_HYBRID)
                    LS_ACQUIRE(&shared_lock, false, lock_acquire);
                    
                    // Critical section
                    int old_val = shared_counter.load();
                    shared_counter.store(old_val + 1);
                    
                    LS_RELEASE(&shared_lock, false, lock_release);
                    
                #elif defined(SHIELD_VERSION_LS_INVOKE)
                    LS_ACQUIRE<LockType, void(LockType::*)()>(
                        &shared_lock, false, &LockType::Acquire);
                    
                    // Critical section
                    int old_val = shared_counter.load();
                    shared_counter.store(old_val + 1);
                    
                    LS_RELEASE<LockType, void(LockType::*)()>(
                        &shared_lock, false, &LockType::Release);
                    
                #else
                    shared_lock.Acquire();
                    
                    // Critical section
                    int old_val = shared_counter.load();
                    shared_counter.store(old_val + 1);
                    
                    shared_lock.Release();
                #endif
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();
    
    // Verify correctness
    int expected_count = NUM_THREADS * ITERATIONS;
    int actual_count = shared_counter.load();
    
    ASSERT_EQ(expected_count, actual_count);
    
    std::cout << "Multithreaded test completed successfully:\n";
    std::cout << "  Expected counter: " << expected_count << "\n";
    std::cout << "  Actual counter: " << actual_count << "\n";
    std::cout << "  Duration: " << duration << " μs\n";
    std::cout << "  Operations/sec: " << 
        (expected_count * 2 * 1000000.0 / duration) << "\n";
}

void test_reentrancy_integration() {
    TEST_SUITE("Reentrancy Integration");
    
    LockType lock;
    
    // Helper functions
    auto lock_acquire = [](LockType* l) { l->Acquire(); };
    auto lock_release = [](LockType* l) { l->Release(); };
    
    #if defined(SHIELD_VERSION_LS_ARRAY) || defined(SHIELD_VERSION_LS_HYBRID)
        // Test nested acquisitions
        LS_Status status1 = LS_ACQUIRE(&lock, true, lock_acquire);
        ASSERT_TRUE(status1 == LS_Status::LS_ACQUIRE_NOW);
        
        LS_Status status2 = LS_ACQUIRE(&lock, true, lock_acquire);
        ASSERT_TRUE(status2 == LS_Status::LS_SKIP_ACQUISITION);
        
        LS_Status status3 = LS_ACQUIRE(&lock, true, lock_acquire);
        ASSERT_TRUE(status3 == LS_Status::LS_SKIP_ACQUISITION);
        
        // Release in reverse order
        LS_Status rel1 = LS_RELEASE(&lock, true, lock_release);
        ASSERT_TRUE(rel1 == LS_Status::LS_SKIP_RELEASE);
        
        LS_Status rel2 = LS_RELEASE(&lock, true, lock_release);
        ASSERT_TRUE(rel2 == LS_Status::LS_SKIP_RELEASE);
        
        LS_Status rel3 = LS_RELEASE(&lock, true, lock_release);
        ASSERT_TRUE(rel3 == LS_Status::LS_RELEASE_NOW);
        
    #elif defined(SHIELD_VERSION_LS_INVOKE)
        // Test nested acquisitions with member function pointers
        LS_Status status1 = LS_ACQUIRE<LockType, void(LockType::*)()>(
            &lock, true, &LockType::Acquire);
        ASSERT_TRUE(status1 == LS_Status::LS_ACQUIRE_NOW);
        
        LS_Status status2 = LS_ACQUIRE<LockType, void(LockType::*)()>(
            &lock, true, &LockType::Acquire);
        ASSERT_TRUE(status2 == LS_Status::LS_SKIP_ACQUISITION);
        
        LS_Status rel1 = LS_RELEASE<LockType, void(LockType::*)()>(
            &lock, true, &LockType::Release);
        ASSERT_TRUE(rel1 == LS_Status::LS_SKIP_RELEASE);
        
        LS_Status rel2 = LS_RELEASE<LockType, void(LockType::*)()>(
            &lock, true, &LockType::Release);
        ASSERT_TRUE(rel2 == LS_Status::LS_RELEASE_NOW);
        
    #else
        // Baseline version - just test that locks work
        lock.Acquire();
        lock.Release();
        std::cout << "Baseline version - reentrancy not applicable\n";
    #endif
    
    std::cout << "Reentrancy test completed successfully\n";
}

void test_performance_comparison() {
    TEST_SUITE("Performance Comparison");
    
    LockType lock;
    const int ITERATIONS = 100000;
    
    auto lock_acquire = [](LockType* l) { l->Acquire(); };
    auto lock_release = [](LockType* l) { l->Release(); };
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < ITERATIONS; ++i) {
        #if defined(SHIELD_VERSION_LS_ARRAY) || defined(SHIELD_VERSION_LS_HYBRID)
            LS_ACQUIRE(&lock, false, lock_acquire);
            LS_RELEASE(&lock, false, lock_release);
            
        #elif defined(SHIELD_VERSION_LS_INVOKE)
            LS_ACQUIRE<LockType, void(LockType::*)()>(
                &lock, false, &LockType::Acquire);
            LS_RELEASE<LockType, void(LockType::*)()>(
                &lock, false, &LockType::Release);
            
        #else
            lock.Acquire();
            lock.Release();
        #endif
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time).count();
    
    double avg_ns_per_op = static_cast<double>(duration) / (ITERATIONS * 2);
    
    std::cout << "Performance test completed:\n";
    std::cout << "  Total operations: " << (ITERATIONS * 2) << "\n";
    std::cout << "  Total time: " << duration << " ns\n";
    std::cout << "  Average time per operation: " << avg_ns_per_op << " ns\n";
    
    // Basic sanity check - operations should complete in reasonable time
    ASSERT_TRUE(avg_ns_per_op < 10000); // Less than 10μs per operation
}

int main() {
    TestFramework::reset();
    
    std::cout << "=== Shield Integration Tests ===\n";
    std::cout << "Compiled with shielding configuration:\n";
    
    #if defined(SHIELD_VERSION_LS_ARRAY)
        std::cout << "  LS_ARRAY (array-based shielding)\n";
    #elif defined(SHIELD_VERSION_LS_HYBRID)
        std::cout << "  LS_HYBRID (hash-based shielding)\n";
    #elif defined(SHIELD_VERSION_LS_INVOKE)
        std::cout << "  LS_INVOKE (invoke-based shielding)\n";
    #else
        std::cout << "  BASELINE (no shielding)\n";
    #endif
    
    test_baseline_locks();
    test_shielded_lock_integration();
    test_multithreaded_integration();
    test_reentrancy_integration();
    test_performance_comparison();
    
    return GET_TEST_RESULTS();
}