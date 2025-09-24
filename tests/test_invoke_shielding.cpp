#include "../tests/test_framework.h"
#include "../inc/shielding_invoke.h"
#include "../src/tas.h"
#include <thread>
#include <vector>
#include <functional>

// Mock lock for testing invoke shielding
class MockInvokeLock {
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
    
    // Parameterized acquire for testing template dispatch
    void Acquire(int priority) {
        acquire_count += priority;
        is_locked = true;
    }
    
    void Release(int priority) {
        release_count += priority;
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

void test_invoke_basic_functionality() {
    TEST_SUITE("LS_INVOKE Basic Functionality");
    
    MockInvokeLock lock;
    
    // Test 1: Member function pointer dispatch
    LS_Status status = LS_ACQUIRE<MockInvokeLock, void(MockInvokeLock::*)()>(
        &lock, false, &MockInvokeLock::Acquire);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(1, lock.get_acquire_count());
    
    // Test 2: Release with member function pointer
    status = LS_RELEASE<MockInvokeLock, void(MockInvokeLock::*)()>(
        &lock, false, &MockInvokeLock::Release);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    ASSERT_EQ(1, lock.get_release_count());
}

void test_invoke_with_parameters() {
    TEST_SUITE("LS_INVOKE With Parameters");
    
    MockInvokeLock lock;
    
    // Test member function with parameters
    LS_Status status = LS_ACQUIRE<MockInvokeLock, void(MockInvokeLock::*)(int)>(
        &lock, false, &MockInvokeLock::Acquire, 5);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(5, lock.get_acquire_count()); // Should be 5 due to priority parameter
    
    status = LS_RELEASE<MockInvokeLock, void(MockInvokeLock::*)(int)>(
        &lock, false, &MockInvokeLock::Release, 3);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    ASSERT_EQ(3, lock.get_release_count()); // Should be 3 due to priority parameter
}

void test_invoke_reentrancy() {
    TEST_SUITE("LS_INVOKE Reentrancy");
    
    MockInvokeLock lock;
    
    // Test 1: First acquisition
    LS_Status status = LS_ACQUIRE<MockInvokeLock, void(MockInvokeLock::*)()>(
        &lock, true, &MockInvokeLock::Acquire);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(1, lock.get_acquire_count());
    
    // Test 2: Reentrant acquisition should skip
    status = LS_ACQUIRE<MockInvokeLock, void(MockInvokeLock::*)()>(
        &lock, true, &MockInvokeLock::Acquire);
    ASSERT_TRUE(status == LS_Status::LS_SKIP_ACQUISITION);
    ASSERT_EQ(1, lock.get_acquire_count()); // Should not increment
    
    // Test 3: First release should skip (ref count > 0)
    status = LS_RELEASE<MockInvokeLock, void(MockInvokeLock::*)()>(
        &lock, true, &MockInvokeLock::Release);
    ASSERT_TRUE(status == LS_Status::LS_SKIP_RELEASE);
    ASSERT_EQ(0, lock.get_release_count()); // Should not increment
    
    // Test 4: Second release should call actual unlock
    status = LS_RELEASE<MockInvokeLock, void(MockInvokeLock::*)()>(
        &lock, true, &MockInvokeLock::Release);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    ASSERT_EQ(1, lock.get_release_count());
}

void test_invoke_lambda_functions() {
    TEST_SUITE("LS_INVOKE Lambda Functions");
    
    MockInvokeLock lock;
    
    // Test with lambda functions
    auto acquire_lambda = [](MockInvokeLock* l) { l->Acquire(); };
    auto release_lambda = [](MockInvokeLock* l) { l->Release(); };
    
    // Note: For lambdas, we use function pointers or std::function
    LS_Status status = LS_ACQUIRE(&lock, false, acquire_lambda);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(1, lock.get_acquire_count());
    
    status = LS_RELEASE(&lock, false, release_lambda);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    ASSERT_EQ(1, lock.get_release_count());
}

void test_invoke_std_function() {
    TEST_SUITE("LS_INVOKE std::function");
    
    MockInvokeLock lock;
    
    // Test with std::function wrapper
    std::function<void(MockInvokeLock*)> acquire_fn = 
        [](MockInvokeLock* l) { l->Acquire(); };
    std::function<void(MockInvokeLock*)> release_fn = 
        [](MockInvokeLock* l) { l->Release(); };
    
    LS_Status status = LS_ACQUIRE(&lock, false, acquire_fn);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(1, lock.get_acquire_count());
    
    status = LS_RELEASE(&lock, false, release_fn);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    ASSERT_EQ(1, lock.get_release_count());
}

void test_invoke_thread_safety() {
    TEST_SUITE("LS_INVOKE Thread Safety");
    
    MockInvokeLock shared_lock;
    const int NUM_THREADS = 4;
    const int ITERATIONS = 100;
    
    std::vector<std::thread> threads;
    
    // Launch threads using member function pointers
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&shared_lock, ITERATIONS]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                LS_ACQUIRE<MockInvokeLock, void(MockInvokeLock::*)()>(
                    &shared_lock, true, &MockInvokeLock::Acquire);
                
                // Brief work simulation
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                
                LS_RELEASE<MockInvokeLock, void(MockInvokeLock::*)()>(
                    &shared_lock, true, &MockInvokeLock::Release);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify lock state is consistent
    ASSERT_TRUE(shared_lock.get_acquire_count() > 0);
    ASSERT_TRUE(shared_lock.get_release_count() > 0);
    std::cout << "Thread safety test completed with " 
              << shared_lock.get_acquire_count() << " acquisitions, "
              << shared_lock.get_release_count() << " releases\n";
}

void test_invoke_template_safety() {
    TEST_SUITE("LS_INVOKE Template Safety");
    
    MockInvokeLock lock;
    
    // Test different member function signatures
    
    // 1. No parameters
    LS_Status status = LS_ACQUIRE<MockInvokeLock, void(MockInvokeLock::*)()>(
        &lock, false, &MockInvokeLock::Acquire);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    
    LS_RELEASE<MockInvokeLock, void(MockInvokeLock::*)()>(
        &lock, false, &MockInvokeLock::Release);
    
    // 2. With int parameter
    lock.reset();
    status = LS_ACQUIRE<MockInvokeLock, void(MockInvokeLock::*)(int)>(
        &lock, false, &MockInvokeLock::Acquire, 7);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    ASSERT_EQ(7, lock.get_acquire_count());
    
    LS_RELEASE<MockInvokeLock, void(MockInvokeLock::*)(int)>(
        &lock, false, &MockInvokeLock::Release, 7);
    ASSERT_EQ(7, lock.get_release_count());
    
    std::cout << "Template safety tests completed\n";
}

void test_invoke_with_real_lock() {
    TEST_SUITE("LS_INVOKE with Real TAS Lock");
    
    TASLock real_lock;
    
    // Test basic acquire/release with real lock using member function pointers
    LS_Status status = LS_ACQUIRE<TASLock, void(TASLock::*)()>(
        &real_lock, false, &TASLock::Acquire);
    ASSERT_TRUE(status == LS_Status::LS_ACQUIRE_NOW);
    
    status = LS_RELEASE<TASLock, void(TASLock::*)()>(
        &real_lock, false, &TASLock::Release);
    ASSERT_TRUE(status == LS_Status::LS_RELEASE_NOW);
    
    // Test reentrancy with real lock
    LS_ACQUIRE<TASLock, void(TASLock::*)()>(
        &real_lock, true, &TASLock::Acquire);
    LS_ACQUIRE<TASLock, void(TASLock::*)()>(
        &real_lock, true, &TASLock::Acquire); // Reentrant
    LS_RELEASE<TASLock, void(TASLock::*)()>(
        &real_lock, true, &TASLock::Release); // Skip
    LS_RELEASE<TASLock, void(TASLock::*)()>(
        &real_lock, true, &TASLock::Release); // Actual
    
    std::cout << "Real TAS lock test completed successfully\n";
}

int main() {
    TestFramework::reset();
    
    test_invoke_basic_functionality();
    test_invoke_with_parameters();
    test_invoke_reentrancy();
    test_invoke_lambda_functions();
    test_invoke_std_function();
    test_invoke_thread_safety();
    test_invoke_template_safety();
    test_invoke_with_real_lock();
    
    return GET_TEST_RESULTS();
}