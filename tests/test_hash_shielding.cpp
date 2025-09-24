#include "../tests/test_framework.h"
#include "../inc/shielding_hash.h"
#include "../src/clh.h"
#include <thread>
#include <vector>
#include <unordered_set>

// Mock lock for testing hash shielding
class MockHashLock {
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

// Wrapper functions for hash shielding
void mock_hash_acquire(MockHashLock* l) { l->Acquire(); }
void mock_hash_release(MockHashLock* l) { l->Release(); }

void test_hash_basic_functionality() {
    TEST_SUITE("LS_HASH Basic Functionality");
    
    MockHashLock lock;
    
    // Test 1: First acquisition should call actual lock
    LS_Status status = LS_ACQUIRE(&lock, false, mock_hash_acquire);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(1, lock.get_acquire_count());
    
    // Test 2: Release should call actual unlock
    status = LS_RELEASE(&lock, false, mock_hash_release);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    ASSERT_EQ(1, lock.get_release_count());
}

void test_hash_reentrancy() {
    TEST_SUITE("LS_HASH Reentrancy");
    
    MockHashLock lock;
    
    // Test 1: First acquisition
    LS_Status status = LS_ACQUIRE(&lock, true, mock_hash_acquire);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(1, lock.get_acquire_count());
    
    // Test 2: Reentrant acquisition should skip
    status = LS_ACQUIRE(&lock, true, mock_hash_acquire);
    ASSERT_TRUE(status == LS_Status::LS_SKIP_ACQUISITION);
    ASSERT_EQ(1, lock.get_acquire_count()); // Should not increment
    
    // Test 3: First release should skip (ref count > 0)
    status = LS_RELEASE(&lock, true, mock_hash_release);
    ASSERT_TRUE(status == LS_Status::LS_SKIP_RELEASE);
    ASSERT_EQ(0, lock.get_release_count()); // Should not increment
    
    // Test 4: Second release should call actual unlock
    status = LS_RELEASE(&lock, true, mock_hash_release);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    ASSERT_EQ(1, lock.get_release_count());
}

void test_hash_multiple_locks() {
    TEST_SUITE("LS_HASH Multiple Locks Scalability");
    
    const int NUM_LOCKS = 50; // Test scalability beyond MAX_LOCKS (4)
    std::vector<MockHashLock> locks(NUM_LOCKS);
    
    // Acquire all locks
    for (int i = 0; i < NUM_LOCKS; ++i) {
        LS_Status status = LS_ACQUIRE(&locks[i], true, mock_hash_acquire);
        ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
        ASSERT_EQ(1, locks[i].get_acquire_count());
    }
    
    // Test reentrant acquisitions on random locks
    std::vector<int> test_indices = {5, 15, 25, 35, 45};
    for (int idx : test_indices) {
        LS_Status status = LS_ACQUIRE(&locks[idx], true, mock_hash_acquire);
        ASSERT_TRUE(status == LS_Status::LS_SKIP_ACQUISITION);
        ASSERT_EQ(1, locks[idx].get_acquire_count()); // Should not increment
    }
    
    // Release the reentrant acquisitions
    for (int idx : test_indices) {
        LS_Status status = LS_RELEASE(&locks[idx], true, mock_hash_release);
        ASSERT_TRUE(status == LS_Status::LS_SKIP_RELEASE);
        ASSERT_EQ(0, locks[idx].get_release_count()); // Should not increment
    }
    
    // Release all locks
    for (int i = 0; i < NUM_LOCKS; ++i) {
        LS_Status status = LS_RELEASE(&locks[i], true, mock_hash_release);
        ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
        ASSERT_EQ(1, locks[i].get_release_count());
    }
    
    std::cout << "Successfully tested " << NUM_LOCKS << " locks with hash shielding\n";
}

void test_hash_pointer_uniqueness() {
    TEST_SUITE("LS_HASH Pointer Address Handling");
    
    MockHashLock lock1, lock2;
    MockHashLock* same_lock = &lock1;
    
    // Acquire lock1 twice via different pointers (same address)
    LS_ACQUIRE(&lock1, true, mock_hash_acquire);
    LS_ACQUIRE(same_lock, true, mock_hash_acquire); // Should skip
    
    ASSERT_EQ(1, lock1.get_acquire_count()); // Only one actual acquisition
    
    // Acquire lock2 (different address)
    LS_ACQUIRE(&lock2, true, mock_hash_acquire);
    ASSERT_EQ(1, lock2.get_acquire_count());
    
    // Release operations
    LS_RELEASE(same_lock, true, mock_hash_release); // First release - skip
    LS_RELEASE(&lock1, true, mock_hash_release);    // Second release - actual
    LS_RELEASE(&lock2, true, mock_hash_release);    // Release lock2
    
    ASSERT_EQ(1, lock1.get_release_count());
    ASSERT_EQ(1, lock2.get_release_count());
}

void test_hash_concurrent_access() {
    TEST_SUITE("LS_HASH Concurrent Access");
    
    const int NUM_THREADS = 8;
    const int NUM_LOCKS_PER_THREAD = 10;
    const int ITERATIONS = 50;
    
    // Create a pool of locks shared across threads
    std::vector<MockHashLock> lock_pool(NUM_LOCKS_PER_THREAD * 2);
    std::vector<std::thread> threads;
    
    // Each thread works with overlapping subsets of locks
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&lock_pool, t, ITERATIONS, NUM_LOCKS_PER_THREAD]() {
            int start_idx = t; // Overlapping ranges
            
            for (int iter = 0; iter < ITERATIONS; ++iter) {
                // Acquire multiple locks
                std::vector<int> acquired_locks;
                for (int i = 0; i < NUM_LOCKS_PER_THREAD; ++i) {
                    int lock_idx = (start_idx + i) % lock_pool.size();
                    LS_ACQUIRE(&lock_pool[lock_idx], true, mock_hash_acquire);
                    acquired_locks.push_back(lock_idx);
                }
                
                // Brief work simulation
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                
                // Release in reverse order
                for (auto it = acquired_locks.rbegin(); it != acquired_locks.rend(); ++it) {
                    LS_RELEASE(&lock_pool[*it], true, mock_hash_release);
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify all locks have consistent state
    int total_acquires = 0, total_releases = 0;
    for (const auto& lock : lock_pool) {
        total_acquires += lock.get_acquire_count();
        total_releases += lock.get_release_count();
    }
    
    std::cout << "Concurrent test completed: " << total_acquires 
              << " total acquisitions, " << total_releases << " total releases\n";
    ASSERT_TRUE(total_acquires > 0);
    ASSERT_TRUE(total_releases > 0);
}

void test_hash_with_real_lock() {
    TEST_SUITE("LS_HASH with Real CLH Lock");
    
    CLHLock real_lock;
    
    // Wrapper functions for real lock
    auto acquire_fn = [](CLHLock* l) { l->Acquire(); };
    auto release_fn = [](CLHLock* l) { l->Release(); };
    
    // Test basic acquire/release with real lock
    LS_Status status = LS_ACQUIRE(&real_lock, false, acquire_fn);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    
    status = LS_RELEASE(&real_lock, false, release_fn);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    
    // Test reentrancy with real lock
    LS_ACQUIRE(&real_lock, true, acquire_fn);
    LS_ACQUIRE(&real_lock, true, acquire_fn); // Reentrant
    LS_RELEASE(&real_lock, true, release_fn); // Skip
    LS_RELEASE(&real_lock, true, release_fn); // Actual
    
    std::cout << "Real CLH lock test completed successfully\n";
}

int main() {
    TestFramework::reset();
    
    test_hash_basic_functionality();
    test_hash_reentrancy();
    test_hash_multiple_locks();
    test_hash_pointer_uniqueness();
    test_hash_concurrent_access();
    test_hash_with_real_lock();
    
    return GET_TEST_RESULTS();
}