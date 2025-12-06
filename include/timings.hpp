#pragma once

/**
 * @file timings.hpp
 * @brief UDS Timing Parameters - ISO 14229-1:2013 Section 7
 * 
 * ISO 14229-1:2013 Timing Parameters (pp. 15-28):
 * Section 7: Application layer protocol
 * 
 * Timing Parameters Defined:
 * - P2server_max (P2): Default maximum time for server response
 *   Default: 50ms
 *   Used for normal request/response operations
 * 
 * - P2*server_max (P2*): Enhanced maximum time for server response
 *   Default: 5000ms (5 seconds)
 *   Used when server sends NRC 0x78 (ResponsePending)
 * 
 * - S3server: Session timeout on server side
 *   Default: 5000ms (5 seconds)
 *   Server returns to default session if no request received
 * 
 * - P3client_min (P3): Minimum time between tester request and server response
 *   Default: 0ms
 *   Inter-message delay
 * 
 * - P4server_max (P4): Inter-byte time for server response
 *   Default: 20ms (recommended, not mandatory)
 * 
 * Enhanced Timings:
 * - Programming operations: May use extended P2* (up to 30 seconds)
 * - Routine control: May require custom timeouts per routine
 * - Security access: Anti-hammering delays (typically 10 seconds)
 * 
 * Usage:
 *   timings::TimingManager mgr;
 *   mgr.update_from_session_params(100, 10000);  // P2=100ms, P2*=10s
 *   auto timeout = mgr.get_timeout_for_service(0x34);  // Programming service
 */

#include <cstdint>
#include <chrono>
#include <optional>
#include <vector>

namespace timings {

// ============================================================================
// UDS Timing Parameters (ISO 14229-1)
// ============================================================================

struct Parameters {
    // P2 - Default timeout for ECU response (ms)
    std::chrono::milliseconds P2{50};
    
    // P2* - Extended timeout for ECU response when busy (ms)
    // Used with NRC 0x78 (ResponsePending)
    std::chrono::milliseconds P2_star{5000};
    
    // S3 - Session timeout (ms)
    std::chrono::milliseconds S3{5000};
    
    // P3 - Delay between end of response and start of new request (ms)
    std::chrono::milliseconds P3{0};
    
    // P4 - Inter-byte time for server response
    std::chrono::milliseconds P4{20};
    
    // Enhanced timing parameters
    std::chrono::milliseconds programming_timeout{30000};
    std::chrono::milliseconds routine_timeout{10000};
    std::chrono::milliseconds security_delay{10000};
};

// ============================================================================
// Timing Manager - Simple + Advanced API
// ============================================================================

class TimingManager {
public:
    TimingManager();
    explicit TimingManager(const Parameters& params);
    
    // Simple API (user specification)
    void update_from_session_params(uint16_t p2_ms, uint16_t p2_star_ms);
    std::chrono::milliseconds p2() const { return p2_; }
    std::chrono::milliseconds p2_star() const { return p2_star_; }
    
    // Advanced API
    const Parameters& get_parameters() const { return params_; }
    void set_parameters(const Parameters& params);
    void set_P2(std::chrono::milliseconds p2);
    void set_P2_star(std::chrono::milliseconds p2_star);
    void set_S3(std::chrono::milliseconds s3);
    bool parse_from_response(const std::vector<uint8_t>& response);
    std::chrono::milliseconds get_timeout_for_service(uint8_t service_id) const;
    std::chrono::milliseconds get_pending_timeout() const { return p2_star_; }
    std::chrono::milliseconds get_default_timeout() const { return p2_; }
    void reset_session_timer();
    bool is_session_expired() const;
    std::chrono::milliseconds time_until_session_expires() const;
    void enforce_inter_request_delay();
    std::chrono::steady_clock::time_point get_last_request_time() const;
    void mark_request_sent();
    void mark_response_received();
    uint32_t get_total_requests() const { return total_requests_; }
    uint32_t get_total_timeouts() const { return total_timeouts_; }
    uint32_t get_total_pending_responses() const { return total_pending_responses_; }
    void reset_statistics();
    
private:
    std::chrono::milliseconds p2_ = std::chrono::milliseconds(50);
    std::chrono::milliseconds p2_star_ = std::chrono::milliseconds(5000);
    Parameters params_;
    std::chrono::steady_clock::time_point session_start_time_;
    std::chrono::steady_clock::time_point last_request_time_;
    std::chrono::steady_clock::time_point last_response_time_;
    uint32_t total_requests_{0};
    uint32_t total_timeouts_{0};
    uint32_t total_pending_responses_{0};
};

} // namespace timings
