#include "uds_io.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace uds {
namespace io {

// ============================================================================
// Payload Building
// ============================================================================

std::vector<uint8_t> build_io_control_payload(const IOControlRequest& request) {
    std::vector<uint8_t> payload;
    payload.reserve(3 + request.control_enable_mask.size() + request.control_state.size());
    
    // DID (2 bytes, big-endian)
    payload.push_back(static_cast<uint8_t>(request.data_identifier >> 8));
    payload.push_back(static_cast<uint8_t>(request.data_identifier & 0xFF));
    
    // Control option parameter
    payload.push_back(static_cast<uint8_t>(request.control_option));
    
    // Control enable mask (optional, for masked operations)
    if (!request.control_enable_mask.empty()) {
        payload.insert(payload.end(), 
                      request.control_enable_mask.begin(), 
                      request.control_enable_mask.end());
    }
    
    // Control state (for ShortTermAdjustment)
    if (!request.control_state.empty()) {
        payload.insert(payload.end(), 
                      request.control_state.begin(), 
                      request.control_state.end());
    }
    
    return payload;
}

// ============================================================================
// Response Parsing
// ============================================================================

IOControlResponse parse_io_control_response(const std::vector<uint8_t>& payload) {
    IOControlResponse response;
    
    if (payload.size() < 3) {
        return response;  // Invalid response
    }
    
    // DID (2 bytes, big-endian)
    response.data_identifier = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
    
    // Control option echo
    response.control_option = static_cast<ControlOption>(payload[2]);
    
    // Control status (remaining bytes)
    if (payload.size() > 3) {
        response.control_status.assign(payload.begin() + 3, payload.end());
    }
    
    return response;
}

// ============================================================================
// Core I/O Control Function
// ============================================================================

IOControlResult io_control(Client& client, const IOControlRequest& request) {
    IOControlResult result;
    
    // Build request payload
    std::vector<uint8_t> payload = build_io_control_payload(request);
    
    // Send request
    auto response = client.exchange(SID::InputOutputControlByIdentifier, payload);
    
    if (!response.ok) {
        result.ok = false;
        result.nrc = response.nrc.code;
        
        // Generate error message based on NRC
        switch (response.nrc.code) {
            case NegativeResponseCode::SubFunctionNotSupported:
                result.error_message = "Control option not supported";
                break;
            case NegativeResponseCode::RequestOutOfRange:
                result.error_message = "DID not found or value out of range";
                break;
            case NegativeResponseCode::SecurityAccessDenied:
                result.error_message = "Security access required for this I/O";
                break;
            case NegativeResponseCode::ConditionsNotCorrect:
                result.error_message = "Conditions not correct (check vehicle state)";
                break;
            case NegativeResponseCode::RequestSequenceError:
                result.error_message = "Request sequence error";
                break;
            default:
                result.error_message = "I/O control failed with NRC 0x" + 
                    std::to_string(static_cast<int>(response.nrc.code));
        }
        return result;
    }
    
    // Parse response
    result.ok = true;
    result.response = parse_io_control_response(response.payload);
    
    return result;
}

// ============================================================================
// Convenience Functions
// ============================================================================

IOControlResult return_control_to_ecu(Client& client, uint16_t did) {
    IOControlRequest request(did, ControlOption::ReturnControlToECU);
    return io_control(client, request);
}

IOControlResult reset_to_default(Client& client, uint16_t did) {
    IOControlRequest request(did, ControlOption::ResetToDefault);
    return io_control(client, request);
}

IOControlResult freeze_current_state(Client& client, uint16_t did) {
    IOControlRequest request(did, ControlOption::FreezeCurrentState);
    return io_control(client, request);
}

IOControlResult short_term_adjustment(Client& client, uint16_t did,
                                       const std::vector<uint8_t>& value) {
    IOControlRequest request(did, ControlOption::ShortTermAdjustment, value);
    return io_control(client, request);
}

IOControlResult short_term_adjustment_masked(Client& client, uint16_t did,
                                              const std::vector<uint8_t>& value,
                                              const std::vector<uint8_t>& mask) {
    IOControlRequest request;
    request.data_identifier = did;
    request.control_option = ControlOption::ShortTermAdjustment;
    request.control_enable_mask = mask;
    request.control_state = value;
    return io_control(client, request);
}

IOControlResult set_digital_output(Client& client, uint16_t did, bool state) {
    std::vector<uint8_t> value = { state ? uint8_t(0xFF) : uint8_t(0x00) };
    return short_term_adjustment(client, did, value);
}

IOControlResult set_analog_output_8bit(Client& client, uint16_t did, uint8_t value) {
    std::vector<uint8_t> data = { value };
    return short_term_adjustment(client, did, data);
}

IOControlResult set_analog_output_16bit(Client& client, uint16_t did, uint16_t value) {
    std::vector<uint8_t> data = { 
        static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value & 0xFF)
    };
    return short_term_adjustment(client, did, data);
}

