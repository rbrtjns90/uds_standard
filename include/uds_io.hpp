#pragma once
/**
 * @file uds_io.hpp
 * @brief InputOutputControlByIdentifier (0x2F) - ISO 14229-1:2013 Section 12.2
 * 
 * This module provides functionality for controlling ECU I/O components
 * such as actuators, sensors, and outputs.
 * 
 * ============================================================================
 * ISO 14229-1:2013 REFERENCE - Section 12.2 (p. 245)
 * ============================================================================
 * 
 * Service Description:
 *   The InputOutputControlByIdentifier service allows a client to substitute
 *   a value for an input signal, internal server function, or output signal.
 * 
 * Request Format:
 *   [0x2F] [dataIdentifier (2 bytes)] [controlOptionRecord] [controlEnableMaskRecord]
 * 
 * Control Options:
 *   0x00: returnControlToECU - Release control back to ECU
 *   0x01: resetToDefault - Reset I/O to default value
 *   0x02: freezeCurrentState - Hold current value
 *   0x03: shortTermAdjustment - Temporary override with specified value
 *   0x04-0xFF: Vehicle manufacturer specific
 * 
 * Response Format:
 *   [0x6F] [dataIdentifier (2 bytes)] [controlStatusRecord]
 * 
 * Supported NRCs (Annex A, Table A.1):
 *   0x13: incorrectMessageLengthOrInvalidFormat
 *   0x22: conditionsNotCorrect
 *   0x31: requestOutOfRange
 *   0x33: securityAccessDenied
 * 
 * WARNING: I/O control can affect vehicle behavior. Use with caution and
 * ensure proper safety measures are in place.
 */

#include "uds.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <chrono>

namespace uds {
namespace io {

// ============================================================================
// I/O Control Option Parameters
// ISO 14229-1:2013 Section 8.19.2.2, Table 98 (p. 324)
// ============================================================================

/**
 * @brief I/O control parameter options
 * 
 * ISO 14229-1:2013 Table 98 (p. 324):
 * inputOutputControlParameter values for controlling ECU I/O.
 */
enum class ControlOption : uint8_t {
    ReturnControlToECU = 0x00,      ///< Table 98 - Release control to ECU
    ResetToDefault = 0x01,          ///< Table 98 - Reset to default value
    FreezeCurrentState = 0x02,      ///< Table 98 - Hold current state
    ShortTermAdjustment = 0x03      ///< Table 98 - Temporary override
    // 0x04-0xFF: Vehicle manufacturer specific (Table 98)
};

/**
 * @brief Get string name for control option
 */
inline const char* control_option_name(ControlOption opt) {
    switch (opt) {
        case ControlOption::ReturnControlToECU: return "ReturnControlToECU";
        case ControlOption::ResetToDefault: return "ResetToDefault";
        case ControlOption::FreezeCurrentState: return "FreezeCurrentState";
        case ControlOption::ShortTermAdjustment: return "ShortTermAdjustment";
        default: return "VehicleManufacturerSpecific";
    }
}

// ============================================================================
// I/O Control Status
// ============================================================================

/**
 * @brief Status of an I/O control operation
 */
enum class IOStatus : uint8_t {
    Idle = 0x00,                    ///< No active control
    Active = 0x01,                  ///< Control is active
    Pending = 0x02,                 ///< Control request pending
    Failed = 0x03,                  ///< Control failed
    TimedOut = 0x04,                ///< Control timed out
    SecurityDenied = 0x05           ///< Security access required
};

// ============================================================================
// I/O Control Data Structures
// ============================================================================

/**
 * @brief I/O control request parameters
 */
struct IOControlRequest {
    uint16_t data_identifier;               ///< DID to control
    ControlOption control_option;           ///< Control option
    std::vector<uint8_t> control_enable_mask; ///< Which bits to control (optional)
    std::vector<uint8_t> control_state;     ///< Desired state/value (for ShortTermAdjustment)
    
