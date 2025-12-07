/**
 * @file nrc_functions_test.cpp
 * @brief Comprehensive tests for NRC functions (nrc.cpp)
 */

#include <gtest/gtest.h>
#include "nrc.hpp"

using namespace nrc;

// ============================================================================
// Action Tests
// ============================================================================

TEST(NRCActionTest, ResponsePending) {
  EXPECT_EQ(Interpreter::get_action(Code::RequestCorrectlyReceivedResponsePending), 
            Action::ContinuePending);
}

TEST(NRCActionTest, BusyRepeatRequest) {
  EXPECT_EQ(Interpreter::get_action(Code::BusyRepeatRequest), Action::WaitAndRetry);
}

TEST(NRCActionTest, RequiredTimeDelay) {
  EXPECT_EQ(Interpreter::get_action(Code::RequiredTimeDelayNotExpired), Action::Wait);
}

TEST(NRCActionTest, RetryableErrors) {
  EXPECT_EQ(Interpreter::get_action(Code::WrongBlockSequenceCounter), Action::Retry);
  EXPECT_EQ(Interpreter::get_action(Code::TransferDataSuspended), Action::Retry);
}

TEST(NRCActionTest, UnsupportedService) {
  EXPECT_EQ(Interpreter::get_action(Code::ServiceNotSupported), Action::Unsupported);
  EXPECT_EQ(Interpreter::get_action(Code::SubFunctionNotSupported), Action::Unsupported);
}

TEST(NRCActionTest, AbortErrors) {
  EXPECT_EQ(Interpreter::get_action(Code::GeneralReject), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::IncorrectMessageLength), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::RequestOutOfRange), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::InvalidKey), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::ExceededNumberOfAttempts), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::GeneralProgrammingFailure), Action::Abort);
}

TEST(NRCActionTest, ConditionErrors) {
  EXPECT_EQ(Interpreter::get_action(Code::ConditionsNotCorrect), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::RequestSequenceError), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::SecurityAccessDenied), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::UploadDownloadNotAccepted), Action::Abort);
}

TEST(NRCActionTest, InstanceMethod) {
  Interpreter interp;
  EXPECT_EQ(interp.action(Code::BusyRepeatRequest), Action::WaitAndRetry);
}

// ============================================================================
// Description Tests
// ============================================================================

TEST(NRCDescriptionTest, KnownCodes) {
  EXPECT_EQ(Interpreter::get_description(Code::PositiveResponse), "Positive Response");
  EXPECT_EQ(Interpreter::get_description(Code::GeneralReject), "General Reject");
  EXPECT_EQ(Interpreter::get_description(Code::ServiceNotSupported), "Service Not Supported");
  EXPECT_EQ(Interpreter::get_description(Code::SubFunctionNotSupported), "Sub-Function Not Supported");
  EXPECT_EQ(Interpreter::get_description(Code::IncorrectMessageLength), "Incorrect Message Length or Invalid Format");
}

TEST(NRCDescriptionTest, SecurityCodes) {
  EXPECT_EQ(Interpreter::get_description(Code::SecurityAccessDenied), "Security Access Denied");
  EXPECT_EQ(Interpreter::get_description(Code::InvalidKey), "Invalid Key");
  EXPECT_EQ(Interpreter::get_description(Code::ExceededNumberOfAttempts), "Exceeded Number Of Attempts");
  EXPECT_EQ(Interpreter::get_description(Code::RequiredTimeDelayNotExpired), "Required Time Delay Not Expired");
}

TEST(NRCDescriptionTest, ProgrammingCodes) {
  EXPECT_EQ(Interpreter::get_description(Code::UploadDownloadNotAccepted), "Upload/Download Not Accepted");
  EXPECT_EQ(Interpreter::get_description(Code::TransferDataSuspended), "Transfer Data Suspended");
  EXPECT_EQ(Interpreter::get_description(Code::GeneralProgrammingFailure), "General Programming Failure");
  EXPECT_EQ(Interpreter::get_description(Code::WrongBlockSequenceCounter), "Wrong Block Sequence Counter");
}

TEST(NRCDescriptionTest, VehicleConditionCodes) {
  EXPECT_EQ(Interpreter::get_description(Code::RpmTooHigh), "RPM Too High");
  EXPECT_EQ(Interpreter::get_description(Code::EngineIsRunning), "Engine Is Running");
  EXPECT_EQ(Interpreter::get_description(Code::VehicleSpeedTooHigh), "Vehicle Speed Too High");
  EXPECT_EQ(Interpreter::get_description(Code::VoltageTooLow), "Voltage Too Low");
}

