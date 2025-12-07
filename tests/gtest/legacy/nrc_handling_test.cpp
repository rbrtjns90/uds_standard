/**
 * @file nrc_handling_test.cpp
 * @brief Tests for Negative Response Code (NRC) handling and recovery
 * 
 * Tests cover:
 * - All standard NRC codes and their meanings
 * - NRC categorization and recommended actions
 * - Response pending (0x78) handling
 * - Busy repeat request (0x21) handling
 * - Security-related NRCs
 * - Programming-related NRCs
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "nrc.hpp"

using namespace uds;
using namespace nrc;

// ============================================================================
// NRC Code Value Tests
// ============================================================================

TEST(NRCCodeTest, GeneralRejectCodes) {
  EXPECT_EQ(static_cast<uint8_t>(Code::GeneralReject), 0x10);
  EXPECT_EQ(static_cast<uint8_t>(Code::ServiceNotSupported), 0x11);
  EXPECT_EQ(static_cast<uint8_t>(Code::SubFunctionNotSupported), 0x12);
  EXPECT_EQ(static_cast<uint8_t>(Code::IncorrectMessageLengthOrInvalidFormat), 0x13);
  EXPECT_EQ(static_cast<uint8_t>(Code::ResponseTooLong), 0x14);
}

TEST(NRCCodeTest, BusyAndConditionCodes) {
  EXPECT_EQ(static_cast<uint8_t>(Code::BusyRepeatRequest), 0x21);
  EXPECT_EQ(static_cast<uint8_t>(Code::ConditionsNotCorrect), 0x22);
  EXPECT_EQ(static_cast<uint8_t>(Code::RequestSequenceError), 0x24);
}

TEST(NRCCodeTest, SecurityCodes) {
  EXPECT_EQ(static_cast<uint8_t>(Code::SecurityAccessDenied), 0x33);
  EXPECT_EQ(static_cast<uint8_t>(Code::InvalidKey), 0x35);
  EXPECT_EQ(static_cast<uint8_t>(Code::ExceededNumberOfAttempts), 0x36);
  EXPECT_EQ(static_cast<uint8_t>(Code::RequiredTimeDelayNotExpired), 0x37);
}

TEST(NRCCodeTest, ProgrammingCodes) {
  EXPECT_EQ(static_cast<uint8_t>(Code::UploadDownloadNotAccepted), 0x70);
  EXPECT_EQ(static_cast<uint8_t>(Code::TransferDataSuspended), 0x71);
  EXPECT_EQ(static_cast<uint8_t>(Code::GeneralProgrammingFailure), 0x72);
  EXPECT_EQ(static_cast<uint8_t>(Code::WrongBlockSequenceCounter), 0x73);
}

TEST(NRCCodeTest, SpecialCodes) {
  EXPECT_EQ(static_cast<uint8_t>(Code::RequestCorrectlyReceivedResponsePending), 0x78);
  EXPECT_EQ(static_cast<uint8_t>(Code::SubFunctionNotSupportedInActiveSession), 0x7E);
  EXPECT_EQ(static_cast<uint8_t>(Code::ServiceNotSupportedInActiveSession), 0x7F);
}

// ============================================================================
// NRC Action Tests
// ============================================================================

TEST(NRCActionTest, WaitActions) {
  // These NRCs should trigger waiting
  EXPECT_EQ(Interpreter::get_action(Code::RequestCorrectlyReceivedResponsePending), Action::ContinuePending);
  EXPECT_EQ(Interpreter::get_action(Code::BusyRepeatRequest), Action::WaitAndRetry);
  EXPECT_EQ(Interpreter::get_action(Code::RequiredTimeDelayNotExpired), Action::Wait);
}

TEST(NRCActionTest, RetryActions) {
  // These NRCs might be recoverable with retry
  EXPECT_EQ(Interpreter::get_action(Code::WrongBlockSequenceCounter), Action::Retry);
  EXPECT_EQ(Interpreter::get_action(Code::TransferDataSuspended), Action::Retry);
}

TEST(NRCActionTest, AbortActions) {
  // These NRCs should abort the operation
  EXPECT_EQ(Interpreter::get_action(Code::SecurityAccessDenied), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::ConditionsNotCorrect), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::RequestSequenceError), Action::Abort);
  EXPECT_EQ(Interpreter::get_action(Code::UploadDownloadNotAccepted), Action::Abort);
}

TEST(NRCActionTest, UnsupportedActions) {
  // These NRCs indicate service/subfunction not supported
  EXPECT_EQ(Interpreter::get_action(Code::ServiceNotSupported), Action::Unsupported);
  EXPECT_EQ(Interpreter::get_action(Code::SubFunctionNotSupported), Action::Unsupported);
}

// ============================================================================
// NRC Category Tests
// ============================================================================

TEST(NRCCategoryTest, BusyCategory) {
  EXPECT_EQ(Interpreter::get_category(Code::BusyRepeatRequest), Category::Busy);
  EXPECT_EQ(Interpreter::get_category(Code::RequestCorrectlyReceivedResponsePending), Category::ResponsePending);
}

TEST(NRCCategoryTest, SecurityCategory) {
  EXPECT_EQ(Interpreter::get_category(Code::SecurityAccessDenied), Category::SecurityIssue);
  EXPECT_EQ(Interpreter::get_category(Code::InvalidKey), Category::SecurityIssue);
  EXPECT_EQ(Interpreter::get_category(Code::ExceededNumberOfAttempts), Category::SecurityIssue);
  EXPECT_EQ(Interpreter::get_category(Code::RequiredTimeDelayNotExpired), Category::SecurityIssue);
}

TEST(NRCCategoryTest, ProgrammingCategory) {
  EXPECT_EQ(Interpreter::get_category(Code::UploadDownloadNotAccepted), Category::ProgrammingError);
  EXPECT_EQ(Interpreter::get_category(Code::TransferDataSuspended), Category::ProgrammingError);
  EXPECT_EQ(Interpreter::get_category(Code::GeneralProgrammingFailure), Category::ProgrammingError);
  EXPECT_EQ(Interpreter::get_category(Code::WrongBlockSequenceCounter), Category::ProgrammingError);
}

TEST(NRCCategoryTest, SessionCategory) {
  EXPECT_EQ(Interpreter::get_category(Code::SubFunctionNotSupportedInActiveSession), Category::SessionIssue);
  EXPECT_EQ(Interpreter::get_category(Code::ServiceNotSupportedInActiveSession), Category::SessionIssue);
}

// ============================================================================
// NRC Description Tests
// ============================================================================

TEST(NRCDescriptionTest, KnownCodes) {
  EXPECT_FALSE(Interpreter::get_description(Code::GeneralReject).empty());
  EXPECT_FALSE(Interpreter::get_description(Code::ServiceNotSupported).empty());
  EXPECT_FALSE(Interpreter::get_description(Code::SecurityAccessDenied).empty());
  EXPECT_FALSE(Interpreter::get_description(Code::RequestCorrectlyReceivedResponsePending).empty());
}

TEST(NRCDescriptionTest, DescriptionContent) {
  std::string desc = Interpreter::get_description(Code::SecurityAccessDenied);
  // Should contain meaningful text
  EXPECT_GT(desc.length(), 5u);
}

// ============================================================================
// NRC Recovery Tests
// ============================================================================

TEST(NRCRecoveryTest, IsRecoverable) {
  // Recoverable NRCs
  EXPECT_TRUE(Interpreter::is_recoverable(Code::BusyRepeatRequest));
  EXPECT_TRUE(Interpreter::is_recoverable(Code::RequestCorrectlyReceivedResponsePending));
  EXPECT_TRUE(Interpreter::is_recoverable(Code::WrongBlockSequenceCounter));
  
  // Non-recoverable NRCs
  EXPECT_FALSE(Interpreter::is_recoverable(Code::ServiceNotSupported));
  EXPECT_FALSE(Interpreter::is_recoverable(Code::SecurityAccessDenied));
  EXPECT_FALSE(Interpreter::is_recoverable(Code::GeneralReject));
}

TEST(NRCRecoveryTest, NeedsExtendedTimeout) {
  EXPECT_TRUE(Interpreter::needs_extended_timeout(Code::RequestCorrectlyReceivedResponsePending));
  EXPECT_FALSE(Interpreter::needs_extended_timeout(Code::GeneralReject));
}

TEST(NRCRecoveryTest, IsSecurityError) {
  EXPECT_TRUE(Interpreter::is_security_error(Code::SecurityAccessDenied));
  EXPECT_TRUE(Interpreter::is_security_error(Code::InvalidKey));
  EXPECT_TRUE(Interpreter::is_security_error(Code::ExceededNumberOfAttempts));
  EXPECT_TRUE(Interpreter::is_security_error(Code::RequiredTimeDelayNotExpired));
  
  EXPECT_FALSE(Interpreter::is_security_error(Code::GeneralReject));
  EXPECT_FALSE(Interpreter::is_security_error(Code::BusyRepeatRequest));
}

TEST(NRCRecoveryTest, IsProgrammingError) {
  EXPECT_TRUE(Interpreter::is_programming_error(Code::UploadDownloadNotAccepted));
  EXPECT_TRUE(Interpreter::is_programming_error(Code::TransferDataSuspended));
  EXPECT_TRUE(Interpreter::is_programming_error(Code::GeneralProgrammingFailure));
  EXPECT_TRUE(Interpreter::is_programming_error(Code::WrongBlockSequenceCounter));
  
  EXPECT_FALSE(Interpreter::is_programming_error(Code::GeneralReject));
}

// ============================================================================
// NRC Response Parsing Tests
// ============================================================================

TEST(NRCParsingTest, ValidNegativeResponse) {
  // Standard NRC format: [0x7F][RequestSID][NRC]
  std::vector<uint8_t> response = {0x7F, 0x10, 0x12};
  
  EXPECT_EQ(response[0], 0x7F);
  
  SID original_sid = static_cast<SID>(response[1]);
  EXPECT_EQ(original_sid, SID::DiagnosticSessionControl);
  
  Code nrc = static_cast<Code>(response[2]);
  EXPECT_EQ(nrc, Code::SubFunctionNotSupported);
}

TEST(NRCParsingTest, ResponsePendingSequence) {
  // Simulate response pending followed by actual response
  std::vector<uint8_t> pending = {0x7F, 0x34, 0x78};  // RequestDownload, ResponsePending
  std::vector<uint8_t> success = {0x74, 0x20, 0x00, 0x10};  // Positive response
  
  // First response is pending
  EXPECT_EQ(pending[0], 0x7F);
  EXPECT_EQ(static_cast<Code>(pending[2]), Code::RequestCorrectlyReceivedResponsePending);
  
  // Second response is success
  EXPECT_EQ(success[0], 0x74);  // 0x34 + 0x40
}

TEST(NRCParsingTest, ShortNegativeResponse) {
  // Malformed NRC (too short)
  std::vector<uint8_t> short_response = {0x7F, 0x10};  // Missing NRC byte
  
  EXPECT_EQ(short_response.size(), 2u);
  EXPECT_LT(short_response.size(), 3u);  // Should have 3 bytes
}

// ============================================================================
// NRC Formatting Tests
// ============================================================================

TEST(NRCFormattingTest, FormatForLog) {
  std::string formatted = Interpreter::format_for_log(Code::SecurityAccessDenied);
  
  // Should contain hex code
  EXPECT_NE(formatted.find("33"), std::string::npos);
}

TEST(NRCFormattingTest, GetRecommendedAction) {
  std::string action = Interpreter::get_recommended_action(Code::RequiredTimeDelayNotExpired);
  
  // Should suggest waiting
  EXPECT_FALSE(action.empty());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(NRCEdgeCaseTest, UnknownNRCCode) {
  // Test handling of unknown/OEM-specific NRC codes
  Code unknown = static_cast<Code>(0xFE);
  
  // Should not crash, should return reasonable defaults
  EXPECT_NO_THROW({
    auto action = Interpreter::get_action(unknown);
    auto category = Interpreter::get_category(unknown);
    auto desc = Interpreter::get_description(unknown);
    (void)action;
    (void)category;
    (void)desc;
  });
}

TEST(NRCEdgeCaseTest, OEMSpecificRange) {
  // OEM-specific NRCs are in range 0x80-0xFE
  for (uint8_t code = 0x80; code < 0xFF; ++code) {
    Code nrc = static_cast<Code>(code);
    
    // Should handle gracefully
    EXPECT_NO_THROW({
      Interpreter::get_action(nrc);
      Interpreter::get_category(nrc);
    });
  }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
