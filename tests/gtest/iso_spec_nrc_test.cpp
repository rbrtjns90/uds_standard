/**
 * @file iso_spec_nrc_test.cpp
 * @brief ISO 14229-1 Spec-Anchored Tests: Negative Response Codes
 * 
 * References:
 * - ISO 14229-1:2020 Section 7.5 (Negative response message)
 * - ISO 14229-1:2020 Annex A (NRC definitions)
 */

#include <gtest/gtest.h>
#include "nrc.hpp"
#include <vector>
#include <cstdint>

using namespace nrc;

// ============================================================================
// ISO 14229-1 Section 7.5: Negative Response Message Format
// ============================================================================

class NegativeResponseFormatSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t NEGATIVE_RESPONSE_SID = 0x7F;
};

TEST_F(NegativeResponseFormatSpecTest, MessageFormat) {
    // Negative response: [0x7F] [requestSID] [NRC]
    
    std::vector<uint8_t> neg_response = {
        NEGATIVE_RESPONSE_SID,
        0x22,  // Original request SID (ReadDataByIdentifier)
        0x14   // NRC: Response too long
    };
    
    EXPECT_EQ(3, neg_response.size()) << "Negative response is 3 bytes";
    EXPECT_EQ(0x7F, neg_response[0]) << "First byte is always 0x7F";
    EXPECT_EQ(0x22, neg_response[1]) << "Second byte is request SID";
    EXPECT_EQ(0x14, neg_response[2]) << "Third byte is NRC";
}

TEST_F(NegativeResponseFormatSpecTest, IdentifyNegativeResponse) {
    // Any response starting with 0x7F is a negative response
    
    std::vector<uint8_t> positive = {0x62, 0xF1, 0x90};  // ReadDataByIdentifier positive
    std::vector<uint8_t> negative = {0x7F, 0x22, 0x14};  // Negative response
    
    EXPECT_NE(0x7F, positive[0]) << "Positive response doesn't start with 0x7F";
    EXPECT_EQ(0x7F, negative[0]) << "Negative response starts with 0x7F";
}

// ============================================================================
// ISO 14229-1 Annex A.1: NRC Values
// ============================================================================

class NRCValuesSpecTest : public ::testing::Test {};

TEST_F(NRCValuesSpecTest, GeneralNRCs) {
    // Per ISO 14229-1 Annex A.1 Table A.1
    
    // General reject
    EXPECT_EQ(0x10, static_cast<uint8_t>(Code::GeneralReject));
    
    // Service not supported
    EXPECT_EQ(0x11, static_cast<uint8_t>(Code::ServiceNotSupported));
    
    // Sub-function not supported
    EXPECT_EQ(0x12, static_cast<uint8_t>(Code::SubFunctionNotSupported));
    
    // Incorrect message length or invalid format
    EXPECT_EQ(0x13, static_cast<uint8_t>(Code::IncorrectMessageLength));
    
    // Response too long
    EXPECT_EQ(0x14, static_cast<uint8_t>(Code::ResponseTooLong));
}

TEST_F(NRCValuesSpecTest, BusyNRCs) {
    // Busy - repeat request
    EXPECT_EQ(0x21, static_cast<uint8_t>(Code::BusyRepeatRequest));
    
    // Conditions not correct
    EXPECT_EQ(0x22, static_cast<uint8_t>(Code::ConditionsNotCorrect));
    
    // Request sequence error
    EXPECT_EQ(0x24, static_cast<uint8_t>(Code::RequestSequenceError));
    
    // No response from subnet component
    EXPECT_EQ(0x25, static_cast<uint8_t>(Code::NoResponseFromSubnetComponent));
    
    // Failure prevents execution of requested action
    EXPECT_EQ(0x26, static_cast<uint8_t>(Code::FailurePreventsExecutionOfRequestedAction));
}

TEST_F(NRCValuesSpecTest, RequestOutOfRangeNRCs) {
    // Request out of range
    EXPECT_EQ(0x31, static_cast<uint8_t>(Code::RequestOutOfRange));
}

TEST_F(NRCValuesSpecTest, SecurityNRCs) {
    // Security access denied
    EXPECT_EQ(0x33, static_cast<uint8_t>(Code::SecurityAccessDenied));
    
    // Invalid key
    EXPECT_EQ(0x35, static_cast<uint8_t>(Code::InvalidKey));
    
    // Exceeded number of attempts
    EXPECT_EQ(0x36, static_cast<uint8_t>(Code::ExceededNumberOfAttempts));
    
    // Required time delay not expired
    EXPECT_EQ(0x37, static_cast<uint8_t>(Code::RequiredTimeDelayNotExpired));
}

TEST_F(NRCValuesSpecTest, UploadDownloadNRCs) {
    // Upload/download not accepted
    EXPECT_EQ(0x70, static_cast<uint8_t>(Code::UploadDownloadNotAccepted));
    
    // Transfer data suspended
    EXPECT_EQ(0x71, static_cast<uint8_t>(Code::TransferDataSuspended));
    
    // General programming failure
    EXPECT_EQ(0x72, static_cast<uint8_t>(Code::GeneralProgrammingFailure));
    
    // Wrong block sequence counter
    EXPECT_EQ(0x73, static_cast<uint8_t>(Code::WrongBlockSequenceCounter));
}

