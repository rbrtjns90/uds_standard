/**
 * @file uds_core_test.cpp
 * @brief Comprehensive Google Test suite for core UDS functionality (uds.hpp, nrc.hpp)
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "nrc.hpp"

using namespace uds;

// ############################################################################
// UDS.HPP TESTS
// ############################################################################

// ============================================================================
// AddressType Tests
// ============================================================================

TEST(AddressTypeTest, Values) {
  EXPECT_EQ(static_cast<uint8_t>(AddressType::Physical), 0);
  EXPECT_EQ(static_cast<uint8_t>(AddressType::Functional), 1);
}

// ============================================================================
// Address Tests
// ============================================================================

TEST(AddressTest, DefaultConstruction) {
  Address addr;
  
  EXPECT_EQ(addr.type, AddressType::Physical);
  EXPECT_EQ(addr.tx_can_id, 0u);
  EXPECT_EQ(addr.rx_can_id, 0u);
}

TEST(AddressTest, OBDIIAddressing) {
  Address addr;
  addr.type = AddressType::Physical;
  addr.tx_can_id = 0x7E0;  // Tester -> ECU
  addr.rx_can_id = 0x7E8;  // ECU -> Tester
  
  EXPECT_EQ(addr.tx_can_id, 0x7E0u);
  EXPECT_EQ(addr.rx_can_id, 0x7E8u);
  EXPECT_EQ(addr.rx_can_id - addr.tx_can_id, 8u);  // Standard offset
}

TEST(AddressTest, FunctionalAddressing) {
  Address addr;
  addr.type = AddressType::Functional;
  addr.tx_can_id = 0x7DF;  // Broadcast
  
  EXPECT_EQ(addr.type, AddressType::Functional);
  EXPECT_EQ(addr.tx_can_id, 0x7DFu);
}

// ============================================================================
// PDU Tests
// ============================================================================

TEST(PDUTest, DefaultConstruction) {
  PDU pdu;
  EXPECT_TRUE(pdu.bytes.empty());
}

TEST(PDUTest, WithData) {
  PDU pdu;
  pdu.bytes = {0x22, 0xF1, 0x90};  // ReadDataByIdentifier for VIN
  
  EXPECT_EQ(pdu.bytes.size(), 3u);
  EXPECT_EQ(pdu.bytes[0], 0x22);  // SID
}

// ============================================================================
// Timings Tests
// ============================================================================

TEST(TimingsTest, DefaultValues) {
  Timings t;
  
  EXPECT_EQ(t.p2.count(), 50);       // 50ms default
  EXPECT_EQ(t.p2_star.count(), 5000); // 5000ms default
  EXPECT_EQ(t.req_gap.count(), 0);
}

TEST(TimingsTest, CustomValues) {
  Timings t;
  t.p2 = std::chrono::milliseconds(100);
  t.p2_star = std::chrono::milliseconds(10000);
  t.req_gap = std::chrono::milliseconds(10);
  
  EXPECT_EQ(t.p2.count(), 100);
  EXPECT_EQ(t.p2_star.count(), 10000);
  EXPECT_EQ(t.req_gap.count(), 10);
}

// ============================================================================
// SID Tests
// ============================================================================

TEST(SIDTest, DiagnosticManagementServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::DiagnosticSessionControl), 0x10);
  EXPECT_EQ(static_cast<uint8_t>(SID::ECUReset), 0x11);
  EXPECT_EQ(static_cast<uint8_t>(SID::TesterPresent), 0x3E);
}

TEST(SIDTest, SecurityServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::SecurityAccess), 0x27);
  EXPECT_EQ(static_cast<uint8_t>(SID::CommunicationControl), 0x28);
  EXPECT_EQ(static_cast<uint8_t>(SID::Authentication), 0x29);
}

TEST(SIDTest, DataServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadDataByIdentifier), 0x22);
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadMemoryByAddress), 0x23);
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadScalingDataByIdentifier), 0x24);
  EXPECT_EQ(static_cast<uint8_t>(SID::WriteDataByIdentifier), 0x2E);
  EXPECT_EQ(static_cast<uint8_t>(SID::WriteMemoryByAddress), 0x3D);
}

TEST(SIDTest, DTCServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::ClearDiagnosticInformation), 0x14);
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadDTCInformation), 0x19);
  EXPECT_EQ(static_cast<uint8_t>(SID::ControlDTCSetting), 0x85);
}

TEST(SIDTest, PeriodicDynamicServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadDataByPeriodicIdentifier), 0x2A);
  EXPECT_EQ(static_cast<uint8_t>(SID::DynamicallyDefineDataIdentifier), 0x2C);
}

TEST(SIDTest, IOAndRoutineServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::InputOutputControlByIdentifier), 0x2F);
  EXPECT_EQ(static_cast<uint8_t>(SID::RoutineControl), 0x31);
}

TEST(SIDTest, UploadDownloadServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::RequestDownload), 0x34);
  EXPECT_EQ(static_cast<uint8_t>(SID::RequestUpload), 0x35);
  EXPECT_EQ(static_cast<uint8_t>(SID::TransferData), 0x36);
  EXPECT_EQ(static_cast<uint8_t>(SID::RequestTransferExit), 0x37);
}

TEST(SIDTest, RemoteActivationServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::AccessTimingParameters), 0x83);
  EXPECT_EQ(static_cast<uint8_t>(SID::SecuredDataTransmission), 0x84);
  EXPECT_EQ(static_cast<uint8_t>(SID::ResponseOnEvent), 0x86);
  EXPECT_EQ(static_cast<uint8_t>(SID::LinkControl), 0x87);
}

TEST(SIDTest, PositiveResponseCalculation) {
  EXPECT_EQ(kPositiveResponseOffset, 0x40);
  EXPECT_EQ(static_cast<uint8_t>(SID::DiagnosticSessionControl) + kPositiveResponseOffset, 0x50);
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadDataByIdentifier) + kPositiveResponseOffset, 0x62);
  EXPECT_EQ(static_cast<uint8_t>(SID::SecurityAccess) + kPositiveResponseOffset, 0x67);
  EXPECT_EQ(static_cast<uint8_t>(SID::RequestDownload) + kPositiveResponseOffset, 0x74);
}

// ============================================================================
// Session Tests
// ============================================================================

TEST(SessionTest, SessionValues) {
  EXPECT_EQ(static_cast<uint8_t>(Session::DefaultSession), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(Session::ProgrammingSession), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(Session::ExtendedSession), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(Session::SafetySystemSession), 0x04);
}

TEST(SessionTest, OEMSessionRange) {
  // OEM sessions are 0x40-0x5F
  uint8_t oem_session_start = 0x40;
  uint8_t oem_session_end = 0x5F;
  
  EXPECT_GT(oem_session_start, static_cast<uint8_t>(Session::SafetySystemSession));
  EXPECT_EQ(oem_session_end - oem_session_start + 1, 32);  // 32 OEM sessions
}

// ============================================================================
// ECUReset Type Tests
// ============================================================================

TEST(EcuResetTest, ResetTypeValues) {
  EXPECT_EQ(static_cast<uint8_t>(EcuResetType::HardReset), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(EcuResetType::KeyOffOnReset), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(EcuResetType::SoftReset), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(EcuResetType::EnableRapidPowerShut), 0x04);
  EXPECT_EQ(static_cast<uint8_t>(EcuResetType::DisableRapidPowerShut), 0x05);
}

// ============================================================================
// CommunicationControl Tests
// ============================================================================

TEST(CommunicationControlTest, ControlTypeValues) {
  EXPECT_EQ(static_cast<uint8_t>(CommunicationControlType::EnableRxAndTx), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(CommunicationControlType::EnableRxDisableTx), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(CommunicationControlType::DisableRxEnableTx), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(CommunicationControlType::DisableRxAndTx), 0x03);
}

TEST(CommunicationControlTest, CommunicationTypeValues) {
  EXPECT_EQ(static_cast<uint8_t>(CommunicationType::NormalCommunicationMessages), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(CommunicationType::NetworkManagementMessages), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(CommunicationType::NetworkDownloadUpload), 0x03);
}

// ============================================================================
// RoutineControl Tests
// ============================================================================

TEST(RoutineControlTest, ActionValues) {
  EXPECT_EQ(static_cast<uint8_t>(RoutineAction::Start), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(RoutineAction::Stop), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(RoutineAction::Result), 0x03);
}

// ============================================================================
// DTCSetting Tests
// ============================================================================

TEST(DTCSettingTest, SettingTypeValues) {
  EXPECT_EQ(static_cast<uint8_t>(DTCSettingType::On), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(DTCSettingType::Off), 0x02);
}

// ============================================================================
// AccessTimingParameters Tests
// ============================================================================

TEST(AccessTimingTest, TypeValues) {
  EXPECT_EQ(static_cast<uint8_t>(AccessTimingParametersType::ReadExtendedTimingParameterSet), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(AccessTimingParametersType::SetTimingParametersToDefaultValues), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(AccessTimingParametersType::ReadCurrentlyActiveTimingParameters), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(AccessTimingParametersType::SetTimingParametersToGivenValues), 0x04);
}

// ============================================================================
// PeriodicTransmissionMode Tests
// ============================================================================

TEST(PeriodicTransmissionTest, ModeValues) {
  EXPECT_EQ(static_cast<uint8_t>(PeriodicTransmissionMode::SendAtSlowRate), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(PeriodicTransmissionMode::SendAtMediumRate), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(PeriodicTransmissionMode::SendAtFastRate), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(PeriodicTransmissionMode::StopSending), 0x04);
}

// ============================================================================
// DDDI SubFunction Tests
// ============================================================================

TEST(DDDITest, SubFunctionValues) {
  EXPECT_EQ(static_cast<uint8_t>(DDDISubFunction::DefineByIdentifier), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(DDDISubFunction::DefineByMemoryAddress), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(DDDISubFunction::ClearDynamicallyDefinedDataIdentifier), 0x03);
}

// ============================================================================
// NegativeResponseCode Tests (uds.hpp version)
// ============================================================================

TEST(NRCTest, GeneralNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::GeneralReject), 0x10);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::ServiceNotSupported), 0x11);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::SubFunctionNotSupported), 0x12);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::IncorrectMessageLengthOrFormat), 0x13);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::ResponseTooLong), 0x14);
}

TEST(NRCTest, BusyAndConditionNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::BusyRepeatRequest), 0x21);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::ConditionsNotCorrect), 0x22);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::RequestSequenceError), 0x24);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::RequestOutOfRange), 0x31);
}

TEST(NRCTest, SecurityNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::SecurityAccessDenied), 0x33);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::InvalidKey), 0x35);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::ExceededNumberOfAttempts), 0x36);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::RequiredTimeDelayNotExpired), 0x37);
}

TEST(NRCTest, ProgrammingNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::UploadDownloadNotAccepted), 0x70);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::TransferDataSuspended), 0x71);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::GeneralProgrammingFailure), 0x72);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::WrongBlockSequenceCounter), 0x73);
}

TEST(NRCTest, SessionNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::RequestCorrectlyReceived_ResponsePending), 0x78);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::SubFunctionNotSupportedInActiveSession), 0x7E);
  EXPECT_EQ(static_cast<uint8_t>(NegativeResponseCode::ServiceNotSupportedInActiveSession), 0x7F);
}

// ============================================================================
// Positive Response Detection Tests
// ============================================================================

TEST(ResponseTest, IsPositiveResponse) {
  EXPECT_TRUE(is_positive_response(0x50, 0x10));   // DSC
  EXPECT_TRUE(is_positive_response(0x62, 0x22));   // RDBI
  EXPECT_TRUE(is_positive_response(0x67, 0x27));   // SA
  EXPECT_TRUE(is_positive_response(0x74, 0x34));   // RequestDownload
  EXPECT_TRUE(is_positive_response(0x76, 0x36));   // TransferData
}

TEST(ResponseTest, IsNotPositiveResponse) {
  EXPECT_FALSE(is_positive_response(0x7F, 0x22));  // Negative response
  EXPECT_FALSE(is_positive_response(0x63, 0x22));  // Wrong positive
  EXPECT_FALSE(is_positive_response(0x22, 0x22));  // Echo (not positive)
}

// ============================================================================
// Codec Tests
// ============================================================================

TEST(CodecTest, BigEndian16) {
  std::vector<uint8_t> buf;
  codec::be16(buf, 0x1234);
  
  ASSERT_EQ(buf.size(), 2u);
  EXPECT_EQ(buf[0], 0x12);
  EXPECT_EQ(buf[1], 0x34);
}

TEST(CodecTest, BigEndian16Zero) {
  std::vector<uint8_t> buf;
  codec::be16(buf, 0x0000);
  
  ASSERT_EQ(buf.size(), 2u);
  EXPECT_EQ(buf[0], 0x00);
  EXPECT_EQ(buf[1], 0x00);
}

TEST(CodecTest, BigEndian16Max) {
  std::vector<uint8_t> buf;
  codec::be16(buf, 0xFFFF);
  
  ASSERT_EQ(buf.size(), 2u);
  EXPECT_EQ(buf[0], 0xFF);
  EXPECT_EQ(buf[1], 0xFF);
}

TEST(CodecTest, BigEndian24) {
  std::vector<uint8_t> buf;
  codec::be24(buf, 0x123456);
  
  ASSERT_EQ(buf.size(), 3u);
  EXPECT_EQ(buf[0], 0x12);
  EXPECT_EQ(buf[1], 0x34);
  EXPECT_EQ(buf[2], 0x56);
}

TEST(CodecTest, BigEndian32) {
  std::vector<uint8_t> buf;
  codec::be32(buf, 0x12345678);
  
  ASSERT_EQ(buf.size(), 4u);
  EXPECT_EQ(buf[0], 0x12);
  EXPECT_EQ(buf[1], 0x34);
  EXPECT_EQ(buf[2], 0x56);
  EXPECT_EQ(buf[3], 0x78);
}

TEST(CodecTest, MultipleAppends) {
  std::vector<uint8_t> buf;
  codec::be16(buf, 0xF190);  // DID
  codec::be32(buf, 0x12345678);  // Data
  
  ASSERT_EQ(buf.size(), 6u);
  EXPECT_EQ(buf[0], 0xF1);
  EXPECT_EQ(buf[1], 0x90);
  EXPECT_EQ(buf[2], 0x12);
}

// ============================================================================
// Request/Response Structure Tests
// ============================================================================

TEST(StructTest, NegativeResponseDefault) {
  NegativeResponse nr;
  // Just verify it can be default constructed
  EXPECT_TRUE(true);
}

TEST(StructTest, PositiveOrNegativeDefault) {
  PositiveOrNegative pn;
  
  EXPECT_FALSE(pn.ok);
  EXPECT_TRUE(pn.payload.empty());
}

TEST(StructTest, DSCRequest) {
  DSC_Request req;
  req.session = Session::ExtendedSession;
  
  EXPECT_EQ(req.session, Session::ExtendedSession);
}

TEST(StructTest, DSCResponse) {
  DSC_Response resp;
  resp.session = Session::ExtendedSession;
  resp.params = {0x00, 0x19, 0x01, 0xF4};  // P2=25ms, P2*=500ms
  
  EXPECT_EQ(resp.session, Session::ExtendedSession);
  EXPECT_EQ(resp.params.size(), 4u);
}

TEST(StructTest, TesterPresentRequest) {
  TesterPresent_Request req;
  
  EXPECT_TRUE(req.suppress_response);  // Default
  
  req.suppress_response = false;
  EXPECT_FALSE(req.suppress_response);
}

TEST(StructTest, SecurityAccessStructs) {
  SecurityAccess_RequestSeed seed_req;
  seed_req.level = 0x01;
  EXPECT_EQ(seed_req.level, 0x01);
  
  SecurityAccess_SendKey key_req;
  key_req.level = 0x02;
  key_req.key = {0x12, 0x34, 0x56, 0x78};
  EXPECT_EQ(key_req.level, 0x02);
  EXPECT_EQ(key_req.key.size(), 4u);
  
  SecurityAccess_SeedResp seed_resp;
  seed_resp.seed = {0xAB, 0xCD, 0xEF, 0x01};
  EXPECT_EQ(seed_resp.seed.size(), 4u);
}

TEST(StructTest, ReadWriteDIDStructs) {
  ReadDID_Request read_req;
  read_req.did = 0xF190;
  EXPECT_EQ(read_req.did, 0xF190);
  
  ReadDID_Response read_resp;
  read_resp.did = 0xF190;
  read_resp.data = {'V', 'I', 'N', '1', '2', '3'};
  EXPECT_EQ(read_resp.data.size(), 6u);
  
  WriteDID_Request write_req;
  write_req.did = 0xF190;
  write_req.data = {'N', 'E', 'W', 'V', 'I', 'N'};
  EXPECT_EQ(write_req.data.size(), 6u);
}

TEST(StructTest, RoutineControlStructs) {
  RoutineControl_Request req;
  req.action = RoutineAction::Start;
  req.id = 0xFF00;
  req.optRecord = {0x01, 0x02};
  
  EXPECT_EQ(req.action, RoutineAction::Start);
  EXPECT_EQ(req.id, 0xFF00);
  
  RoutineControl_Response resp;
  resp.action = RoutineAction::Result;
  resp.id = 0xFF00;
  resp.resultRecord = {0x00};  // Success
  EXPECT_EQ(resp.resultRecord.size(), 1u);
}

TEST(StructTest, TransferDataStructs) {
  RequestDownload_Request dl_req;
  dl_req.dataFormatId = 0x00;
  dl_req.memoryAddress = {0x00, 0x01, 0x00, 0x00};
  dl_req.memorySize = {0x00, 0x01, 0x00, 0x00};
  
  EXPECT_EQ(dl_req.dataFormatId, 0x00);
  EXPECT_EQ(dl_req.memoryAddress.size(), 4u);
  
  TransferData_Request td_req;
  td_req.block = 0x01;
  td_req.data = {0xDE, 0xAD, 0xBE, 0xEF};
  EXPECT_EQ(td_req.block, 0x01);
}

// ============================================================================
// DID Tests
// ============================================================================

TEST(DIDTest, DIDEncoding) {
  std::vector<uint8_t> buf;
  codec::be16(buf, 0xF190);  // VIN DID
  
  ASSERT_EQ(buf.size(), 2u);
  EXPECT_EQ(buf[0], 0xF1);
  EXPECT_EQ(buf[1], 0x90);
}

TEST(DIDTest, StandardDIDRanges) {
  // ISO 14229-1 DID ranges
  uint16_t vin_did = 0xF190;
  EXPECT_GE(vin_did, 0xF100);
  EXPECT_LE(vin_did, 0xF1FF);
  
  // OEM-specific range
  uint16_t oem_did = 0xFD00;
  EXPECT_GE(oem_did, 0xFD00);
  EXPECT_LE(oem_did, 0xFEFF);
}

// ############################################################################
// NRC.HPP TESTS
// ############################################################################

// ============================================================================
// NRC Code Tests
// ============================================================================

TEST(NRCCodeTest, GeneralNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::PositiveResponse), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::GeneralReject), 0x10);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::ServiceNotSupported), 0x11);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::SubFunctionNotSupported), 0x12);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::IncorrectMessageLength), 0x13);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::ResponseTooLong), 0x14);
}

TEST(NRCCodeTest, BusyAndConditionNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::BusyRepeatRequest), 0x21);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::ConditionsNotCorrect), 0x22);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::RequestSequenceError), 0x24);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::NoResponseFromSubnetComponent), 0x25);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::FailurePreventsExecutionOfRequestedAction), 0x26);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::RequestOutOfRange), 0x31);
}

TEST(NRCCodeTest, SecurityNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::SecurityAccessDenied), 0x33);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::InvalidKey), 0x35);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::ExceededNumberOfAttempts), 0x36);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::RequiredTimeDelayNotExpired), 0x37);
}

TEST(NRCCodeTest, ProgrammingNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::UploadDownloadNotAccepted), 0x70);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::TransferDataSuspended), 0x71);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::GeneralProgrammingFailure), 0x72);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::WrongBlockSequenceCounter), 0x73);
}

TEST(NRCCodeTest, ResponsePendingNRC) {
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::RequestCorrectlyReceivedResponsePending), 0x78);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::RequestCorrectlyReceived_RP), 0x78);
}

TEST(NRCCodeTest, SessionNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::SubFunctionNotSupportedInActiveSession), 0x7E);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::ServiceNotSupportedInActiveSession), 0x7F);
}

TEST(NRCCodeTest, OEMNRCs) {
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::RpmTooHigh), 0x81);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::EngineIsRunning), 0x83);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::VehicleSpeedTooHigh), 0x88);
  EXPECT_EQ(static_cast<uint8_t>(nrc::Code::VoltageTooLow), 0x93);
}

// ============================================================================
// NRC Action Tests
// ============================================================================

TEST(NRCActionTest, ActionValues) {
  // Just verify enum values exist
  nrc::Action abort = nrc::Action::Abort;
  nrc::Action retry = nrc::Action::Retry;
  nrc::Action wait = nrc::Action::Wait;
  nrc::Action wait_retry = nrc::Action::WaitAndRetry;
  nrc::Action pending = nrc::Action::ContinuePending;
  nrc::Action unsupported = nrc::Action::Unsupported;
  
  EXPECT_NE(static_cast<int>(abort), static_cast<int>(retry));
  EXPECT_NE(static_cast<int>(wait), static_cast<int>(pending));
  (void)wait_retry;
  (void)unsupported;
}

// ============================================================================
// NRC Category Tests
// ============================================================================

TEST(NRCCategoryTest, CategoryValues) {
  EXPECT_NE(static_cast<int>(nrc::Category::GeneralReject), 
            static_cast<int>(nrc::Category::Busy));
  EXPECT_NE(static_cast<int>(nrc::Category::SecurityIssue), 
            static_cast<int>(nrc::Category::ProgrammingError));
  EXPECT_NE(static_cast<int>(nrc::Category::SessionIssue), 
            static_cast<int>(nrc::Category::VehicleCondition));
}

// ============================================================================
// NRC Interpreter Tests
// ============================================================================

TEST(NRCInterpreterTest, GetDescription) {
  auto desc = nrc::Interpreter::get_description(nrc::Code::SecurityAccessDenied);
  EXPECT_FALSE(desc.empty());
  
  desc = nrc::Interpreter::get_description(nrc::Code::GeneralReject);
  EXPECT_FALSE(desc.empty());
  
  desc = nrc::Interpreter::get_description(nrc::Code::WrongBlockSequenceCounter);
  EXPECT_FALSE(desc.empty());
}

TEST(NRCInterpreterTest, GetCategory) {
  EXPECT_EQ(nrc::Interpreter::get_category(nrc::Code::SecurityAccessDenied), 
            nrc::Category::SecurityIssue);
  EXPECT_EQ(nrc::Interpreter::get_category(nrc::Code::InvalidKey), 
            nrc::Category::SecurityIssue);
  EXPECT_EQ(nrc::Interpreter::get_category(nrc::Code::BusyRepeatRequest), 
            nrc::Category::Busy);
  EXPECT_EQ(nrc::Interpreter::get_category(nrc::Code::GeneralProgrammingFailure), 
            nrc::Category::ProgrammingError);
  EXPECT_EQ(nrc::Interpreter::get_category(nrc::Code::RequestCorrectlyReceivedResponsePending), 
            nrc::Category::ResponsePending);
}

TEST(NRCInterpreterTest, IsResponsePending) {
  EXPECT_TRUE(nrc::Interpreter::is_response_pending(nrc::Code::RequestCorrectlyReceivedResponsePending));
  EXPECT_TRUE(nrc::Interpreter::is_response_pending(nrc::Code::RequestCorrectlyReceived_RP));
  EXPECT_FALSE(nrc::Interpreter::is_response_pending(nrc::Code::GeneralReject));
  EXPECT_FALSE(nrc::Interpreter::is_response_pending(nrc::Code::SecurityAccessDenied));
}

TEST(NRCInterpreterTest, IsRecoverable) {
  EXPECT_TRUE(nrc::Interpreter::is_recoverable(nrc::Code::BusyRepeatRequest));
  EXPECT_TRUE(nrc::Interpreter::is_recoverable(nrc::Code::RequestCorrectlyReceivedResponsePending));
  EXPECT_FALSE(nrc::Interpreter::is_recoverable(nrc::Code::GeneralReject));
  EXPECT_FALSE(nrc::Interpreter::is_recoverable(nrc::Code::ServiceNotSupported));
}

TEST(NRCInterpreterTest, IsSecurityError) {
  EXPECT_TRUE(nrc::Interpreter::is_security_error(nrc::Code::SecurityAccessDenied));
  EXPECT_TRUE(nrc::Interpreter::is_security_error(nrc::Code::InvalidKey));
  EXPECT_TRUE(nrc::Interpreter::is_security_error(nrc::Code::ExceededNumberOfAttempts));
  EXPECT_FALSE(nrc::Interpreter::is_security_error(nrc::Code::GeneralReject));
}

TEST(NRCInterpreterTest, IsProgrammingError) {
  EXPECT_TRUE(nrc::Interpreter::is_programming_error(nrc::Code::UploadDownloadNotAccepted));
  EXPECT_TRUE(nrc::Interpreter::is_programming_error(nrc::Code::GeneralProgrammingFailure));
  EXPECT_TRUE(nrc::Interpreter::is_programming_error(nrc::Code::WrongBlockSequenceCounter));
  EXPECT_FALSE(nrc::Interpreter::is_programming_error(nrc::Code::SecurityAccessDenied));
}

TEST(NRCInterpreterTest, IsSessionError) {
  EXPECT_TRUE(nrc::Interpreter::is_session_error(nrc::Code::SubFunctionNotSupportedInActiveSession));
  EXPECT_TRUE(nrc::Interpreter::is_session_error(nrc::Code::ServiceNotSupportedInActiveSession));
  EXPECT_FALSE(nrc::Interpreter::is_session_error(nrc::Code::GeneralReject));
}

TEST(NRCInterpreterTest, NeedsExtendedTimeout) {
  EXPECT_TRUE(nrc::Interpreter::needs_extended_timeout(nrc::Code::RequestCorrectlyReceivedResponsePending));
  EXPECT_FALSE(nrc::Interpreter::needs_extended_timeout(nrc::Code::GeneralReject));
}

TEST(NRCInterpreterTest, GetAction) {
  EXPECT_EQ(nrc::Interpreter::get_action(nrc::Code::RequestCorrectlyReceivedResponsePending), 
            nrc::Action::ContinuePending);
  EXPECT_EQ(nrc::Interpreter::get_action(nrc::Code::ServiceNotSupported), 
            nrc::Action::Unsupported);
}

TEST(NRCInterpreterTest, ParseFromResponse) {
  // Valid negative response
  std::vector<uint8_t> neg_resp = {0x7F, 0x22, 0x33};
  auto nrc = nrc::Interpreter::parse_from_response(neg_resp);
  EXPECT_TRUE(nrc.has_value());
  EXPECT_EQ(nrc.value(), nrc::Code::SecurityAccessDenied);
  
  // Positive response (not an NRC)
  std::vector<uint8_t> pos_resp = {0x62, 0xF1, 0x90};
  nrc = nrc::Interpreter::parse_from_response(pos_resp);
  EXPECT_FALSE(nrc.has_value());
  
  // Too short
  std::vector<uint8_t> short_resp = {0x7F};
  nrc = nrc::Interpreter::parse_from_response(short_resp);
  EXPECT_FALSE(nrc.has_value());
}

TEST(NRCInterpreterTest, FormatForLog) {
  auto log = nrc::Interpreter::format_for_log(nrc::Code::SecurityAccessDenied);
  EXPECT_FALSE(log.empty());
  // Should contain hex code
  EXPECT_NE(log.find("33"), std::string::npos);
}

TEST(NRCInterpreterTest, GetRecommendedAction) {
  auto action = nrc::Interpreter::get_recommended_action(nrc::Code::BusyRepeatRequest);
  EXPECT_FALSE(action.empty());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
