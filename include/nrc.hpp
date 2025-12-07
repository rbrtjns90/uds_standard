#pragma once
/**
 * @file nrc.hpp
 * @brief Negative Response Codes (NRC) - ISO 14229-1:2013 Annex A
 * 
 * ISO 14229-1:2013 Annex A (pp. 325-332) defines all NRCs.
 * Table A.1 (pp. 325-332) provides the complete NRC list.
 * 
 * Negative Response Message Format (Section 8.4, p. 34):
 *   [0x7F] [Request SID] [NRC]
 * 
 * NRC Ranges (Table A.1):
 *   0x00:       Reserved (positive response indicator)
 *   0x01-0x0F:  ISO Reserved
 *   0x10-0x2F:  General NRCs
 *   0x30:       ISO Reserved
 *   0x31-0x33:  Request/security NRCs
 *   0x34:       ISO Reserved
 *   0x35-0x37:  Security NRCs
 *   0x38-0x6F:  ISO Reserved
 *   0x70-0x78:  Upload/download NRCs
 *   0x79-0x7D:  ISO Reserved
 *   0x7E-0x7F:  Session NRCs
 *   0x80-0xFF:  Vehicle manufacturer specific
 */

#include <cstdint>
#include <string>
#include <optional>
#include <vector>

namespace nrc {

// ============================================================================
// Negative Response Codes (NRC)
// ISO 14229-1:2013 Annex A, Table A.1 (pp. 325-332)
// ============================================================================

/**
 * @brief Negative Response Codes per ISO 14229-1:2013 Table A.1
 * 
 * CRITICAL NRCs for implementation:
 * - 0x78: ResponsePending - Wait P2* and retry receive
 * - 0x21: BusyRepeatRequest - Retry after P2
 * - 0x73: WrongBlockSequenceCounter - Retry same block
 */
enum class Code : uint8_t {
    // === General NRCs (0x10-0x14) - Table A.1, p. 325 ===
    PositiveResponse                            = 0x00,  ///< Not an NRC, used for comparison
    GeneralReject                               = 0x10,  ///< Table A.1 - General rejection
    ServiceNotSupported                         = 0x11,  ///< Table A.1 - SID not implemented
    SubFunctionNotSupported                     = 0x12,  ///< Table A.1 - Sub-function invalid
    IncorrectMessageLength                      = 0x13,  ///< Table A.1 - Wrong message length
    IncorrectMessageLengthOrInvalidFormat       = 0x13,  ///< Alias for 0x13
    ResponseTooLong                             = 0x14,  ///< Table A.1 - Response exceeds buffer
    
    // === Busy NRCs (0x21) - Table A.1, p. 326 ===
    BusyRepeatRequest                           = 0x21,  ///< Table A.1 - Server busy, retry after P2
    
    // === Condition NRCs (0x22-0x26) - Table A.1, p. 326-327 ===
    ConditionsNotCorrect                        = 0x22,  ///< Table A.1 - Preconditions not met
    RequestSequenceError                        = 0x24,  ///< Table A.1 - Wrong request order
    NoResponseFromSubnetComponent               = 0x25,  ///< Table A.1 - Gateway timeout
    FailurePreventsExecutionOfRequestedAction   = 0x26,  ///< Table A.1 - Execution blocked
    
    // === Range NRCs (0x31) - Table A.1, p. 327 ===
    RequestOutOfRange                           = 0x31,  ///< Table A.1 - Parameter out of range
    
    // === Security NRCs (0x33-0x37) - Table A.1, p. 328 ===
    SecurityAccessDenied                        = 0x33,  ///< Table A.1 - Security not unlocked
    InvalidKey                                  = 0x35,  ///< Table A.1 - Wrong key sent
    ExceededNumberOfAttempts                    = 0x36,  ///< Table A.1 - Too many failed attempts
    RequiredTimeDelayNotExpired                 = 0x37,  ///< Table A.1 - Security lockout active
    
    // === Upload/Download NRCs (0x70-0x73) - Table A.1, p. 329-330 ===
    UploadDownloadNotAccepted                   = 0x70,  ///< Table A.1 - Transfer rejected
    TransferDataSuspended                       = 0x71,  ///< Table A.1 - Transfer paused
    GeneralProgrammingFailure                   = 0x72,  ///< Table A.1 - Flash write failed
    WrongBlockSequenceCounter                   = 0x73,  ///< Table A.1 - Block counter mismatch
    WrongBlockSequenceCounter_Alias             = 0x76,  ///< Some OEMs use 0x76
    
    // === Response Pending (0x78) - Table A.1, p. 330 ===
    // CRITICAL: When received, wait P2* timeout and listen for actual response
    RequestCorrectlyReceivedResponsePending     = 0x78,  ///< Table A.1 - Wait for P2* timeout
    RequestCorrectlyReceived_RP                 = 0x78,  ///< Alias for 0x78
    
