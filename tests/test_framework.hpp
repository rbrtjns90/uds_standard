#ifndef TEST_FRAMEWORK_HPP
#define TEST_FRAMEWORK_HPP

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace test {

// ANSI color codes for terminal output
namespace colors {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* RED = "\033[31m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* BOLD = "\033[1m";
}

// Test result statistics
struct TestStats {
    int total = 0;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    std::chrono::milliseconds total_time{0};
};

// Test case result
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    std::chrono::milliseconds duration{0};
};

// Mock CAN transport for testing
class MockTransport {
public:
    struct ExpectedTransaction {
        std::vector<uint8_t> request;
        std::vector<uint8_t> response;
        std::chrono::milliseconds delay{0};
    };

    void expect_exchange(const std::vector<uint8_t>& request, 
                         const std::vector<uint8_t>& response,
                         std::chrono::milliseconds delay = std::chrono::milliseconds{0}) {
        expectations_.push_back({request, response, delay});
    }

    bool send(const std::vector<uint8_t>& data, [[maybe_unused]] std::chrono::milliseconds timeout) {
        sent_messages_.push_back(data);
        if (current_expectation_ >= expectations_.size()) {
            return false;
        }
        
        const auto& expected = expectations_[current_expectation_];
        if (data != expected.request) {
            last_error_ = "Unexpected request";
            return false;
        }
        
        return true;
    }

    bool receive(std::vector<uint8_t>& data, [[maybe_unused]] std::chrono::milliseconds timeout) {
        if (current_expectation_ >= expectations_.size()) {
            return false;
        }
        
        const auto& expected = expectations_[current_expectation_];
        if (expected.delay > std::chrono::milliseconds{0}) {
            std::this_thread::sleep_for(expected.delay);
        }
        
        data = expected.response;
        current_expectation_++;
        received_messages_.push_back(data);
        return true;
    }

    void reset() {
        expectations_.clear();
        sent_messages_.clear();
        received_messages_.clear();
        current_expectation_ = 0;
        last_error_.clear();
    }

    bool verify_all_expectations_met() const {
        return current_expectation_ == expectations_.size();
    }

    const std::vector<std::vector<uint8_t>>& get_sent_messages() const {
        return sent_messages_;
    }

    const std::vector<std::vector<uint8_t>>& get_received_messages() const {
        return received_messages_;
    }

    std::string get_last_error() const { return last_error_; }

private:
    std::vector<ExpectedTransaction> expectations_;
    std::vector<std::vector<uint8_t>> sent_messages_;
    std::vector<std::vector<uint8_t>> received_messages_;
    size_t current_expectation_ = 0;
    std::string last_error_;
};

// Test runner class
class TestRunner {
public:
    static TestRunner& instance() {
        // Use a pointer to avoid destruction order issues with static registrars
        static TestRunner* runner = new TestRunner();
        return *runner;
    }

    void add_test(const std::string& name, std::function<void()> test_func) {
        tests_.push_back({name, test_func});
    }

    void run_all_tests() {
        std::cout << colors::BOLD << colors::CYAN 
                  << "╔═══════════════════════════════════════════════════════════╗\n"
                  << "║           UDS Test Suite - ISO 14229-1 Compliance        ║\n"
                  << "╚═══════════════════════════════════════════════════════════╝"
                  << colors::RESET << "\n\n";

        auto start_time = std::chrono::steady_clock::now();

        for (const auto& test : tests_) {
            run_single_test(test.name, test.func);
        }

        auto end_time = std::chrono::steady_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        print_summary();
    }

    TestStats get_stats() const { return stats_; }

private:
    struct Test {
        std::string name;
        std::function<void()> func;
    };

    void run_single_test(const std::string& name, std::function<void()> test_func) {
        stats_.total++;
        std::cout << colors::BLUE << "[ RUN      ] " << colors::RESET << name << std::endl;

        auto start = std::chrono::steady_clock::now();
        try {
            test_func();
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            stats_.passed++;
            std::cout << colors::GREEN << "[       OK ] " << colors::RESET 
                      << name << " (" << duration.count() << " ms)\n" << std::endl;
        } catch (const std::exception& e) {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            stats_.failed++;
            std::cout << colors::RED << "[  FAILED  ] " << colors::RESET 
                      << name << " (" << duration.count() << " ms)\n"
                      << colors::RED << "  Error: " << e.what() << colors::RESET << "\n" << std::endl;
        } catch (...) {
            stats_.failed++;
            std::cout << colors::RED << "[  FAILED  ] " << colors::RESET 
                      << name << "\n  Unknown error\n" << std::endl;
        }
    }

    void print_summary() {
        std::cout << colors::BOLD << colors::CYAN 
                  << "╔═══════════════════════════════════════════════════════════╗\n"
                  << "║                       Test Summary                        ║\n"
                  << "╚═══════════════════════════════════════════════════════════╝"
                  << colors::RESET << "\n\n";

        std::cout << "Total tests:   " << stats_.total << "\n";
        std::cout << colors::GREEN << "Passed:        " << stats_.passed << colors::RESET << "\n";
        std::cout << colors::RED << "Failed:        " << stats_.failed << colors::RESET << "\n";
        std::cout << "Total time:    " << stats_.total_time.count() << " ms\n";

        double pass_rate = stats_.total > 0 ? 
            (static_cast<double>(stats_.passed) / stats_.total * 100.0) : 0.0;
        
        std::cout << "\nPass rate:     ";
        if (pass_rate >= 100.0) {
            std::cout << colors::GREEN;
        } else if (pass_rate >= 80.0) {
            std::cout << colors::YELLOW;
        } else {
            std::cout << colors::RED;
        }
        std::cout << std::fixed << std::setprecision(1) << pass_rate << "%" 
                  << colors::RESET << "\n\n";

        if (stats_.failed == 0) {
            std::cout << colors::GREEN << colors::BOLD 
                      << "✓ All tests passed!" << colors::RESET << "\n";
        } else {
            std::cout << colors::RED << colors::BOLD 
                      << "✗ Some tests failed" << colors::RESET << "\n";
        }
    }

    std::vector<Test> tests_;
    TestStats stats_;
};

// Test registration macro
#define TEST(test_name) \
    void test_##test_name(); \
    struct TestRegistrar_##test_name { \
        TestRegistrar_##test_name() { \
            test::TestRunner::instance().add_test(#test_name, test_##test_name); \
        } \
    }; \
    static TestRegistrar_##test_name registrar_##test_name; \
    void test_##test_name()

