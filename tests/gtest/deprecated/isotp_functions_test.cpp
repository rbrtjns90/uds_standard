/**
 * @file isotp_functions_test.cpp
 * @brief Comprehensive tests for ISO-TP transport (isotp.cpp)
 */

#include <gtest/gtest.h>
#include "isotp.hpp"
#include <queue>

using namespace isotp;

// Mock CAN Driver
class MockCANDriver : public CANProtocol {
public:
  bool send(const CANFrame& frame) override {
    if (fail_send_) return false;
    sent_frames_.push_back(frame);
    return true;
  }
  
  bool recv(CANFrame& frame, std::chrono::milliseconds) override {
    if (recv_frames_.empty()) return false;
    frame = recv_frames_.front();
    recv_frames_.pop();
    return true;
  }
  
  void queue_recv(const CANFrame& frame) { recv_frames_.push(frame); }
  const std::vector<CANFrame>& sent() const { return sent_frames_; }
  void set_fail_send(bool f) { fail_send_ = f; }
  void reset() { sent_frames_.clear(); while (!recv_frames_.empty()) recv_frames_.pop(); fail_send_ = false; }

private:
  std::vector<CANFrame> sent_frames_;
  std::queue<CANFrame> recv_frames_;
  bool fail_send_ = false;
};

class ISOTPTest : public ::testing::Test {
protected:
  void SetUp() override {
    driver_.reset();
    uds::Address addr;
    addr.tx_can_id = 0x7E0;
    addr.rx_can_id = 0x7E8;
    transport_ = std::make_unique<Transport>(driver_, addr);
  }
  
  MockCANDriver driver_;
  std::unique_ptr<Transport> transport_;
};

// ============================================================================
// Single Frame Tests
// ============================================================================

TEST_F(ISOTPTest, SendSingleFrame) {
  std::vector<uint8_t> data = {0x22, 0xF1, 0x90};  // ReadDataByIdentifier
  
  // Queue response
  CANProtocol::CANFrame resp{};
  resp.id = 0x7E8;
  resp.data[0] = 0x06;  // SF, len=6
  resp.data[1] = 0x62;
  resp.data[2] = 0xF1;
  resp.data[3] = 0x90;
  resp.data[4] = 'V';
  resp.data[5] = 'I';
  resp.data[6] = 'N';
  driver_.queue_recv(resp);
  
  std::vector<uint8_t> rx;
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(1000));
  
  EXPECT_TRUE(result);
  EXPECT_EQ(rx.size(), 6u);
  EXPECT_EQ(rx[0], 0x62);
}

TEST_F(ISOTPTest, SendSingleFrameMaxLength) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};  // 7 bytes max for SF
  
  CANProtocol::CANFrame resp{};
  resp.id = 0x7E8;
  resp.data[0] = 0x01;  // SF, len=1
  resp.data[1] = 0x41;
  driver_.queue_recv(resp);
  
  std::vector<uint8_t> rx;
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(1000));
  
  EXPECT_TRUE(result);
  EXPECT_EQ(driver_.sent().size(), 1u);
  EXPECT_EQ(driver_.sent()[0].data[0] & 0xF0, 0x00);  // SF PCI
}

TEST_F(ISOTPTest, ReceiveSingleFrame) {
  std::vector<uint8_t> data = {0x3E, 0x00};  // TesterPresent
  
  CANProtocol::CANFrame resp{};
  resp.id = 0x7E8;
  resp.data[0] = 0x02;  // SF, len=2
  resp.data[1] = 0x7E;
  resp.data[2] = 0x00;
  driver_.queue_recv(resp);
  
  std::vector<uint8_t> rx;
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(1000));
  
  EXPECT_TRUE(result);
  EXPECT_EQ(rx.size(), 2u);
}

// ============================================================================
// Multi-Frame Tests
// ============================================================================

TEST_F(ISOTPTest, SendMultiFrame) {
  // Data longer than 7 bytes requires multi-frame
  std::vector<uint8_t> data(20, 0xAA);
  
  // Queue Flow Control response
  CANProtocol::CANFrame fc{};
  fc.id = 0x7E8;
  fc.data[0] = 0x30;  // FC, CTS
  fc.data[1] = 0x00;  // BS=0 (unlimited)
  fc.data[2] = 0x00;  // STmin=0
  driver_.queue_recv(fc);
  
  // Queue final response
  CANProtocol::CANFrame resp{};
  resp.id = 0x7E8;
  resp.data[0] = 0x01;  // SF, len=1
  resp.data[1] = 0x00;
  driver_.queue_recv(resp);
  
  std::vector<uint8_t> rx;
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(1000));
  
  EXPECT_TRUE(result);
  
  // Should have sent FF + CFs
  EXPECT_GT(driver_.sent().size(), 1u);
  EXPECT_EQ(driver_.sent()[0].data[0] & 0xF0, 0x10);  // FF PCI
}