    IOControlRequest() = default;
    IOControlRequest(uint16_t did, ControlOption opt)
        : data_identifier(did), control_option(opt) {}
    IOControlRequest(uint16_t did, ControlOption opt, const std::vector<uint8_t>& state)
        : data_identifier(did), control_option(opt), control_state(state) {}
};

/**
 * @brief I/O control response data
 */
struct IOControlResponse {
    uint16_t data_identifier;               ///< DID that was controlled
    ControlOption control_option;           ///< Control option that was applied
    std::vector<uint8_t> control_status;    ///< Current status/value after control
    
    bool is_valid() const { return data_identifier != 0; }
};

/**
 * @brief Result of an I/O control operation
 */
struct IOControlResult {
    bool ok = false;                        ///< True if operation succeeded
    IOControlResponse response;             ///< Response data
    NegativeResponseCode nrc = static_cast<NegativeResponseCode>(0x00);  ///< 0x00 indicates no NRC (success)
    std::string error_message;              ///< Human-readable error
};

// ============================================================================
// I/O Identifier Information
// ============================================================================

/**
 * @brief Information about an I/O controllable identifier
 */
struct IOIdentifierInfo {
    uint16_t did;                           ///< Data identifier
    std::string name;                       ///< Human-readable name
    std::string description;                ///< Description of the I/O
    uint8_t data_length;                    ///< Length of control data in bytes
    bool requires_security;                 ///< Requires security access
    uint8_t required_security_level;        ///< Security level if required
    std::vector<ControlOption> supported_options; ///< Supported control options
    