TEST(NRCDescriptionTest, UnknownCode) {
  EXPECT_EQ(Interpreter::get_description(static_cast<Code>(0xFE)), "Unknown NRC");
}

TEST(NRCDescriptionTest, InstanceMethod) {
  Interpreter interp;
  EXPECT_EQ(interp.description(Code::GeneralReject), "General Reject");
}

// ============================================================================
// Category Tests
// ============================================================================

TEST(NRCCategoryTest, ResponsePending) {
  EXPECT_EQ(Interpreter::get_category(Code::RequestCorrectlyReceivedResponsePending), 
            Category::ResponsePending);
}

TEST(NRCCategoryTest, Busy) {
  EXPECT_EQ(Interpreter::get_category(Code::BusyRepeatRequest), Category::Busy);
}

TEST(NRCCategoryTest, ConditionsNotMet) {
  EXPECT_EQ(Interpreter::get_category(Code::ConditionsNotCorrect), Category::ConditionsNotMet);
  EXPECT_EQ(Interpreter::get_category(Code::RequestSequenceError), Category::ConditionsNotMet);
}

TEST(NRCCategoryTest, SecurityIssue) {
  EXPECT_EQ(Interpreter::get_category(Code::SecurityAccessDenied), Category::SecurityIssue);
  EXPECT_EQ(Interpreter::get_category(Code::InvalidKey), Category::SecurityIssue);
  EXPECT_EQ(Interpreter::get_category(Code::ExceededNumberOfAttempts), Category::SecurityIssue);
  EXPECT_EQ(Interpreter::get_category(Code::RequiredTimeDelayNotExpired), Category::SecurityIssue);
}

TEST(NRCCategoryTest, ProgrammingError) {
  EXPECT_EQ(Interpreter::get_category(Code::UploadDownloadNotAccepted), Category::ProgrammingError);
  EXPECT_EQ(Interpreter::get_category(Code::TransferDataSuspended), Category::ProgrammingError);
  EXPECT_EQ(Interpreter::get_category(Code::GeneralProgrammingFailure), Category::ProgrammingError);
  EXPECT_EQ(Interpreter::get_category(Code::WrongBlockSequenceCounter), Category::ProgrammingError);
}

TEST(NRCCategoryTest, SessionIssue) {
  EXPECT_EQ(Interpreter::get_category(Code::SubFunctionNotSupportedInActiveSession), Category::SessionIssue);
  EXPECT_EQ(Interpreter::get_category(Code::ServiceNotSupportedInActiveSession), Category::SessionIssue);
}

TEST(NRCCategoryTest, VehicleCondition) {
  EXPECT_EQ(Interpreter::get_category(Code::RpmTooHigh), Category::VehicleCondition);
  EXPECT_EQ(Interpreter::get_category(Code::RpmTooLow), Category::VehicleCondition);
  EXPECT_EQ(Interpreter::get_category(Code::EngineIsRunning), Category::VehicleCondition);
  EXPECT_EQ(Interpreter::get_category(Code::EngineIsNotRunning), Category::VehicleCondition);
  EXPECT_EQ(Interpreter::get_category(Code::TemperatureTooHigh), Category::VehicleCondition);
  EXPECT_EQ(Interpreter::get_category(Code::TemperatureTooLow), Category::VehicleCondition);
  EXPECT_EQ(Interpreter::get_category(Code::VehicleSpeedTooHigh), Category::VehicleCondition);
  EXPECT_EQ(Interpreter::get_category(Code::VehicleSpeedTooLow), Category::VehicleCondition);
  EXPECT_EQ(Interpreter::get_category(Code::VoltageTooHigh), Category::VehicleCondition);
  EXPECT_EQ(Interpreter::get_category(Code::VoltageTooLow), Category::VehicleCondition);
}

TEST(NRCCategoryTest, GeneralReject) {
  EXPECT_EQ(Interpreter::get_category(Code::GeneralReject), Category::GeneralReject);
  EXPECT_EQ(Interpreter::get_category(Code::ServiceNotSupported), Category::GeneralReject);
  EXPECT_EQ(Interpreter::get_category(Code::SubFunctionNotSupported), Category::GeneralReject);
  EXPECT_EQ(Interpreter::get_category(Code::IncorrectMessageLength), Category::GeneralReject);
  EXPECT_EQ(Interpreter::get_category(Code::RequestOutOfRange), Category::GeneralReject);
}

