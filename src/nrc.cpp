#include "nrc.hpp"
#include <unordered_map>
#include <sstream>
#include <iomanip>

namespace nrc {

// ============================================================================
// Action determination
// ============================================================================

Action Interpreter::action(Code c) const {
    return get_action(c);
}

Action Interpreter::get_action(Code nrc) {
    switch (nrc) {
        // Response Pending - continue waiting with extended timeout
        case Code::RequestCorrectlyReceivedResponsePending:
            return Action::ContinuePending;
        
        // Busy - retry after brief delay
        case Code::BusyRepeatRequest:
            return Action::WaitAndRetry;
        
        // Security delay required - wait specified time
        case Code::RequiredTimeDelayNotExpired:
            return Action::Wait;
        
        // Retryable errors
        case Code::WrongBlockSequenceCounter:
        case Code::WrongBlockSequenceCounter_Alias:
        case Code::TransferDataSuspended:
            return Action::Retry;
        
        // Unsupported service/sub-function
        case Code::ServiceNotSupported:
        case Code::SubFunctionNotSupported:
            return Action::Unsupported;
        
        // Unrecoverable errors - abort
        case Code::GeneralReject:
        case Code::IncorrectMessageLength:
        case Code::RequestOutOfRange:
        case Code::InvalidKey:
        case Code::ExceededNumberOfAttempts:
        case Code::GeneralProgrammingFailure:
            return Action::Abort;
        
        // Conditions not met - abort (user must fix preconditions)
        case Code::ConditionsNotCorrect:
        case Code::RequestSequenceError:
        case Code::SecurityAccessDenied:
        case Code::UploadDownloadNotAccepted:
            return Action::Abort;
        
        // Default: abort on unknown NRC
        default:
            return Action::Abort;
    }
}

// ============================================================================
// Description strings
// ============================================================================

std::string Interpreter::description(Code c) const {
    return get_description(c);
}

std::string Interpreter::get_description(Code nrc) {
    static const std::unordered_map<uint8_t, const char*> descriptions = {
        {0x00, "Positive Response"},
        {0x10, "General Reject"},
        {0x11, "Service Not Supported"},
        {0x12, "Sub-Function Not Supported"},
        {0x13, "Incorrect Message Length or Invalid Format"},
        {0x14, "Response Too Long"},
        {0x21, "Busy - Repeat Request"},
        {0x22, "Conditions Not Correct"},
        {0x24, "Request Sequence Error"},
        {0x25, "No Response From Subnet Component"},
        {0x26, "Failure Prevents Execution of Requested Action"},
        {0x31, "Request Out Of Range"},
        {0x33, "Security Access Denied"},
        {0x35, "Invalid Key"},
        {0x36, "Exceeded Number Of Attempts"},
        {0x37, "Required Time Delay Not Expired"},
        {0x70, "Upload/Download Not Accepted"},
        {0x71, "Transfer Data Suspended"},
        {0x72, "General Programming Failure"},
        {0x73, "Wrong Block Sequence Counter"},
        {0x76, "Wrong Block Sequence Counter (Alt)"},
        {0x78, "Request Correctly Received - Response Pending"},
        {0x7E, "Sub-Function Not Supported In Active Session"},
        {0x7F, "Service Not Supported In Active Session"},
        {0x81, "RPM Too High"},
        {0x82, "RPM Too Low"},
        {0x83, "Engine Is Running"},
        {0x84, "Engine Is Not Running"},
        {0x85, "Engine Run Time Too Low"},
        {0x86, "Temperature Too High"},
        {0x87, "Temperature Too Low"},
        {0x88, "Vehicle Speed Too High"},
        {0x89, "Vehicle Speed Too Low"},
        {0x8A, "Throttle/Pedal Too High"},
        {0x8B, "Throttle/Pedal Too Low"},
        {0x8C, "Transmission Range Not In Neutral"},
        {0x8D, "Transmission Range Not In Gear"},
        {0x8F, "Brake Switch(es) Not Closed"},
        {0x90, "Shifter Lever Not In Park"},
        {0x91, "Torque Converter Clutch Locked"},
        {0x92, "Voltage Too High"},
        {0x93, "Voltage Too Low"}
    };
    
    auto it = descriptions.find(static_cast<uint8_t>(nrc));
    if (it != descriptions.end()) {
        return it->second;
    }
    
    return "Unknown NRC";
}

// ============================================================================
// Category classification
// ============================================================================

Category Interpreter::get_category(Code nrc) {
    switch (nrc) {
        case Code::RequestCorrectlyReceivedResponsePending:
            return Category::ResponsePending;
        
        case Code::BusyRepeatRequest:
            return Category::Busy;
        
        case Code::ConditionsNotCorrect:
        case Code::RequestSequenceError:
            return Category::ConditionsNotMet;
        
        case Code::SecurityAccessDenied:
        case Code::InvalidKey:
        case Code::ExceededNumberOfAttempts:
        case Code::RequiredTimeDelayNotExpired:
            return Category::SecurityIssue;
        
        case Code::UploadDownloadNotAccepted:
        case Code::TransferDataSuspended:
        case Code::GeneralProgrammingFailure:
        case Code::WrongBlockSequenceCounter:
        case Code::WrongBlockSequenceCounter_Alias:
            return Category::ProgrammingError;
        
        case Code::SubFunctionNotSupportedInActiveSession:
        case Code::ServiceNotSupportedInActiveSession:
            return Category::SessionIssue;
        
        case Code::RpmTooHigh:
        case Code::RpmTooLow:
        case Code::EngineIsRunning:
        case Code::EngineIsNotRunning:
        case Code::TemperatureTooHigh:
        case Code::TemperatureTooLow:
        case Code::VehicleSpeedTooHigh:
        case Code::VehicleSpeedTooLow:
        case Code::ThrottlePedalTooHigh:
        case Code::ThrottlePedalTooLow:
        case Code::TransmissionRangeNotInNeutral:
        case Code::TransmissionRangeNotInGear:
        case Code::BrakeSwitchNotClosed:
        case Code::ShifterLeverNotInPark:
        case Code::TorqueConverterClutchLocked:
        case Code::VoltageTooHigh:
        case Code::VoltageTooLow:
            return Category::VehicleCondition;
        
        case Code::GeneralReject:
        case Code::ServiceNotSupported:
        case Code::SubFunctionNotSupported:
        case Code::IncorrectMessageLength:
        case Code::RequestOutOfRange:
            return Category::GeneralReject;
        
        default:
            return Category::Unknown;
    }
}

// ============================================================================
// Helper methods
// ============================================================================

bool Interpreter::is_recoverable(Code nrc) {
    Action act = get_action(nrc);
    return act == Action::Retry || 
           act == Action::WaitAndRetry || 
           act == Action::ContinuePending;
}

bool Interpreter::needs_extended_timeout(Code nrc) {
    return is_response_pending(nrc);
}

std::string Interpreter::get_recommended_action(Code nrc) {
    Action act = get_action(nrc);
    
    switch (act) {
        case Action::Abort:
            return "Abort request - unrecoverable error";
        case Action::Retry:
            return "Retry request immediately";
        case Action::Wait:
            return "Wait for specified delay then continue";
        case Action::WaitAndRetry:
            return "Wait briefly then retry request";
        case Action::ContinuePending:
            return "Continue waiting with extended timeout (P2*)";
        case Action::Unsupported:
            return "Service/sub-function not supported";
        default:
            return "Unknown action";
    }
}

bool Interpreter::is_security_error(Code nrc) {
    return get_category(nrc) == Category::SecurityIssue;
}

bool Interpreter::is_programming_error(Code nrc) {
    return get_category(nrc) == Category::ProgrammingError;
}

bool Interpreter::is_session_error(Code nrc) {
    return get_category(nrc) == Category::SessionIssue;
}

std::optional<Code> Interpreter::parse_from_response(const std::vector<uint8_t>& response) {
    // Negative response format: [0x7F, SID, NRC]
    if (response.size() >= 3 && response[0] == 0x7F) {
        return static_cast<Code>(response[2]);
    }
    
    // Not a negative response
    return std::nullopt;
}

std::string Interpreter::format_for_log(Code nrc) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
        << static_cast<int>(nrc) << ": " << get_description(nrc);
    return oss.str();
}

} // namespace nrc
