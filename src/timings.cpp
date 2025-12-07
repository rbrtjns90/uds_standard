/**
 * @file timings.cpp
 * @brief UDS Timing Manager Implementation
 */

#include "timings.hpp"
#include <thread>

namespace timings {

TimingManager::TimingManager() 
    : session_start_time_(std::chrono::steady_clock::now()),
      last_request_time_(std::chrono::steady_clock::now()),
      last_response_time_(std::chrono::steady_clock::now()) {
}

TimingManager::TimingManager(const Parameters& params) 
    : p2_(params.P2),
      p2_star_(params.P2_star),
      params_(params),
      session_start_time_(std::chrono::steady_clock::now()),
      last_request_time_(std::chrono::steady_clock::now()),
      last_response_time_(std::chrono::steady_clock::now()) {
}

void TimingManager::update_from_session_params(uint16_t p2_ms, uint16_t p2_star_ms) {
    p2_ = std::chrono::milliseconds(p2_ms);
    p2_star_ = std::chrono::milliseconds(p2_star_ms);
    params_.P2 = p2_;
    params_.P2_star = p2_star_;
}

void TimingManager::set_parameters(const Parameters& params) {
    params_ = params;
    p2_ = params.P2;
    p2_star_ = params.P2_star;
}

void TimingManager::set_P2(std::chrono::milliseconds p2) {
    p2_ = p2;
    params_.P2 = p2;
}

void TimingManager::set_P2_star(std::chrono::milliseconds p2_star) {
    p2_star_ = p2_star;
    params_.P2_star = p2_star;
}

void TimingManager::set_S3(std::chrono::milliseconds s3) {
    params_.S3 = s3;
}

bool TimingManager::parse_from_response(const std::vector<uint8_t>& response) {
    // DiagnosticSessionControl positive response format:
    // [session][P2_high][P2_low][P2*_high][P2*_low]
    if (response.size() < 5) {
        return false;
    }
    
    uint16_t p2_ms = (static_cast<uint16_t>(response[1]) << 8) | response[2];
    uint16_t p2_star_10ms = (static_cast<uint16_t>(response[3]) << 8) | response[4];
    
    // P2* is in 10ms units
    update_from_session_params(p2_ms, p2_star_10ms * 10);
    
    return true;
}

std::chrono::milliseconds TimingManager::get_timeout_for_service(uint8_t service_id) const {
    // Services that typically need extended timeout
    switch (service_id) {
        case 0x34:  // RequestDownload
        case 0x35:  // RequestUpload
        case 0x36:  // TransferData
        case 0x37:  // RequestTransferExit
            return params_.programming_timeout;
        
        case 0x31:  // RoutineControl
            return params_.routine_timeout;
        
        case 0x27:  // SecurityAccess
            return params_.security_delay;
        
        case 0x14:  // ClearDiagnosticInformation
        case 0x19:  // ReadDTCInformation
            return p2_star_;
        
        default:
            return p2_;
    }
}

void TimingManager::reset_session_timer() {
    session_start_time_ = std::chrono::steady_clock::now();
}

bool TimingManager::is_session_expired() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - session_start_time_);
    return elapsed > params_.S3;
}

std::chrono::milliseconds TimingManager::time_until_session_expires() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - session_start_time_);
    
    if (elapsed >= params_.S3) {
        return std::chrono::milliseconds(0);
    }
    
    return params_.S3 - elapsed;
}

void TimingManager::enforce_inter_request_delay() {
    if (params_.P3.count() > 0) {
        auto now = std::chrono::steady_clock::now();
        auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_response_time_);
        
        if (since_last < params_.P3) {
            std::this_thread::sleep_for(params_.P3 - since_last);
        }
    }
}

std::chrono::steady_clock::time_point TimingManager::get_last_request_time() const {
    return last_request_time_;
}

void TimingManager::mark_request_sent() {
    last_request_time_ = std::chrono::steady_clock::now();
    total_requests_++;
}

void TimingManager::mark_response_received() {
    last_response_time_ = std::chrono::steady_clock::now();
    // Reset session timer on any response
    session_start_time_ = last_response_time_;
}

void TimingManager::reset_statistics() {
    total_requests_ = 0;
    total_timeouts_ = 0;
    total_pending_responses_ = 0;
}

} // namespace timings