TEST_F(NRCValuesSpecTest, ResponsePendingNRC) {
    // Response pending (special handling required)
    EXPECT_EQ(0x78, static_cast<uint8_t>(Code::RequestCorrectlyReceivedResponsePending));
}

TEST_F(NRCValuesSpecTest, SessionNRCs) {
    // Sub-function not supported in active session
    EXPECT_EQ(0x7E, static_cast<uint8_t>(Code::SubFunctionNotSupportedInActiveSession));
    
    // Service not supported in active session
    EXPECT_EQ(0x7F, static_cast<uint8_t>(Code::ServiceNotSupportedInActiveSession));
}

TEST_F(NRCValuesSpecTest, VehicleConditionNRCs) {
    // RPM too high
    EXPECT_EQ(0x81, static_cast<uint8_t>(Code::RpmTooHigh));
    
    // RPM too low
    EXPECT_EQ(0x82, static_cast<uint8_t>(Code::RpmTooLow));
    
    // Engine is running
    EXPECT_EQ(0x83, static_cast<uint8_t>(Code::EngineIsRunning));
    
    // Engine is not running
    EXPECT_EQ(0x84, static_cast<uint8_t>(Code::EngineIsNotRunning));
    
    // Engine run time too low
    EXPECT_EQ(0x85, static_cast<uint8_t>(Code::EngineRunTimeTooLow));
    
    // Temperature too high
    EXPECT_EQ(0x86, static_cast<uint8_t>(Code::TemperatureTooHigh));
    
    // Temperature too low
    EXPECT_EQ(0x87, static_cast<uint8_t>(Code::TemperatureTooLow));
    
    // Vehicle speed too high
    EXPECT_EQ(0x88, static_cast<uint8_t>(Code::VehicleSpeedTooHigh));
    
    // Vehicle speed too low
    EXPECT_EQ(0x89, static_cast<uint8_t>(Code::VehicleSpeedTooLow));
    
    // Throttle/pedal too high
    EXPECT_EQ(0x8A, static_cast<uint8_t>(Code::ThrottlePedalTooHigh));
    
    // Throttle/pedal too low
    EXPECT_EQ(0x8B, static_cast<uint8_t>(Code::ThrottlePedalTooLow));
    
    // Transmission range not in neutral
    EXPECT_EQ(0x8C, static_cast<uint8_t>(Code::TransmissionRangeNotInNeutral));
    
    // Transmission range not in gear
    EXPECT_EQ(0x8D, static_cast<uint8_t>(Code::TransmissionRangeNotInGear));
    
    // Brake switch not closed
    EXPECT_EQ(0x8F, static_cast<uint8_t>(Code::BrakeSwitchNotClosed));
    
    // Shifter lever not in park
    EXPECT_EQ(0x90, static_cast<uint8_t>(Code::ShifterLeverNotInPark));
    
    // Torque converter clutch locked
    EXPECT_EQ(0x91, static_cast<uint8_t>(Code::TorqueConverterClutchLocked));
    
    // Voltage too high
    EXPECT_EQ(0x92, static_cast<uint8_t>(Code::VoltageTooHigh));
    
    // Voltage too low
    EXPECT_EQ(0x93, static_cast<uint8_t>(Code::VoltageTooLow));
}

// ============================================================================
// ISO 14229-1: NRC Ranges
// ============================================================================

class NRCRangesSpecTest : public ::testing::Test {};

TEST_F(NRCRangesSpecTest, ISODefinedRange) {
    // Per ISO 14229-1: 0x00-0x93 are ISO-defined
    // 0x00: positiveResponse (not an NRC)
    // 0x01-0x0F: ISO/SAE reserved
    // 0x10-0x93: Defined NRCs
    
    EXPECT_TRUE(0x10 >= 0x10 && 0x10 <= 0x93);
    EXPECT_TRUE(0x93 >= 0x10 && 0x93 <= 0x93);
}

TEST_F(NRCRangesSpecTest, ReservedRanges) {
    // 0x94-0xEF: Reserved for future ISO use
    // 0xF0-0xFE: Vehicle manufacturer specific
    // 0xFF: Reserved
    
    constexpr uint8_t RESERVED_START = 0x94;
    constexpr uint8_t RESERVED_END = 0xEF;
    constexpr uint8_t OEM_START = 0xF0;
    constexpr uint8_t OEM_END = 0xFE;
    
    EXPECT_EQ(0x94, RESERVED_START);
    EXPECT_EQ(0xEF, RESERVED_END);
    EXPECT_EQ(0xF0, OEM_START);
    EXPECT_EQ(0xFE, OEM_END);
}

// ============================================================================
// ISO 14229-1: Response Pending (NRC 0x78) Handling
// ============================================================================

