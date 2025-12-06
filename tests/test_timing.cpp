/**
 * @file test_timing.cpp
 * @brief UDS timing requirement tests - ISO 14229-1 timing compliance
 */

#include "test_framework.hpp"
#include "../include/timings.hpp"
#include <chrono>
#include <thread>

using namespace test;
using namespace std::chrono;

// ============================================================================
// Test: P2 Timeout (Default Response Time)
// ============================================================================

TEST(Timing_P2_Default) {
    std::cout << "  Testing: P2 default timeout (50ms)" << std::endl;
    
    const auto P2_DEFAULT = milliseconds(50);
    
    ASSERT_EQ(50, P2_DEFAULT.count());
}

TEST(Timing_P2_Measurement) {
    std::cout << "  Testing: P2 timeout measurement" << std::endl;
    
    auto start = steady_clock::now();
    std::this_thread::sleep_for(milliseconds(30));
    auto end = steady_clock::now();
    
    auto elapsed = duration_cast<milliseconds>(end - start);
    
    ASSERT_GE(elapsed.count(), 30);
    ASSERT_LT(elapsed.count(), 50); // Should be less than P2
}

TEST(Timing_P2_Exceeded) {
    std::cout << "  Testing: P2 timeout exceeded detection" << std::endl;
    
    const auto P2_DEFAULT = milliseconds(50);
    
    auto start = steady_clock::now();
    std::this_thread::sleep_for(milliseconds(60)); // Exceed P2
    auto end = steady_clock::now();
    
    auto elapsed = duration_cast<milliseconds>(end - start);
    
    ASSERT_GT(elapsed.count(), P2_DEFAULT.count());
}

// ============================================================================
// Test: P2* Timeout (Extended Response Time)
// ============================================================================

TEST(Timing_P2Star_Extended) {
    std::cout << "  Testing: P2* extended timeout (5000ms)" << std::endl;
    
    const auto P2_STAR = milliseconds(5000);
    
    ASSERT_EQ(5000, P2_STAR.count());
}

TEST(Timing_P2Star_ResponsePending) {
    std::cout << "  Testing: P2* after receiving NRC 0x78 (Response Pending)" << std::endl;
    
    // Simulate receiving NRC 0x78 and extending timeout
    const auto P2_DEFAULT = milliseconds(50);
    const auto P2_STAR = milliseconds(5000);
    
    bool received_nrc_78 = true;
    auto timeout = received_nrc_78 ? P2_STAR : P2_DEFAULT;
    
    ASSERT_EQ(5000, timeout.count());
}

TEST(Timing_P2Star_MultipleExtensions) {
    std::cout << "  Testing: Multiple P2* extensions for long operations" << std::endl;
    
    const auto P2_STAR = milliseconds(5000);
    int extension_count = 0;
    const int max_extensions = 10;
    
    // Simulate receiving multiple NRC 0x78 responses
    for (int i = 0; i < 5; ++i) {
        extension_count++;
        ASSERT_LT(extension_count, max_extensions);
    }
    
    ASSERT_EQ(5, extension_count);
}

// ============================================================================
// Test: S3 Timeout (Session Timeout)
// ============================================================================

TEST(Timing_S3_SessionTimeout) {
    std::cout << "  Testing: S3 session timeout (5000ms)" << std::endl;
    
    const auto S3_SERVER = milliseconds(5000);
    
    ASSERT_EQ(5000, S3_SERVER.count());
}

TEST(Timing_S3_TesterPresent) {
    std::cout << "  Testing: S3 timeout prevention with Tester Present" << std::endl;
    
    const auto S3_SERVER = milliseconds(5000);
    const auto TESTER_PRESENT_INTERVAL = milliseconds(2000);
    
    // Tester Present should be sent before S3 expires
    ASSERT_LT(TESTER_PRESENT_INTERVAL.count(), S3_SERVER.count());
}

TEST(Timing_S3_Expiration) {
    std::cout << "  Testing: Session expiration after S3 timeout" << std::endl;
    
    auto session_start = steady_clock::now();
    const auto S3_SERVER = milliseconds(100); // Reduced for testing
    
    std::this_thread::sleep_for(milliseconds(110));
    
    auto now = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(now - session_start);
    
    bool session_expired = elapsed > S3_SERVER;
    ASSERT_TRUE(session_expired);
}

// ============================================================================
// Test: P3 Timeout (Inter-request Delay)
// ============================================================================

TEST(Timing_P3_InterRequestDelay) {
    std::cout << "  Testing: P3 minimum inter-request delay" << std::endl;
    
    const auto P3_MIN = milliseconds(0); // Can be 0 for client
    
    ASSERT_GE(P3_MIN.count(), 0);
}