TEST(NRCCategoryTest, Unknown) {
  EXPECT_EQ(Interpreter::get_category(static_cast<Code>(0xFE)), Category::Unknown);
}

// ============================================================================
// Helper Method Tests
// ============================================================================

TEST(NRCHelperTest, IsRecoverable) {
  EXPECT_TRUE(Interpreter::is_recoverable(Code::BusyRepeatRequest));
  EXPECT_TRUE(Interpreter::is_recoverable(Code::RequestCorrectlyReceivedResponsePending));
  EXPECT_TRUE(Interpreter::is_recoverable(Code::WrongBlockSequenceCounter));
  EXPECT_TRUE(Interpreter::is_recoverable(Code::TransferDataSuspended));
  
  EXPECT_FALSE(Interpreter::is_recoverable(Code::GeneralReject));
  EXPECT_FALSE(Interpreter::is_recoverable(Code::ServiceNotSupported));
  EXPECT_FALSE(Interpreter::is_recoverable(Code::InvalidKey));
}

TEST(NRCHelperTest, NeedsExtendedTimeout) {
  EXPECT_TRUE(Interpreter::needs_extended_timeout(Code::RequestCorrectlyReceivedResponsePending));
  EXPECT_FALSE(Interpreter::needs_extended_timeout(Code::GeneralReject));
  EXPECT_FALSE(Interpreter::needs_extended_timeout(Code::BusyRepeatRequest));
}

TEST(NRCHelperTest, IsSecurityError) {
  EXPECT_TRUE(Interpreter::is_security_error(Code::SecurityAccessDenied));
  EXPECT_TRUE(Interpreter::is_security_error(Code::InvalidKey));
  EXPECT_TRUE(Interpreter::is_security_error(Code::ExceededNumberOfAttempts));
  EXPECT_TRUE(Interpreter::is_security_error(Code::RequiredTimeDelayNotExpired));
  
  EXPECT_FALSE(Interpreter::is_security_error(Code::GeneralReject));
  EXPECT_FALSE(Interpreter::is_security_error(Code::GeneralProgrammingFailure));
}

TEST(NRCHelperTest, IsProgrammingError) {
  EXPECT_TRUE(Interpreter::is_programming_error(Code::UploadDownloadNotAccepted));
  EXPECT_TRUE(Interpreter::is_programming_error(Code::TransferDataSuspended));
  EXPECT_TRUE(Interpreter::is_programming_error(Code::GeneralProgrammingFailure));
  EXPECT_TRUE(Interpreter::is_programming_error(Code::WrongBlockSequenceCounter));
  
  EXPECT_FALSE(Interpreter::is_programming_error(Code::SecurityAccessDenied));
  EXPECT_FALSE(Interpreter::is_programming_error(Code::GeneralReject));
}

TEST(NRCHelperTest, IsSessionError) {
  EXPECT_TRUE(Interpreter::is_session_error(Code::SubFunctionNotSupportedInActiveSession));
  EXPECT_TRUE(Interpreter::is_session_error(Code::ServiceNotSupportedInActiveSession));
  
  EXPECT_FALSE(Interpreter::is_session_error(Code::GeneralReject));
  EXPECT_FALSE(Interpreter::is_session_error(Code::SecurityAccessDenied));
}

TEST(NRCHelperTest, IsResponsePending) {
  EXPECT_TRUE(Interpreter::is_response_pending(Code::RequestCorrectlyReceivedResponsePending));
  EXPECT_FALSE(Interpreter::is_response_pending(Code::GeneralReject));
  EXPECT_FALSE(Interpreter::is_response_pending(Code::BusyRepeatRequest));
}

// ============================================================================
// Recommended Action Tests
// ============================================================================

TEST(NRCRecommendedActionTest, Abort) {
  auto action = Interpreter::get_recommended_action(Code::GeneralReject);
  EXPECT_NE(action.find("Abort"), std::string::npos);
}

TEST(NRCRecommendedActionTest, Retry) {
  auto action = Interpreter::get_recommended_action(Code::WrongBlockSequenceCounter);
  EXPECT_NE(action.find("Retry"), std::string::npos);
}