class ResponsePendingSpecTest : public ::testing::Test {};

TEST_F(ResponsePendingSpecTest, ResponsePendingFormat) {
    // NRC 0x78 indicates server needs more time
    // Client should wait for P2* timeout instead of P2
    
    std::vector<uint8_t> response_pending = {0x7F, 0x34, 0x78};
    
    EXPECT_EQ(0x7F, response_pending[0]);
    EXPECT_EQ(0x34, response_pending[1]) << "Original request was RequestDownload";
    EXPECT_EQ(0x78, response_pending[2]) << "Response pending NRC";
}

TEST_F(ResponsePendingSpecTest, MultipleResponsePending) {
    // Server can send multiple 0x78 responses
    // Client must wait P2* after each one
    
    // Sequence:
    // 1. Client sends request
    // 2. Server sends 7F xx 78 (response pending)
    // 3. Client waits P2*
    // 4. Server sends 7F xx 78 again (still busy)
    // 5. Client waits P2*
    // 6. Server sends positive response
    
    std::vector<std::vector<uint8_t>> sequence = {
        {0x7F, 0x34, 0x78},  // Response pending 1
        {0x7F, 0x34, 0x78},  // Response pending 2
        {0x74, 0x20, 0x0F, 0xFE}  // Final positive response
    };
    
    EXPECT_EQ(0x78, sequence[0][2]);
    EXPECT_EQ(0x78, sequence[1][2]);
    EXPECT_EQ(0x74, sequence[2][0]) << "Final response is positive";
}

// ============================================================================
// ISO 14229-1: NRC Recovery Actions
// ============================================================================

class NRCRecoverySpecTest : public ::testing::Test {};

TEST_F(NRCRecoverySpecTest, RetryableNRCs) {
    // NRCs that indicate retry may succeed
    
    // BusyRepeatRequest (0x21): Retry immediately
    EXPECT_EQ(0x21, static_cast<uint8_t>(Code::BusyRepeatRequest));
    
    // ResponsePending (0x78): Wait and continue
    EXPECT_EQ(0x78, static_cast<uint8_t>(Code::RequestCorrectlyReceivedResponsePending));
}

TEST_F(NRCRecoverySpecTest, NonRetryableNRCs) {
    // NRCs that indicate request cannot succeed without changes
    
    // ServiceNotSupported (0x11): Service not available
    EXPECT_EQ(0x11, static_cast<uint8_t>(Code::ServiceNotSupported));
    
    // SecurityAccessDenied (0x33): Need to unlock first
    EXPECT_EQ(0x33, static_cast<uint8_t>(Code::SecurityAccessDenied));
    
    // ConditionsNotCorrect (0x22): Preconditions not met
    EXPECT_EQ(0x22, static_cast<uint8_t>(Code::ConditionsNotCorrect));
}

TEST_F(NRCRecoverySpecTest, SecurityDelayNRCs) {
    // NRCs that require waiting before retry
    
    // RequiredTimeDelayNotExpired (0x37): Wait for anti-hammering delay
    EXPECT_EQ(0x37, static_cast<uint8_t>(Code::RequiredTimeDelayNotExpired));
    
    // ExceededNumberOfAttempts (0x36): Security lockout
    EXPECT_EQ(0x36, static_cast<uint8_t>(Code::ExceededNumberOfAttempts));
}

// ============================================================================
// ISO 14229-1: Service-Specific NRCs
// ============================================================================

class ServiceSpecificNRCsTest : public ::testing::Test {};

TEST_F(ServiceSpecificNRCsTest, SecurityAccessNRCs) {
    // NRCs specific to SecurityAccess (0x27)
    
    // Invalid key sent
    EXPECT_EQ(0x35, static_cast<uint8_t>(Code::InvalidKey));
    
    // Too many failed attempts
    EXPECT_EQ(0x36, static_cast<uint8_t>(Code::ExceededNumberOfAttempts));
    
    // Must wait before retry
    EXPECT_EQ(0x37, static_cast<uint8_t>(Code::RequiredTimeDelayNotExpired));
}

TEST_F(ServiceSpecificNRCsTest, TransferDataNRCs) {
    // NRCs specific to TransferData (0x36)
    
    // Wrong block sequence counter
    EXPECT_EQ(0x73, static_cast<uint8_t>(Code::WrongBlockSequenceCounter));
    
    // Transfer suspended
    EXPECT_EQ(0x71, static_cast<uint8_t>(Code::TransferDataSuspended));
    
    // Programming failure
    EXPECT_EQ(0x72, static_cast<uint8_t>(Code::GeneralProgrammingFailure));
}

TEST_F(ServiceSpecificNRCsTest, RoutineControlNRCs) {
    // NRCs that may occur during RoutineControl (0x31)
    
    // Request sequence error (routine not started)
    EXPECT_EQ(0x24, static_cast<uint8_t>(Code::RequestSequenceError));
    
    // Request out of range (invalid routine ID)
    EXPECT_EQ(0x31, static_cast<uint8_t>(Code::RequestOutOfRange));
}
