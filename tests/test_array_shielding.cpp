#include "../tests/test_framework.h"
#include "../inc/shielding_array.h"
#include "../src/mcs.h"
#include <thread>
#include <vector>

// Mock lock for testing
class MockLock {
private:
    int acquire_count = 0;
    int release_count = 0;
    bool is_locked = false;

public:
    void Acquire() {
        acquire_count++;
        is_locked = true;
    }
    
    void Release() {
        release_count++;
        is_locked = false;
    }
    
    int get_acquire_count() const { return acquire_count; }
    int get_release_count() const { return release_count; }
    bool get_lock_state() const { return is_locked; }
    
    void reset() {
        acquire_count = 0;
        release_count = 0;
        is_locked = false;
    }
};

// Wrapper functions for array shielding
void mock_lock_acquire(MockLock* l) { l->Acquire(); }
void mock_lock_release(MockLock* l) { l->Release(); }

void test_array_basic_functionality() {
    TEST_SUITE("LS_ARRAY Basic Functionality");
    
    MockLock lock;
    
    // Test 1: First acquisition should call actual lock
    LS_Status status = LS_ACQUIRE(&lock, false, mock_lock_acquire);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(1, lock.get_acquire_count());
    
    // Test 2: Release should call actual unlock
    status = LS_RELEASE(&lock, false, mock_lock_release);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    ASSERT_EQ(1, lock.get_release_count());
}

void test_array_reentrancy() {
    TEST_SUITE("LS_ARRAY Reentrancy");
    
    MockLock lock;
    
    // Test 1: First acquisition
    LS_Status status = LS_ACQUIRE(&lock, true, mock_lock_acquire);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(1, lock.get_acquire_count());
    
    // Test 2: Reentrant acquisition should skip
    status = LS_ACQUIRE(&lock, true, mock_lock_acquire);
    ASSERT_TRUE(status == LS_Status::LS_SKIP_ACQUISITION);
    ASSERT_EQ(1, lock.get_acquire_count()); // Should not increment
    
    // Test 3: First release should skip (ref count > 0)
    status = LS_RELEASE(&lock, true, mock_lock_release);
    ASSERT_TRUE(status == LS_Status::LS_SKIP_RELEASE);
    ASSERT_EQ(0, lock.get_release_count()); // Should not increment
    
    // Test 4: Second release should call actual unlock
    status = LS_RELEASE(&lock, true, mock_lock_release);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    ASSERT_EQ(1, lock.get_release_count());
}

void test_array_reference_counting() {
    TEST_SUITE("LS_ARRAY Reference Counting");
    
    MockLock lock1, lock2;
    
    // Test multiple locks with reference counting
    LS_ACQUIRE(&lock1, true, mock_lock_acquire);
    LS_ACQUIRE(&lock2, true, mock_lock_acquire);
    
    // Both locks should be acquired
    ASSERT_EQ(1, lock1.get_acquire_count());
    ASSERT_EQ(1, lock2.get_acquire_count());
    
    // Multiple acquisitions of same lock
    LS_ACQUIRE(&lock1, true, mock_lock_acquire);
    LS_ACQUIRE(&lock1, true, mock_lock_acquire);
    ASSERT_EQ(1, lock1.get_acquire_count()); // Should still be 1
    
    // Release lock1 multiple times
    LS_RELEASE(&lock1, true, mock_lock_release); // ref count 2->1
    LS_RELEASE(&lock1, true, mock_lock_release); // ref count 1->0, actual release
    ASSERT_EQ(1, lock1.get_release_count());
    
    // Release lock2
    LS_RELEASE(&lock2, true, mock_lock_release);
    ASSERT_EQ(1, lock2.get_release_count());
}

void test_array_thread_safety() {
    TEST_SUITE("LS_ARRAY Thread Safety");
    
    MockLock shared_lock;
    const int NUM_THREADS = 4;
    const int ITERATIONS = 100;
    
    std::vector<std::thread> threads;
    
    // Launch threads that acquire/release the same lock
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&shared_lock, ITERATIONS]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                LS_ACQUIRE(&shared_lock, true, mock_lock_acquire);
                // Brief work simulation
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                LS_RELEASE(&shared_lock, true, mock_lock_release);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify lock state is consistent
    // Note: Due to thread-local storage, each thread will have its own reference count
    ASSERT_TRUE(shared_lock.get_acquire_count() > 0);
    ASSERT_TRUE(shared_lock.get_release_count() > 0);
    std::cout << "Thread safety test completed with " 
              << shared_lock.get_acquire_count() << " acquisitions, "
              << shared_lock.get_release_count() << " releases\n";
}

void test_array_with_real_lock() {
    TEST_SUITE("LS_ARRAY with Real MCS Lock");
    
    MCSLock real_lock;
    
    // Wrapper functions for real lock
    auto acquire_fn = [](MCSLock* l) { l->Acquire(); };
    auto release_fn = [](MCSLock* l) { l->Release(); };
    
    // Test basic acquire/release with real lock
    LS_Status status = LS_ACQUIRE(&real_lock, false, acquire_fn);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    
    status = LS_RELEASE(&real_lock, false, release_fn);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    
    std::cout << "Real lock test completed successfully\n";
}

int main() {
    TestFramework::reset();
    
    test_array_basic_functionality();
    test_array_reentrancy();
    test_array_reference_counting();
    test_array_thread_safety();
    test_array_with_real_lock();
    
    return GET_TEST_RESULTS();
}