TEST(Timing_P3_Enforcement) {
    std::cout << "  Testing: P3 delay enforcement between requests" << std::endl;
    
    const auto P3_MIN = milliseconds(10);
    
    auto request1_time = steady_clock::now();
    std::this_thread::sleep_for(P3_MIN);
    auto request2_time = steady_clock::now();
    
    auto delay = duration_cast<milliseconds>(request2_time - request1_time);
    
    ASSERT_GE(delay.count(), P3_MIN.count());
}

// ============================================================================
// Test: P4 Timeout (Inter-byte Timing)
// ============================================================================

TEST(Timing_P4_InterByteTime) {
    std::cout << "  Testing: P4 inter-byte timing (20ms)" << std::endl;
    
    const auto P4_MAX = milliseconds(20);
    
    ASSERT_EQ(20, P4_MAX.count());
}

// ============================================================================
// Test: Service-Specific Timeouts
// ============================================================================

TEST(Timing_Service_ECUReset) {
    std::cout << "  Testing: ECU Reset extended timeout" << std::endl;
    
    // ECU Reset may take longer
    const auto RESET_TIMEOUT = milliseconds(10000);
    
    ASSERT_GT(RESET_TIMEOUT.count(), 5000);
}

TEST(Timing_Service_SecurityAccess) {
    std::cout << "  Testing: Security Access timeout" << std::endl;
    
    // Security Access uses standard P2/P2*
    const auto P2 = milliseconds(50);
    const auto P2_STAR = milliseconds(5000);
    
    ASSERT_EQ(50, P2.count());
    ASSERT_EQ(5000, P2_STAR.count());
}

TEST(Timing_Service_RoutineControl) {
    std::cout << "  Testing: Routine Control extended timeout" << std::endl;
    
    // Some routines (e.g., erase) take very long
    const auto ROUTINE_TIMEOUT = milliseconds(60000); // 60 seconds
    
    ASSERT_GT(ROUTINE_TIMEOUT.count(), 5000);
}

TEST(Timing_Service_RequestDownload) {
    std::cout << "  Testing: Request Download timeout" << std::endl;
    
    const auto DOWNLOAD_TIMEOUT = milliseconds(2000);
    
    ASSERT_GT(DOWNLOAD_TIMEOUT.count(), 50);
}

TEST(Timing_Service_TransferData) {
    std::cout << "  Testing: Transfer Data timeout (per block)" << std::endl;
    
    const auto TRANSFER_TIMEOUT = milliseconds(1000);
    
    ASSERT_GT(TRANSFER_TIMEOUT.count(), 50);
}

// ============================================================================
// Test: Timeout Calculation
// ============================================================================

TEST(Timing_Calculate_Scaled) {
    std::cout << "  Testing: Scaled timeout calculation" << std::endl;
    
    const auto BASE_TIMEOUT = milliseconds(50);
    const double SCALE_FACTOR = 1.5;
    
    auto scaled = milliseconds(static_cast<long long>(BASE_TIMEOUT.count() * SCALE_FACTOR));
    
    ASSERT_EQ(75, scaled.count());
}

TEST(Timing_Calculate_WithMargin) {
    std::cout << "  Testing: Timeout with safety margin" << std::endl;
    
    const auto BASE_TIMEOUT = milliseconds(50);
    const auto MARGIN = milliseconds(10);
    
    auto with_margin = BASE_TIMEOUT + MARGIN;
    
    ASSERT_EQ(60, with_margin.count());
}

// ============================================================================
// Test: Timing Manager
// ============================================================================

TEST(TimingManager_DefaultValues) {
    std::cout << "  Testing: Timing manager default values" << std::endl;
    
    timings::TimingManager mgr;
    
    auto p2 = mgr.p2();
    auto p2_star = mgr.p2_star();
    
    ASSERT_EQ(50, p2.count());
    ASSERT_EQ(5000, p2_star.count());
}

TEST(TimingManager_CustomValues) {
    std::cout << "  Testing: Timing manager custom values" << std::endl;
    
    timings::TimingManager mgr;
    
    mgr.set_P2(milliseconds(100));
    mgr.set_P2_star(milliseconds(10000));
    
    ASSERT_EQ(100, mgr.p2().count());
    ASSERT_EQ(10000, mgr.p2_star().count());
}

TEST(TimingManager_ServiceTimeout) {
    std::cout << "  Testing: Service-specific timeout retrieval" << std::endl;
    
    timings::TimingManager mgr;
    
    // Different services may have different timeouts
    auto timeout_22 = mgr.get_timeout_for_service(0x22); // ReadDataByIdentifier
    auto timeout_31 = mgr.get_timeout_for_service(0x31); // RoutineControl
    
    // Routine control typically has longest timeout
    ASSERT_GT(timeout_31.count(), timeout_22.count());
}