    // === Session NRCs (0x7E-0x7F) - Table A.1, p. 331 ===
    SubFunctionNotSupportedInActiveSession      = 0x7E,  ///< Table A.1 - Sub-func needs different session
    ServiceNotSupportedInActiveSession          = 0x7F,  ///< Table A.1 - Service needs different session
    
    // === Vehicle Manufacturer Specific (0x80-0xFF) - Table A.1, p. 331-332 ===
    // Common OEM-defined conditions (not standardized, examples only)
    RpmTooHigh                                  = 0x81,  ///< OEM - Engine RPM too high
    RpmTooLow                                   = 0x82,  ///< OEM - Engine RPM too low
    EngineIsRunning                             = 0x83,  ///< OEM - Engine must be off
    EngineIsNotRunning                          = 0x84,  ///< OEM - Engine must be running
    EngineRunTimeTooLow                         = 0x85,  ///< OEM - Engine not warmed up
    TemperatureTooHigh                          = 0x86,  ///< OEM - Temperature exceeded
    TemperatureTooLow                           = 0x87,  ///< OEM - Temperature too low
    VehicleSpeedTooHigh                         = 0x88,  ///< OEM - Vehicle moving too fast
    VehicleSpeedTooLow                          = 0x89,  ///< OEM - Vehicle too slow
    ThrottlePedalTooHigh                        = 0x8A,  ///< OEM - Throttle pressed
    ThrottlePedalTooLow                         = 0x8B,  ///< OEM - Throttle released
    TransmissionRangeNotInNeutral               = 0x8C,  ///< OEM - Must be in neutral
    TransmissionRangeNotInGear                  = 0x8D,  ///< OEM - Must be in gear
    BrakeSwitchNotClosed                        = 0x8F,  ///< OEM - Brake not pressed
    ShifterLeverNotInPark                       = 0x90,  ///< OEM - Must be in park
    TorqueConverterClutchLocked                 = 0x91,  ///< OEM - TC clutch engaged
    VoltageTooHigh                              = 0x92,  ///< OEM - Battery overvoltage
    VoltageTooLow                               = 0x93   ///< OEM - Battery undervoltage
};

// ============================================================================
// Action to take based on NRC
// ============================================================================

enum class Action {
    Abort,              // Unrecoverable error, stop immediately
    Retry,              // Retry the request immediately
    Wait,               // Wait before continuing (use specific timeout)
    WaitAndRetry,       // Wait then retry the same request
    ContinuePending,    // Continue waiting for response (NRC 0x78)
    Unsupported         // Service/sub-function not supported
};

// ============================================================================
// NRC Category Classification
// ============================================================================

enum class Category {
    GeneralReject,      // Unrecoverable errors
    Busy,               // ECU is busy, retry may succeed
    ConditionsNotMet,   // Preconditions not satisfied
    SecurityIssue,      // Security access problems
    ProgrammingError,   // Flash/programming errors
    SessionIssue,       // Wrong diagnostic session
    VehicleCondition,   // Vehicle state not suitable
    ResponsePending,    // Long operation in progress
    Unknown
};

// ============================================================================
// NRC Interpreter - Provides detailed information about negative responses
// ============================================================================

class Interpreter {
public:
    Interpreter() = default;
    
    // ========================================================================
    // Primary API - Action-based decision making
    // ========================================================================
    
    /// Get recommended action for NRC (used by Client::exchange)
    Action action(Code c) const;
    
    /// Get human-readable description of NRC
    std::string description(Code c) const;
    
    // ========================================================================
    // Static helper methods
    // ========================================================================
    
    /// Get human-readable description of NRC (static version)
    static std::string get_description(Code nrc);
    
    /// Get NRC category for decision making
    static Category get_category(Code nrc);
    
    /// Check if NRC is recoverable (retry may succeed)
    static bool is_recoverable(Code nrc);
    
    /// Check if NRC indicates need for extended timeout
    static bool needs_extended_timeout(Code nrc);
    
    /// Get recommended action for NRC (static version)
    static Action get_action(Code nrc);
    
    /// Get recommended action string
    static std::string get_recommended_action(Code nrc);
    
    /// Check if NRC is response pending (0x78)
    static bool is_response_pending(Code nrc) {
        return nrc == Code::RequestCorrectlyReceivedResponsePending ||
               nrc == Code::RequestCorrectlyReceived_RP;
    }
    
    /// Check if NRC is security-related
    static bool is_security_error(Code nrc);
    
    /// Check if NRC is programming-related
    static bool is_programming_error(Code nrc);
    
    /// Check if NRC requires different session
    static bool is_session_error(Code nrc);
    
    /// Parse NRC from response bytes
    static std::optional<Code> parse_from_response(const std::vector<uint8_t>& response);
    
    /// Format NRC for logging (e.g., "0x22: ConditionsNotCorrect")
    static std::string format_for_log(Code nrc);
};

} // namespace nrc
