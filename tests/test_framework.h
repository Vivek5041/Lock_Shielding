#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <iostream>
#include <string>
#include <cstdlib>

// Simple test framework for Shield unit tests
class TestFramework {
private:
    static int total_tests;
    static int passed_tests;
    static std::string current_test_suite;

public:
    static void start_suite(const std::string& suite_name) {
        current_test_suite = suite_name;
        std::cout << "\n=== Testing " << suite_name << " ===\n";
    }

    static void assert_true(bool condition, const std::string& test_name) {
        total_tests++;
        if (condition) {
            passed_tests++;
            std::cout << "✓ " << test_name << "\n";
        } else {
            std::cout << "✗ " << test_name << " FAILED\n";
        }
    }

    static void assert_equals(long expected, long actual, const std::string& test_name) {
        total_tests++;
        if (expected == actual) {
            passed_tests++;
            std::cout << "✓ " << test_name << "\n";
        } else {
            std::cout << "✗ " << test_name << " FAILED (expected: " << expected 
                      << ", actual: " << actual << ")\n";
        }
    }

    static void assert_not_null(void* ptr, const std::string& test_name) {
        assert_true(ptr != nullptr, test_name);
    }

    static void assert_null(void* ptr, const std::string& test_name) {
        assert_true(ptr == nullptr, test_name);
    }

    static int get_results() {
        std::cout << "\n=== Test Results ===\n";
        std::cout << "Passed: " << passed_tests << "/" << total_tests << "\n";
        
        if (passed_tests == total_tests) {
            std::cout << "All tests PASSED! ✓\n";
            return 0;
        } else {
            std::cout << "Some tests FAILED! ✗\n";
            return 1;
        }
    }

    static void reset() {
        total_tests = 0;
        passed_tests = 0;
        current_test_suite = "";
    }
};

// Static member definitions
int TestFramework::total_tests = 0;
int TestFramework::passed_tests = 0;
std::string TestFramework::current_test_suite = "";

// Convenience macros
#define TEST_SUITE(name) TestFramework::start_suite(name)
#define ASSERT_TRUE(condition) TestFramework::assert_true((condition), #condition)
#define ASSERT_FALSE(condition) TestFramework::assert_true(!(condition), "NOT(" #condition ")")
#define ASSERT_EQ(expected, actual) TestFramework::assert_equals((expected), (actual), #expected " == " #actual)
#define ASSERT_NOT_NULL(ptr) TestFramework::assert_not_null((ptr), #ptr " != NULL")
#define ASSERT_NULL(ptr) TestFramework::assert_null((ptr), #ptr " == NULL")
#define GET_TEST_RESULTS() TestFramework::get_results()

#endif // TEST_FRAMEWORK_H