// Assertion macros
#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #condition + \
                                 " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

#define ASSERT_FALSE(condition) \
    ASSERT_TRUE(!(condition))

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        std::ostringstream oss; \
        oss << "Expected: " << (expected) << ", Actual: " << (actual) \
            << " at " << __FILE__ << ":" << __LINE__; \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_NE(val1, val2) \
    if ((val1) == (val2)) { \
        std::ostringstream oss; \
        oss << "Expected values to be different at " \
            << __FILE__ << ":" << __LINE__; \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_GT(val1, val2) \
    if (!((val1) > (val2))) { \
        std::ostringstream oss; \
        oss << "Expected " << (val1) << " > " << (val2) \
            << " at " << __FILE__ << ":" << __LINE__; \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_GE(val1, val2) \
    if (!((val1) >= (val2))) { \
        std::ostringstream oss; \
        oss << "Expected " << (val1) << " >= " << (val2) \
            << " at " << __FILE__ << ":" << __LINE__; \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_LT(val1, val2) \
    if (!((val1) < (val2))) { \
        std::ostringstream oss; \
        oss << "Expected " << (val1) << " < " << (val2) \
            << " at " << __FILE__ << ":" << __LINE__; \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_LE(val1, val2) \
    if (!((val1) <= (val2))) { \
        std::ostringstream oss; \
        oss << "Expected " << (val1) << " <= " << (val2) \
            << " at " << __FILE__ << ":" << __LINE__; \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_THROWS(statement) \
    { \
        bool threw = false; \
        try { \
            statement; \
        } catch (...) { \
            threw = true; \
        } \
        if (!threw) { \
            throw std::runtime_error(std::string("Expected exception but none was thrown at ") + \
                                     __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    }

// Helper functions for hex formatting
inline std::string to_hex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) oss << " ";
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(data[i]);
    }
    oss << "]";
    return oss.str();
}

inline std::string to_hex(uint8_t byte) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(2) << std::setfill('0') 
        << static_cast<int>(byte);
    return oss.str();
}

inline std::string to_hex(uint16_t word) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << word;
    return oss.str();
}

inline std::string to_hex(uint32_t dword) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << dword;
    return oss.str();
}

// Timing helpers for performance tests
class Timer {
public:
    void start() { start_ = std::chrono::steady_clock::now(); }
    
    std::chrono::milliseconds elapsed() const {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
    }

private:
    std::chrono::steady_clock::time_point start_;
};

} // namespace test

#endif // TEST_FRAMEWORK_HPP