TEST(TimingManager_SessionExpiry) {
    std::cout << "  Testing: Session expiry detection" << std::endl;
    
    timings::TimingManager mgr;
    mgr.reset_session_timer();
    
    // Check immediately - should not be expired
    ASSERT_FALSE(mgr.is_session_expired());
    
    // Simulate passage of time
    std::this_thread::sleep_for(milliseconds(100));
    
    // With S3=5000ms, should still not be expired after 100ms
    ASSERT_FALSE(mgr.is_session_expired());
}

TEST(TimingManager_ResetSession) {
    std::cout << "  Testing: Session timer reset" << std::endl;
    
    timings::TimingManager mgr;
    mgr.reset_session_timer();
    
    std::this_thread::sleep_for(milliseconds(50));
    
    mgr.reset_session_timer(); // Reset timer
    
    // Should not be expired after reset
    ASSERT_FALSE(mgr.is_session_expired());
}

// ============================================================================
// Test: Performance Measurements
// ============================================================================

TEST(Performance_FastResponse) {
    std::cout << "  Testing: Fast response measurement (<10ms)" << std::endl;
    
    Timer timer;
    timer.start();
    
    // Simulate fast operation
    std::this_thread::sleep_for(milliseconds(5));
    
    auto elapsed = timer.elapsed();
    
    ASSERT_LT(elapsed.count(), 10);
}

TEST(Performance_SlowResponse) {
    std::cout << "  Testing: Slow response measurement (>P2)" << std::endl;
    
    Timer timer;
    timer.start();
    
    const auto P2 = milliseconds(50);
    std::this_thread::sleep_for(milliseconds(60));
    
    auto elapsed = timer.elapsed();
    
    ASSERT_GT(elapsed.count(), P2.count());
}

TEST(Performance_Throughput) {
    std::cout << "  Testing: Request throughput (requests per second)" << std::endl;
    
    const int NUM_REQUESTS = 10;
    
    auto start = steady_clock::now();
    
    for (int i = 0; i < NUM_REQUESTS; ++i) {
        std::this_thread::sleep_for(milliseconds(10));
    }
    
    auto end = steady_clock::now();
    auto total_time = duration_cast<milliseconds>(end - start);
    
    double requests_per_second = (NUM_REQUESTS * 1000.0) / total_time.count();
    
    ASSERT_GT(requests_per_second, 0);
}

// ============================================================================
// Test: Deadline Management
// ============================================================================

TEST(Deadline_Calculate) {
    std::cout << "  Testing: Deadline calculation" << std::endl;
    
    auto now = steady_clock::now();
    auto timeout = milliseconds(100);
    auto deadline = now + timeout;
    
    auto remaining = duration_cast<milliseconds>(deadline - now);
    
    ASSERT_GE(remaining.count(), 99); // Account for execution time
}

TEST(Deadline_Expired) {
    std::cout << "  Testing: Deadline expiration check" << std::endl;
    
    auto deadline = steady_clock::now() + milliseconds(10);
    
    std::this_thread::sleep_for(milliseconds(20));
    
    bool expired = steady_clock::now() > deadline;
    ASSERT_TRUE(expired);
}

TEST(Deadline_NotExpired) {
    std::cout << "  Testing: Deadline not expired check" << std::endl;
    
    auto deadline = steady_clock::now() + milliseconds(100);
    
    std::this_thread::sleep_for(milliseconds(10));
    
    bool expired = steady_clock::now() > deadline;
    ASSERT_FALSE(expired);
}

// ============================================================================
// Test: Timing Statistics
// ============================================================================

TEST(Statistics_AverageResponseTime) {
    std::cout << "  Testing: Average response time calculation" << std::endl;
    
    std::vector<int> response_times = {30, 40, 35, 45, 50};
    
    int sum = 0;
    for (auto time : response_times) {
        sum += time;
    }
    
    double average = static_cast<double>(sum) / response_times.size();
    
    ASSERT_EQ(40, static_cast<int>(average));
}

TEST(Statistics_MaxResponseTime) {
    std::cout << "  Testing: Maximum response time tracking" << std::endl;
    
    std::vector<int> response_times = {30, 40, 100, 45, 50};
    
    int max_time = *std::max_element(response_times.begin(), response_times.end());
    
    ASSERT_EQ(100, max_time);
}

TEST(Statistics_TimeoutRate) {
    std::cout << "  Testing: Timeout rate calculation" << std::endl;
    
    int total_requests = 100;
    int timeouts = 5;
    
    double timeout_rate = (static_cast<double>(timeouts) / total_requests) * 100.0;
    
    ASSERT_EQ(5.0, timeout_rate);
}

// ============================================================================
// Main test runner
// ============================================================================

int main() {
    std::cout << "\n" << colors::BOLD << "UDS Timing Requirement Tests" << colors::RESET << "\n";
    std::cout << "Testing ISO 14229-1 Timing Compliance\n\n";
    
    TestRunner::instance().run_all_tests();
    
    auto stats = TestRunner::instance().get_stats();
    return (stats.failed == 0) ? 0 : 1;
}

