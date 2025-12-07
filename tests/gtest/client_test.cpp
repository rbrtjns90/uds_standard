/**
 * @file client_test.cpp
 * @brief Comprehensive tests for UDS Client methods (uds.cpp)
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "isotp.hpp"
#include <queue>

using namespace uds;

// Mock Transport for Testing
class MockTransport : public Transport {
public:
  void set_address(const Address& addr) override { addr_ = addr; }
  const Address& address() const override { return addr_; }
  
  bool request_response(const std::vector<uint8_t>& tx,
                        std::vector<uint8_t>& rx,
                        std::chrono::milliseconds) override {
    last_request_ = tx;
    if (fail_next_) { fail_next_ = false; return false; }
    if (!responses_.empty()) { rx = responses_.front(); responses_.pop(); return true; }
    return false;
  }
  
  bool recv_unsolicited(std::vector<uint8_t>& rx, std::chrono::milliseconds) override {
    if (!unsolicited_.empty()) { rx = unsolicited_.front(); unsolicited_.pop(); return true; }
    return false;
  }
  
  void queue_response(const std::vector<uint8_t>& r) { responses_.push(r); }
  void queue_unsolicited(const std::vector<uint8_t>& m) { unsolicited_.push(m); }
  void set_fail_next(bool f) { fail_next_ = f; }
  const std::vector<uint8_t>& last_request() const { return last_request_; }
  void reset() { while (!responses_.empty()) responses_.pop(); last_request_.clear(); fail_next_ = false; }

private:
  Address addr_;
  std::queue<std::vector<uint8_t>> responses_, unsolicited_;
  std::vector<uint8_t> last_request_;
  bool fail_next_ = false;
};

class ClientTest : public ::testing::Test {
protected:
  void SetUp() override { transport_.reset(); }
  MockTransport transport_;
};

// Exchange Tests
TEST_F(ClientTest, ExchangePositiveResponse) {
  Client client(transport_);
  transport_.queue_response({0x62, 0xF1, 0x90, 'V', 'I', 'N'});
  auto result = client.exchange(SID::ReadDataByIdentifier, {0xF1, 0x90});
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.payload.size(), 5u);
}

TEST_F(ClientTest, ExchangeNegativeResponse) {
  Client client(transport_);
  transport_.queue_response({0x7F, 0x22, 0x33});
  auto result = client.exchange(SID::ReadDataByIdentifier, {0xF1, 0x90});
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.nrc.code, NegativeResponseCode::SecurityAccessDenied);
}

TEST_F(ClientTest, ExchangeTransportFailure) {
  Client client(transport_);
  transport_.set_fail_next(true);
  auto result = client.exchange(SID::ReadDataByIdentifier, {0xF1, 0x90});
  EXPECT_FALSE(result.ok);
}

TEST_F(ClientTest, ExchangeEmptyResponse) {
  Client client(transport_);
  transport_.queue_response({});
  auto result = client.exchange(SID::ReadDataByIdentifier, {0xF1, 0x90});
  EXPECT_FALSE(result.ok);
}

// DiagnosticSessionControl Tests
TEST_F(ClientTest, DiagnosticSessionControlDefault) {
  Client client(transport_);
  transport_.queue_response({0x50, 0x01, 0x00, 0x32, 0x01, 0xF4});
  auto result = client.diagnostic_session_control(Session::DefaultSession);
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(transport_.last_request()[0], 0x10);
}

TEST_F(ClientTest, DiagnosticSessionControlUpdatesTimings) {
  Client client(transport_);
  transport_.queue_response({0x50, 0x03, 0x00, 0x32, 0x01, 0xF4});
  client.diagnostic_session_control(Session::ExtendedSession);
  EXPECT_EQ(client.timings().p2.count(), 50);
  EXPECT_EQ(client.timings().p2_star.count(), 500);
}

// ECUReset Tests
TEST_F(ClientTest, ECUResetHard) {
  Client client(transport_);
  transport_.queue_response({0x51, 0x01});
  auto result = client.ecu_reset(EcuResetType::HardReset);
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(transport_.last_request()[1], 0x01);
}

// TesterPresent Tests
TEST_F(ClientTest, TesterPresent) {
  Client client(transport_);
  transport_.queue_response({0x7E, 0x00});
  auto result = client.tester_present(false);
  EXPECT_TRUE(result.ok);
}

// SecurityAccess Tests
TEST_F(ClientTest, SecurityAccessRequestSeed) {
  Client client(transport_);
  transport_.queue_response({0x67, 0x01, 0xAB, 0xCD, 0xEF, 0x12});
  auto result = client.security_access_request_seed(0x01);
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, SecurityAccessSendKey) {
  Client client(transport_);
  transport_.queue_response({0x67, 0x02});
  auto result = client.security_access_send_key(0x01, {0x12, 0x34, 0x56, 0x78});
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(transport_.last_request()[1], 0x02);
}

// ReadDataByIdentifier Tests
TEST_F(ClientTest, ReadDataByIdentifier) {
  Client client(transport_);
  transport_.queue_response({0x62, 0xF1, 0x90, 'V', 'I', 'N'});
  auto result = client.read_data_by_identifier(0xF190);
  EXPECT_TRUE(result.ok);
}

// WriteDataByIdentifier Tests
TEST_F(ClientTest, WriteDataByIdentifier) {
  Client client(transport_);
  transport_.queue_response({0x6E, 0xF1, 0x90});
  auto result = client.write_data_by_identifier(0xF190, {'N', 'E', 'W'});
  EXPECT_TRUE(result.ok);
}

// ReadMemoryByAddress Tests
TEST_F(ClientTest, ReadMemoryByAddress) {
  Client client(transport_);
  transport_.queue_response({0x63, 0xDE, 0xAD, 0xBE, 0xEF});
  auto result = client.read_memory_by_address(0x00010000, 4);
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(transport_.last_request()[1], 0x44);
}

TEST_F(ClientTest, ReadMemoryByAddressVector) {
  Client client(transport_);
  transport_.queue_response({0x63, 0x01, 0x02});
  auto result = client.read_memory_by_address({0x00, 0x01}, {0x02});
  EXPECT_TRUE(result.ok);
}

// WriteMemoryByAddress Tests
TEST_F(ClientTest, WriteMemoryByAddress) {
  Client client(transport_);
  transport_.queue_response({0x7D, 0x44});
  auto result = client.write_memory_by_address(0x00010000, {0xDE, 0xAD});
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, WriteMemoryByAddressVector) {
  Client client(transport_);
  transport_.queue_response({0x7D, 0x22});
  auto result = client.write_memory_by_address({0x00, 0x01}, {0x02}, {0xAB});
  EXPECT_TRUE(result.ok);
}

// RoutineControl Tests
TEST_F(ClientTest, RoutineControlStart) {
  Client client(transport_);
  transport_.queue_response({0x71, 0x01, 0xFF, 0x00});
  auto result = client.routine_control(RoutineAction::Start, 0xFF00, {});
  EXPECT_TRUE(result.ok);
}

// Transfer Tests
TEST_F(ClientTest, RequestDownload) {
  Client client(transport_);
  transport_.queue_response({0x74, 0x20, 0x00, 0x80});
  auto result = client.request_download(0x00, {0x00, 0x01}, {0x00, 0x10});
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, RequestUpload) {
  Client client(transport_);
  transport_.queue_response({0x75, 0x20, 0x00, 0x80});
  auto result = client.request_upload(0x00, {0x00, 0x01}, {0x00, 0x10});
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, TransferData) {
  Client client(transport_);
  transport_.queue_response({0x76, 0x01});
  auto result = client.transfer_data(0x01, {0xDE, 0xAD});
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, RequestTransferExit) {
  Client client(transport_);
  transport_.queue_response({0x77});
  auto result = client.request_transfer_exit({});
  EXPECT_TRUE(result.ok);
}

// CommunicationControl Tests
TEST_F(ClientTest, CommunicationControlEnable) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x00});
  auto result = client.communication_control(0x00, 0x01);
  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(client.communication_state().rx_enabled);
  EXPECT_TRUE(client.communication_state().tx_enabled);
}

TEST_F(ClientTest, CommunicationControlDisable) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x03});
  auto result = client.communication_control(0x03, 0x01);
  EXPECT_TRUE(result.ok);
  EXPECT_FALSE(client.communication_state().rx_enabled);
  EXPECT_FALSE(client.communication_state().tx_enabled);
}

// ControlDTCSetting Tests
TEST_F(ClientTest, ControlDTCSettingOn) {
  Client client(transport_);
  transport_.queue_response({0xC5, 0x01});
  auto result = client.control_dtc_setting(0x01);
  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(client.is_dtc_setting_enabled());
}

TEST_F(ClientTest, ControlDTCSettingOff) {
  Client client(transport_);
  transport_.queue_response({0xC5, 0x02});
  auto result = client.control_dtc_setting(0x02);
  EXPECT_TRUE(result.ok);
  EXPECT_FALSE(client.is_dtc_setting_enabled());
}

// AccessTimingParameters Tests
TEST_F(ClientTest, AccessTimingParametersRead) {
  Client client(transport_);
  transport_.queue_response({0xC3, 0x03, 0x00, 0x32, 0x00, 0x64});
  auto result = client.access_timing_parameters(AccessTimingParametersType::ReadCurrentlyActiveTimingParameters, {});
  EXPECT_TRUE(result.ok);
}

// PeriodicIdentifier Tests
TEST_F(ClientTest, ReadDataByPeriodicIdentifier) {
  Client client(transport_);
  transport_.queue_response({0x6A, 0x01});
  auto result = client.read_data_by_periodic_identifier(PeriodicTransmissionMode::SendAtSlowRate, {0x01, 0x02});
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, ReceivePeriodicData) {
  Client client(transport_);
  transport_.queue_unsolicited({0x6A, 0x01, 0xAB, 0xCD});
  PeriodicDataMessage msg;
  bool received = client.receive_periodic_data(msg, std::chrono::milliseconds(100));
  EXPECT_TRUE(received);
  EXPECT_EQ(msg.identifier, 0x01);
  EXPECT_EQ(msg.data.size(), 2u);
}

TEST_F(ClientTest, ReceivePeriodicDataTimeout) {
  Client client(transport_);
  PeriodicDataMessage msg;
  bool received = client.receive_periodic_data(msg, std::chrono::milliseconds(10));
  EXPECT_FALSE(received);
}

// DDDI Tests
TEST_F(ClientTest, DynamicallyDefineByDID) {
  Client client(transport_);
  transport_.queue_response({0x6C, 0x01, 0xF3, 0x00});
  std::vector<DDDI_SourceByDID> sources = {{0xF190, 1, 17}};
  auto result = client.dynamically_define_data_identifier_by_did(0xF300, sources);
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, DynamicallyDefineByMemory) {
  Client client(transport_);
  transport_.queue_response({0x6C, 0x02, 0xF3, 0x00});
  std::vector<DDDI_SourceByMemory> sources = {{0x44, {0x00, 0x01}, {0x00, 0x10}}};
  auto result = client.dynamically_define_data_identifier_by_memory(0xF300, sources);
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, ClearDynamicallyDefinedDID) {
  Client client(transport_);
  transport_.queue_response({0x6C, 0x03, 0xF3, 0x00});
  auto result = client.clear_dynamically_defined_data_identifier(0xF300);
  EXPECT_TRUE(result.ok);
}

// DTC Tests
TEST_F(ClientTest, ClearDiagnosticInformation) {
  Client client(transport_);
  transport_.queue_response({0x54});
  auto result = client.clear_diagnostic_information({0xFF, 0xFF, 0xFF});
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, ReadDTCInformation) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x02, 0xFF});
  auto result = client.read_dtc_information(0x02, {0xFF});
  EXPECT_TRUE(result.ok);
}

// Scaling Tests
TEST_F(ClientTest, ReadScalingDataByIdentifier) {
  Client client(transport_);
  transport_.queue_response({0x64, 0xF1, 0x90, 0x01});
  auto result = client.read_scaling_data_by_identifier(0xF190);
  EXPECT_TRUE(result.ok);
}

// Additional CommunicationControl Tests for full coverage
TEST_F(ClientTest, CommunicationControlEnableRxDisableTx) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x01});
  auto result = client.communication_control(0x01, 0x01);  // EnableRxDisableTx
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, CommunicationControlDisableRxEnableTx) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x02});
  auto result = client.communication_control(0x02, 0x01);  // DisableRxEnableTx
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, CommunicationControlDisableRxAndTx) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x03});
  auto result = client.communication_control(0x03, 0x01);  // DisableRxAndTx
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, CommunicationControlEnhancedAddr) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x04});
  auto result = client.communication_control(0x04, 0x01);  // EnableRxAndTxWithEnhancedAddrInfo
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, CommunicationControlEnhancedAddrRxOnly) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x05});
  auto result = client.communication_control(0x05, 0x01);  // EnableRxDisableTxWithEnhancedAddrInfo
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, CommunicationControlEnhancedAddrTxOnly) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x06});
  auto result = client.communication_control(0x06, 0x01);  // DisableRxEnableTxWithEnhancedAddrInfo
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, CommunicationControlEnhancedAddrDisable) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x07});
  auto result = client.communication_control(0x07, 0x01);  // DisableRxAndTxWithEnhancedAddrInfo
  EXPECT_TRUE(result.ok);
}

TEST_F(ClientTest, CommunicationControlOEMSpecific) {
  Client client(transport_);
  transport_.queue_response({0x68, 0x80});
  auto result = client.communication_control(0x80, 0x01);  // OEM-specific
  EXPECT_TRUE(result.ok);
}

// Additional ControlDTCSetting Tests
TEST_F(ClientTest, ControlDTCSettingOEMSpecific) {
  Client client(transport_);
  transport_.queue_response({0xC5, 0x80});
  auto result = client.control_dtc_setting(0x80);  // OEM-specific
  EXPECT_TRUE(result.ok);
}

// AccessTimingParameters with timing update
TEST_F(ClientTest, AccessTimingParametersReadExtended) {
  Client client(transport_);
  // Response with P2=50ms (0x0032), P2*=5000ms (0x01F4 in 10ms units)
  transport_.queue_response({0xC3, 0x02, 0x00, 0x32, 0x01, 0xF4});
  auto result = client.access_timing_parameters(AccessTimingParametersType::ReadExtendedTimingParameterSet, {});
  EXPECT_TRUE(result.ok);
}

// ReceivePeriodicData with invalid response
TEST_F(ClientTest, ReceivePeriodicDataInvalidResponse) {
  Client client(transport_);
  transport_.queue_unsolicited({0x6A});  // Too short - no identifier
  PeriodicDataMessage msg;
  bool received = client.receive_periodic_data(msg, std::chrono::milliseconds(100));
  EXPECT_FALSE(received);
}

// Exchange with empty response
TEST_F(ClientTest, ExchangeShortResponse) {
  Client client(transport_);
  transport_.queue_response({});  // Empty response
  auto result = client.exchange(SID::TesterPresent, {0x00});
  EXPECT_FALSE(result.ok);
}

// ReceivePeriodicData with wrong SID
TEST_F(ClientTest, ReceivePeriodicDataWrongSID) {
  Client client(transport_);
  transport_.queue_unsolicited({0x62, 0x01, 0xAB});  // Wrong SID (should be 0x6A)
  PeriodicDataMessage msg;
  bool received = client.receive_periodic_data(msg, std::chrono::milliseconds(100));
  EXPECT_FALSE(received);
}

// ReceivePeriodicData with no data (just SID + identifier)
TEST_F(ClientTest, ReceivePeriodicDataNoPayload) {
  Client client(transport_);
  transport_.queue_unsolicited({0x6A, 0x01});  // Just SID + identifier, no data
  PeriodicDataMessage msg;
  bool received = client.receive_periodic_data(msg, std::chrono::milliseconds(100));
  EXPECT_TRUE(received);
  EXPECT_EQ(msg.identifier, 0x01);
  EXPECT_TRUE(msg.data.empty());
}

// DiagnosticSessionControl failure
TEST_F(ClientTest, DiagnosticSessionControlFailure) {
  Client client(transport_);
  transport_.queue_response({0x7F, 0x10, 0x12});  // NRC: SubFunctionNotSupported
  auto result = client.diagnostic_session_control(Session::ProgrammingSession);
  EXPECT_FALSE(result.ok);
}

// AccessTimingParameters failure path
TEST_F(ClientTest, AccessTimingParametersFailure) {
  Client client(transport_);
  transport_.queue_response({0x7F, 0x83, 0x12});  // NRC
  auto result = client.access_timing_parameters(AccessTimingParametersType::ReadCurrentlyActiveTimingParameters, {});
  EXPECT_FALSE(result.ok);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