TEST_F(ISOTPTest, ReceiveMultiFrame) {
  std::vector<uint8_t> data = {0x22, 0xF1, 0x90};
  
  // Queue First Frame response (20 bytes total)
  CANProtocol::CANFrame ff{};
  ff.id = 0x7E8;
  ff.data[0] = 0x10 | 0x00;  // FF, high nibble of length
  ff.data[1] = 0x14;  // 20 bytes total
  ff.data[2] = 0x62;
  ff.data[3] = 0xF1;
  ff.data[4] = 0x90;
  ff.data[5] = 'V';
  ff.data[6] = 'I';
  ff.data[7] = 'N';
  driver_.queue_recv(ff);
  
  // Queue Consecutive Frames
  CANProtocol::CANFrame cf1{};
  cf1.id = 0x7E8;
  cf1.data[0] = 0x21;  // CF, SN=1
  cf1.data[1] = '1';
  cf1.data[2] = '2';
  cf1.data[3] = '3';
  cf1.data[4] = '4';
  cf1.data[5] = '5';
  cf1.data[6] = '6';
  cf1.data[7] = '7';
  driver_.queue_recv(cf1);
  
  CANProtocol::CANFrame cf2{};
  cf2.id = 0x7E8;
  cf2.data[0] = 0x22;  // CF, SN=2
  cf2.data[1] = '8';
  cf2.data[2] = '9';
  cf2.data[3] = 'A';
  cf2.data[4] = 'B';
  cf2.data[5] = 'C';
  cf2.data[6] = 'D';
  cf2.data[7] = 'E';
  driver_.queue_recv(cf2);
  
  std::vector<uint8_t> rx;
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(1000));
  
  EXPECT_TRUE(result);
  EXPECT_EQ(rx.size(), 20u);
  
  // Should have sent request + FC
  bool sent_fc = false;
  for (const auto& frame : driver_.sent()) {
    if ((frame.data[0] & 0xF0) == 0x30) {
      sent_fc = true;
      break;
    }
  }
  EXPECT_TRUE(sent_fc);
}

// ============================================================================
// Flow Control Tests
// ============================================================================

TEST_F(ISOTPTest, FlowControlWait) {
  std::vector<uint8_t> data(20, 0xBB);
  
  // Queue FC Wait, then FC CTS
  CANProtocol::CANFrame fc_wait{};
  fc_wait.id = 0x7E8;
  fc_wait.data[0] = 0x31;  // FC, Wait
  driver_.queue_recv(fc_wait);
  
  CANProtocol::CANFrame fc_cts{};
  fc_cts.id = 0x7E8;
  fc_cts.data[0] = 0x30;  // FC, CTS
  fc_cts.data[1] = 0x00;
  fc_cts.data[2] = 0x00;
  driver_.queue_recv(fc_cts);
  
  CANProtocol::CANFrame resp{};
  resp.id = 0x7E8;
  resp.data[0] = 0x01;
  resp.data[1] = 0x00;
  driver_.queue_recv(resp);
  
  std::vector<uint8_t> rx;
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(1000));
  
  EXPECT_TRUE(result);
}

TEST_F(ISOTPTest, FlowControlOverflow) {
  std::vector<uint8_t> data(20, 0xCC);
  
  // Queue FC Overflow
  CANProtocol::CANFrame fc{};
  fc.id = 0x7E8;
  fc.data[0] = 0x32;  // FC, Overflow
  driver_.queue_recv(fc);
  
  std::vector<uint8_t> rx;
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(1000));
  
  EXPECT_FALSE(result);
}