    IOIdentifierInfo() = default;
    IOIdentifierInfo(uint16_t id, const std::string& n, uint8_t len)
        : did(id), name(n), data_length(len), requires_security(false), required_security_level(0) {}
};

// ============================================================================
// Common Automotive I/O DIDs
// ============================================================================

namespace common_io {

// Engine/Powertrain I/O
constexpr uint16_t THROTTLE_ACTUATOR = 0xF000;
constexpr uint16_t IDLE_AIR_CONTROL = 0xF001;
constexpr uint16_t EGR_VALVE = 0xF002;
constexpr uint16_t FUEL_INJECTOR_1 = 0xF010;
constexpr uint16_t FUEL_INJECTOR_2 = 0xF011;
constexpr uint16_t FUEL_INJECTOR_3 = 0xF012;
constexpr uint16_t FUEL_INJECTOR_4 = 0xF013;
constexpr uint16_t IGNITION_COIL_1 = 0xF020;
constexpr uint16_t IGNITION_COIL_2 = 0xF021;
constexpr uint16_t IGNITION_COIL_3 = 0xF022;
constexpr uint16_t IGNITION_COIL_4 = 0xF023;
constexpr uint16_t FUEL_PUMP_RELAY = 0xF030;
constexpr uint16_t COOLING_FAN_RELAY = 0xF031;
constexpr uint16_t AC_COMPRESSOR_CLUTCH = 0xF032;

// Lighting I/O
constexpr uint16_t HEADLIGHT_LOW = 0xF100;
constexpr uint16_t HEADLIGHT_HIGH = 0xF101;
constexpr uint16_t TURN_SIGNAL_LEFT = 0xF102;
constexpr uint16_t TURN_SIGNAL_RIGHT = 0xF103;
constexpr uint16_t BRAKE_LIGHT = 0xF104;
constexpr uint16_t REVERSE_LIGHT = 0xF105;
constexpr uint16_t FOG_LIGHT_FRONT = 0xF106;
constexpr uint16_t FOG_LIGHT_REAR = 0xF107;
constexpr uint16_t INTERIOR_LIGHT = 0xF108;
constexpr uint16_t INSTRUMENT_BACKLIGHT = 0xF109;

// Body Control I/O
constexpr uint16_t DOOR_LOCK_DRIVER = 0xF200;
constexpr uint16_t DOOR_LOCK_PASSENGER = 0xF201;
constexpr uint16_t DOOR_LOCK_REAR_LEFT = 0xF202;
constexpr uint16_t DOOR_LOCK_REAR_RIGHT = 0xF203;
constexpr uint16_t WINDOW_DRIVER = 0xF210;
constexpr uint16_t WINDOW_PASSENGER = 0xF211;
constexpr uint16_t WINDOW_REAR_LEFT = 0xF212;
constexpr uint16_t WINDOW_REAR_RIGHT = 0xF213;
constexpr uint16_t SUNROOF = 0xF220;
constexpr uint16_t TRUNK_RELEASE = 0xF221;
constexpr uint16_t HORN = 0xF230;
constexpr uint16_t WIPER_FRONT = 0xF240;
constexpr uint16_t WIPER_REAR = 0xF241;
constexpr uint16_t WASHER_FRONT = 0xF242;
constexpr uint16_t WASHER_REAR = 0xF243;

// HVAC I/O
constexpr uint16_t BLOWER_MOTOR = 0xF300;
constexpr uint16_t AC_CLUTCH = 0xF301;
constexpr uint16_t HEATER_VALVE = 0xF302;
constexpr uint16_t BLEND_DOOR = 0xF303;
constexpr uint16_t RECIRCULATION_DOOR = 0xF304;

// Instrument Cluster I/O
constexpr uint16_t SPEEDOMETER = 0xF400;
constexpr uint16_t TACHOMETER = 0xF401;
constexpr uint16_t FUEL_GAUGE = 0xF402;
constexpr uint16_t TEMP_GAUGE = 0xF403;
constexpr uint16_t WARNING_LAMP_MIL = 0xF410;
constexpr uint16_t WARNING_LAMP_ABS = 0xF411;
constexpr uint16_t WARNING_LAMP_AIRBAG = 0xF412;
constexpr uint16_t WARNING_LAMP_OIL = 0xF413;
constexpr uint16_t WARNING_LAMP_BATTERY = 0xF414;
constexpr uint16_t WARNING_LAMP_TEMP = 0xF415;

} // namespace common_io

// ============================================================================
// I/O Control API Functions
// ============================================================================

/**
 * @brief Send InputOutputControlByIdentifier request
 * @param client UDS client
 * @param request Control request parameters
 * @return Control result with response or error
 */
IOControlResult io_control(Client& client, const IOControlRequest& request);

/**
 * @brief Return control of an I/O to the ECU
 * @param client UDS client
 * @param did Data identifier to release
 * @return Control result
 */
IOControlResult return_control_to_ecu(Client& client, uint16_t did);

/**
 * @brief Reset an I/O to its default value
 * @param client UDS client
 * @param did Data identifier to reset
 * @return Control result
 */
IOControlResult reset_to_default(Client& client, uint16_t did);

/**
 * @brief Freeze the current state of an I/O
 * @param client UDS client
 * @param did Data identifier to freeze
 * @return Control result
 */
IOControlResult freeze_current_state(Client& client, uint16_t did);

/**
 * @brief Apply short-term adjustment to an I/O
 * @param client UDS client
 * @param did Data identifier to adjust
 * @param value New value to apply
 * @return Control result
 */
IOControlResult short_term_adjustment(Client& client, uint16_t did, 
                                       const std::vector<uint8_t>& value);

/**
 * @brief Apply short-term adjustment with enable mask
 * @param client UDS client
 * @param did Data identifier to adjust
 * @param value New value to apply
 * @param mask Enable mask (which bits to control)
 * @return Control result
 */
IOControlResult short_term_adjustment_masked(Client& client, uint16_t did,
                                              const std::vector<uint8_t>& value,
                                              const std::vector<uint8_t>& mask);

// ============================================================================
// Convenience Functions for Common I/O Operations
// ============================================================================

/**
 * @brief Set a digital output (on/off)
 * @param client UDS client
 * @param did Data identifier
 * @param state True = on, False = off
 * @return Control result
 */
IOControlResult set_digital_output(Client& client, uint16_t did, bool state);

/**
 * @brief Set an analog output (0-255 or 0-65535)
 * @param client UDS client
 * @param did Data identifier
 * @param value Output value (8-bit)
 * @return Control result
 */
IOControlResult set_analog_output_8bit(Client& client, uint16_t did, uint8_t value);

/**
 * @brief Set an analog output (16-bit)
 * @param client UDS client
 * @param did Data identifier
 * @param value Output value (16-bit)
 * @return Control result
 */
IOControlResult set_analog_output_16bit(Client& client, uint16_t did, uint16_t value);

/**
 * @brief Set a PWM output duty cycle
 * @param client UDS client
 * @param did Data identifier
 * @param duty_percent Duty cycle percentage (0.0 - 100.0)
 * @return Control result
 */
IOControlResult set_pwm_duty_cycle(Client& client, uint16_t did, float duty_percent);

// ============================================================================
// I/O Control Guard (RAII)
// ============================================================================

/**
 * @brief RAII guard that automatically returns control to ECU on destruction
 * 
 * Use this to ensure I/O control is always released, even if an exception occurs.
 * 
 * Example:
 * @code
 * {
 *     IOControlGuard guard(client, 0xF100);  // Take control of headlight
 *     if (guard.is_active()) {
 *         set_digital_output(client, 0xF100, true);  // Turn on
 *         // ... do work ...
 *     }
 * }  // Automatically returns control to ECU
 * @endcode
 */
class IOControlGuard {
public:
    /**
     * @brief Construct guard and optionally take control
     * @param client UDS client reference
     * @param did Data identifier to control
     * @param take_control If true, immediately freeze current state
     */
    IOControlGuard(Client& client, uint16_t did, bool take_control = true);
    