IOControlResult set_pwm_duty_cycle(Client& client, uint16_t did, float duty_percent) {
    // Clamp to 0-100%
    if (duty_percent < 0.0f) duty_percent = 0.0f;
    if (duty_percent > 100.0f) duty_percent = 100.0f;
    
    // Convert to 0-255 range
    uint8_t value = static_cast<uint8_t>((duty_percent / 100.0f) * 255.0f);
    return set_analog_output_8bit(client, did, value);
}

// ============================================================================
// IOControlGuard Implementation
// ============================================================================

IOControlGuard::IOControlGuard(Client& client, uint16_t did, bool take_control)
    : client_(&client), did_(did), active_(false) {
    if (take_control) {
        auto result = freeze_current_state(*client_, did_);
        active_ = result.ok;
    }
}

IOControlGuard::~IOControlGuard() {
    release();
}

IOControlGuard::IOControlGuard(IOControlGuard&& other) noexcept
    : client_(other.client_), did_(other.did_), active_(other.active_) {
    other.active_ = false;
    other.client_ = nullptr;
}

IOControlGuard& IOControlGuard::operator=(IOControlGuard&& other) noexcept {
    if (this != &other) {
        release();
        client_ = other.client_;
        did_ = other.did_;
        active_ = other.active_;
        other.active_ = false;
        other.client_ = nullptr;
    }
    return *this;
}

void IOControlGuard::release() {
    if (active_ && client_) {
        return_control_to_ecu(*client_, did_);
        active_ = false;
    }
}

IOControlResult IOControlGuard::set_value(const std::vector<uint8_t>& value) {
    if (!active_ || !client_) {
        IOControlResult result;
        result.ok = false;
        result.error_message = "Control not active";
        return result;
    }
    return short_term_adjustment(*client_, did_, value);
}

// ============================================================================
// IOControlSession Implementation
// ============================================================================

IOControlSession::IOControlSession(Client& client)
    : client_(client) {
}

IOControlSession::~IOControlSession() {
    release_all();
}

bool IOControlSession::acquire(uint16_t did) {
    // Check if already controlled
    if (is_controlled(did)) {
        return true;
    }
    
    auto result = freeze_current_state(client_, did);
    if (result.ok) {
        controlled_.push_back(did);
        return true;
    }
    return false;
}

void IOControlSession::release(uint16_t did) {
    auto it = std::find(controlled_.begin(), controlled_.end(), did);
    if (it != controlled_.end()) {
        return_control_to_ecu(client_, did);
        controlled_.erase(it);
    }
}

void IOControlSession::release_all() {
    for (uint16_t did : controlled_) {
        return_control_to_ecu(client_, did);
    }
    controlled_.clear();
}

bool IOControlSession::is_controlled(uint16_t did) const {
    return std::find(controlled_.begin(), controlled_.end(), did) != controlled_.end();
}

std::vector<uint16_t> IOControlSession::controlled_dids() const {
    return controlled_;
}

IOControlResult IOControlSession::set_value(uint16_t did, const std::vector<uint8_t>& value) {
    if (!is_controlled(did)) {
        IOControlResult result;
        result.ok = false;
        result.error_message = "DID not under control";
        return result;
    }
    return short_term_adjustment(client_, did, value);
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string describe_io_result(const IOControlResult& result) {
    std::ostringstream ss;
    
    if (result.ok) {
        ss << "I/O Control Success: DID 0x" 
           << std::hex << std::setw(4) << std::setfill('0') << result.response.data_identifier
           << ", Option: " << control_option_name(result.response.control_option);
        
        if (!result.response.control_status.empty()) {
            ss << ", Status:";
            for (uint8_t b : result.response.control_status) {
                ss << " " << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
            }
        }
    } else {
        ss << "I/O Control Failed: " << result.error_message;
        if (result.nrc != static_cast<NegativeResponseCode>(0x00)) {
            ss << " (NRC: 0x" << std::hex << std::setw(2) << std::setfill('0') 
               << static_cast<int>(result.nrc) << ")";
        }
    }
    
    return ss.str();
}

} // namespace io
} // namespace uds