TEST_F(ISOTPTest, FlowControlBlockSize) {
  std::vector<uint8_t> data(50, 0xDD);  // Needs multiple blocks
  
  // FC with BS=2
  CANProtocol::CANFrame fc1{};
  fc1.id = 0x7E8;
  fc1.data[0] = 0x30;
  fc1.data[1] = 0x02;  // BS=2
  fc1.data[2] = 0x00;
  driver_.queue_recv(fc1);
  
  // Second FC after 2 CFs
  CANProtocol::CANFrame fc2{};
  fc2.id = 0x7E8;
  fc2.data[0] = 0x30;
  fc2.data[1] = 0x02;
  fc2.data[2] = 0x00;
  driver_.queue_recv(fc2);
  
  // Third FC
  CANProtocol::CANFrame fc3{};
  fc3.id = 0x7E8;
  fc3.data[0] = 0x30;
  fc3.data[1] = 0x02;
  fc3.data[2] = 0x00;
  driver_.queue_recv(fc3);
  
  // Response
  CANProtocol::CANFrame resp{};
  resp.id = 0x7E8;
  resp.data[0] = 0x01;
  resp.data[1] = 0x00;
  driver_.queue_recv(resp);
  
  std::vector<uint8_t> rx;
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(1000));
  
  EXPECT_TRUE(result);
}

// ============================================================================
// STmin Configuration Tests
// ============================================================================

TEST_F(ISOTPTest, STminConfiguration) {
  // Test STmin configuration via public interface
  transport_->set_stmin(10);
  EXPECT_EQ(transport_->stmin(), 10u);
  
  transport_->set_stmin(0);
  EXPECT_EQ(transport_->stmin(), 0u);
  
  transport_->set_stmin(127);
  EXPECT_EQ(transport_->stmin(), 127u);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ISOTPTest, SendFailure) {
  driver_.set_fail_send(true);
  
  std::vector<uint8_t> data = {0x22, 0xF1, 0x90};
  std::vector<uint8_t> rx;
  
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(100));
  
  EXPECT_FALSE(result);
}

TEST_F(ISOTPTest, ReceiveTimeout) {
  std::vector<uint8_t> data = {0x22, 0xF1, 0x90};
  std::vector<uint8_t> rx;
  
  // No response queued
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(10));
  
  EXPECT_FALSE(result);
}

TEST_F(ISOTPTest, WrongCANID) {
  std::vector<uint8_t> data = {0x22, 0xF1, 0x90};
  
  // Response with wrong CAN ID
  CANProtocol::CANFrame resp{};
  resp.id = 0x7E9;  // Wrong ID
  resp.data[0] = 0x02;
  resp.data[1] = 0x62;
  driver_.queue_recv(resp);
  
  std::vector<uint8_t> rx;
  bool result = transport_->request_response(data, rx, std::chrono::milliseconds(10));
  
  EXPECT_FALSE(result);  // Should timeout waiting for correct ID
}

// ============================================================================
// Address Configuration Tests
// ============================================================================

TEST_F(ISOTPTest, AddressConfiguration) {
  uds::Address addr;
  addr.tx_can_id = 0x700;
  addr.rx_can_id = 0x708;
  
  transport_->set_address(addr);
  
  EXPECT_EQ(transport_->address().tx_can_id, 0x700u);
  EXPECT_EQ(transport_->address().rx_can_id, 0x708u);
}

// ============================================================================
// recv_only Tests
// ============================================================================

TEST_F(ISOTPTest, RecvOnly) {
  CANProtocol::CANFrame resp{};
  resp.id = 0x7E8;
  resp.data[0] = 0x03;
  resp.data[1] = 0x7F;
  resp.data[2] = 0x22;
  resp.data[3] = 0x78;  // ResponsePending
  driver_.queue_recv(resp);
  
  std::vector<uint8_t> rx;
  bool result = transport_->recv_only(rx, std::chrono::milliseconds(100));
  
  EXPECT_TRUE(result);
  EXPECT_EQ(rx.size(), 3u);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(ISOTPTest, SetBlockSize) {
  transport_->set_block_size(8);
  EXPECT_EQ(transport_->block_size(), 8u);
}

TEST_F(ISOTPTest, SetSTmin) {
  transport_->set_stmin(10);
  EXPECT_EQ(transport_->stmin(), 10u);
}

TEST_F(ISOTPTest, SetTimings) {
  ISOTPTimings t;
  t.N_Bs = std::chrono::milliseconds(2000);
  t.N_Cr = std::chrono::milliseconds(2000);
  t.max_wft = 10;
  
  transport_->set_timings(t);
  
  EXPECT_EQ(transport_->timings().N_Bs.count(), 2000);
  EXPECT_EQ(transport_->timings().max_wft, 10u);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