TEST(NRCRecommendedActionTest, Wait) {
  auto action = Interpreter::get_recommended_action(Code::RequiredTimeDelayNotExpired);
  EXPECT_NE(action.find("Wait"), std::string::npos);
}

TEST(NRCRecommendedActionTest, WaitAndRetry) {
  auto action = Interpreter::get_recommended_action(Code::BusyRepeatRequest);
  EXPECT_NE(action.find("retry"), std::string::npos);
}

TEST(NRCRecommendedActionTest, ContinuePending) {
  auto action = Interpreter::get_recommended_action(Code::RequestCorrectlyReceivedResponsePending);
  EXPECT_NE(action.find("Continue"), std::string::npos);
}

TEST(NRCRecommendedActionTest, Unsupported) {
  auto action = Interpreter::get_recommended_action(Code::ServiceNotSupported);
  EXPECT_NE(action.find("not supported"), std::string::npos);
}

// ============================================================================
// Parse From Response Tests
// ============================================================================

TEST(NRCParseTest, ValidNegativeResponse) {
  std::vector<uint8_t> response = {0x7F, 0x22, 0x33};  // SecurityAccessDenied
  
  auto nrc = Interpreter::parse_from_response(response);
  
  EXPECT_TRUE(nrc.has_value());
  EXPECT_EQ(nrc.value(), Code::SecurityAccessDenied);
}

TEST(NRCParseTest, PositiveResponse) {
  std::vector<uint8_t> response = {0x62, 0xF1, 0x90, 'V', 'I', 'N'};
  
  auto nrc = Interpreter::parse_from_response(response);
  
  EXPECT_FALSE(nrc.has_value());
}

TEST(NRCParseTest, TooShort) {
  std::vector<uint8_t> response = {0x7F, 0x22};  // Missing NRC byte
  
  auto nrc = Interpreter::parse_from_response(response);
  
  EXPECT_FALSE(nrc.has_value());
}

TEST(NRCParseTest, Empty) {
  std::vector<uint8_t> response;
  
  auto nrc = Interpreter::parse_from_response(response);
  
  EXPECT_FALSE(nrc.has_value());
}

TEST(NRCParseTest, AllNRCCodes) {
  // Test parsing various NRC codes
  std::vector<std::pair<uint8_t, Code>> test_cases = {
    {0x10, Code::GeneralReject},
    {0x11, Code::ServiceNotSupported},
    {0x12, Code::SubFunctionNotSupported},
    {0x33, Code::SecurityAccessDenied},
    {0x35, Code::InvalidKey},
    {0x78, Code::RequestCorrectlyReceivedResponsePending},
  };
  
  for (const auto& [nrc_byte, expected_code] : test_cases) {
    std::vector<uint8_t> response = {0x7F, 0x22, nrc_byte};
    auto nrc = Interpreter::parse_from_response(response);
    EXPECT_TRUE(nrc.has_value());
    EXPECT_EQ(nrc.value(), expected_code);
  }
}

// ============================================================================
// Format For Log Tests
// ============================================================================

TEST(NRCFormatTest, ContainsHexCode) {
  auto log = Interpreter::format_for_log(Code::SecurityAccessDenied);
  
  EXPECT_NE(log.find("33"), std::string::npos);  // 0x33
}

TEST(NRCFormatTest, ContainsDescription) {
  auto log = Interpreter::format_for_log(Code::SecurityAccessDenied);
  
  EXPECT_NE(log.find("Security Access Denied"), std::string::npos);
}

TEST(NRCFormatTest, Format0x) {
  auto log = Interpreter::format_for_log(Code::GeneralReject);
  
  EXPECT_NE(log.find("0x"), std::string::npos);
}

TEST(NRCFormatTest, VariousCodes) {
  // Just verify formatting doesn't crash for various codes
  EXPECT_FALSE(Interpreter::format_for_log(Code::PositiveResponse).empty());
  EXPECT_FALSE(Interpreter::format_for_log(Code::GeneralReject).empty());
  EXPECT_FALSE(Interpreter::format_for_log(Code::BusyRepeatRequest).empty());
  EXPECT_FALSE(Interpreter::format_for_log(Code::RequestCorrectlyReceivedResponsePending).empty());
  EXPECT_FALSE(Interpreter::format_for_log(Code::VoltageTooLow).empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