    /**
     * @brief Destructor - returns control to ECU
     */
    ~IOControlGuard();
    
    // Non-copyable
    IOControlGuard(const IOControlGuard&) = delete;
    IOControlGuard& operator=(const IOControlGuard&) = delete;
    
    // Movable
    IOControlGuard(IOControlGuard&& other) noexcept;
    IOControlGuard& operator=(IOControlGuard&& other) noexcept;
    
    /**
     * @brief Check if control is active
     */
    bool is_active() const { return active_; }
    
    /**
     * @brief Get the controlled DID
     */
    uint16_t did() const { return did_; }
    
    /**
     * @brief Manually release control early
     */
    void release();
    
    /**
     * @brief Apply a value while holding control
     */
    IOControlResult set_value(const std::vector<uint8_t>& value);

private:
    Client* client_;
    uint16_t did_;
    bool active_;
};

// ============================================================================
// Multi-I/O Control Session
// ============================================================================

/**
 * @brief Manage multiple I/O controls in a session
 * 
 * Useful for coordinated control of multiple outputs.
 */
class IOControlSession {
public:
    explicit IOControlSession(Client& client);
    ~IOControlSession();
    
    // Non-copyable, non-movable
    IOControlSession(const IOControlSession&) = delete;
    IOControlSession& operator=(const IOControlSession&) = delete;
    
    /**
     * @brief Take control of an I/O
     * @param did Data identifier
     * @return True if control was acquired
     */
    bool acquire(uint16_t did);
    
    /**
     * @brief Release control of an I/O
     * @param did Data identifier
     */
    void release(uint16_t did);
    
    /**
     * @brief Release all controlled I/Os
     */
    void release_all();
    
    /**
     * @brief Check if a DID is under control
     */
    bool is_controlled(uint16_t did) const;
    
    /**
     * @brief Get list of controlled DIDs
     */
    std::vector<uint16_t> controlled_dids() const;
    
    /**
     * @brief Set value for a controlled I/O
     */
    IOControlResult set_value(uint16_t did, const std::vector<uint8_t>& value);

private:
    Client& client_;
    std::vector<uint16_t> controlled_;
};

// ============================================================================
// I/O Control Helpers
// ============================================================================

/**
 * @brief Build I/O control request payload
 */
std::vector<uint8_t> build_io_control_payload(const IOControlRequest& request);

/**
 * @brief Parse I/O control response
 */
IOControlResponse parse_io_control_response(const std::vector<uint8_t>& payload);

/**
 * @brief Get human-readable description of I/O control result
 */
std::string describe_io_result(const IOControlResult& result);

} // namespace io
} // namespace uds
