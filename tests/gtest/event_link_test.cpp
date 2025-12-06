/**
 * @file event_link_test.cpp
 * @brief Google Test suite for ResponseOnEvent and LinkControl services
 */

#include <gtest/gtest.h>
#include "uds_event.hpp"
#include "uds_link.hpp"

using namespace uds;

// ============================================================================
// ResponseOnEvent Event Type Tests
// ============================================================================

TEST(EventTest, EventTypeValues) {
  using namespace event;
  
  EXPECT_EQ(static_cast<uint8_t>(EventType::StopResponseOnEvent), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(EventType::OnDTCStatusChange), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(EventType::OnTimerInterrupt), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(EventType::OnChangeOfDataIdentifier), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(EventType::ReportActivatedEvents), 0x04);
  EXPECT_EQ(static_cast<uint8_t>(EventType::StartResponseOnEvent), 0x05);
  EXPECT_EQ(static_cast<uint8_t>(EventType::ClearResponseOnEvent), 0x06);
  EXPECT_EQ(static_cast<uint8_t>(EventType::OnComparisonOfValues), 0x07);
}

// ============================================================================
// Event Window Time Tests
// ============================================================================

TEST(EventTest, EventWindowTimeValues) {
  using namespace event;
  
  EXPECT_EQ(static_cast<uint8_t>(EventWindowTime::InfiniteTimeToResponse), 0x02);
}

// ============================================================================
// Event Configuration Tests
// ============================================================================

TEST(EventTest, EventConfigDefaults) {
  using namespace event;
  
  EventConfig config;
  
  EXPECT_EQ(config.event_type, EventType::StopResponseOnEvent);
  EXPECT_EQ(config.event_window_time, 0x02);
  EXPECT_EQ(config.service_to_respond, 0x00);
  EXPECT_TRUE(config.service_record.empty());
}

TEST(EventTest, EventConfigDTCChange) {
  using namespace event;
  
  EventConfig config;
  config.event_type = EventType::OnDTCStatusChange;
  config.event_window_time = 0x02;  // Infinite
  config.service_to_respond = 0x19;  // ReadDTCInformation
  config.service_record = {0x02, 0xFF};  // reportDTCByStatusMask, all DTCs
  
  EXPECT_EQ(config.event_type, EventType::OnDTCStatusChange);
  EXPECT_EQ(config.service_to_respond, 0x19);
  EXPECT_EQ(config.service_record.size(), 2u);
}

TEST(EventTest, EventConfigTimer) {
  using namespace event;
  
  EventConfig config;
  config.event_type = EventType::OnTimerInterrupt;
  config.service_record = {0x00, 0x64};  // 100ms timer (big-endian)
  
  uint16_t timer_ms = (config.service_record[0] << 8) | config.service_record[1];
  EXPECT_EQ(timer_ms, 100);
}

TEST(EventTest, EventConfigDIDChange) {
  using namespace event;
  
  EventConfig config;
  config.event_type = EventType::OnChangeOfDataIdentifier;
  config.service_to_respond = 0x22;  // ReadDataByIdentifier
  config.service_record = {0xF1, 0x90};  // VIN DID
  
  uint16_t did = (config.service_record[0] << 8) | config.service_record[1];
  EXPECT_EQ(did, 0xF190);
}

// ============================================================================
// LinkControl Type Tests
// ============================================================================

TEST(LinkTest, LinkControlTypeValues) {
  using namespace link;
  
  EXPECT_EQ(static_cast<uint8_t>(LinkControlType::VerifyBaudrateTransitionWithFixedBaudrate), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(LinkControlType::VerifyBaudrateTransitionWithSpecificBaudrate), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(LinkControlType::TransitionBaudrate), 0x03);
}

// ============================================================================
// Fixed Baudrate Tests
// ============================================================================

TEST(LinkTest, FixedBaudrateValues) {
  using namespace link;
  
  EXPECT_EQ(static_cast<uint8_t>(FixedBaudrate::CAN_125kbps), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(FixedBaudrate::CAN_250kbps), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(FixedBaudrate::CAN_500kbps), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(FixedBaudrate::CAN_1Mbps), 0x04);
}

// ============================================================================
// Baudrate Encoding Tests
// ============================================================================

TEST(LinkTest, BaudrateEncoding) {
  // Specific baudrate is encoded as 3 bytes (big-endian)
  uint32_t baudrate = 500000;  // 500 kbps
  
  std::vector<uint8_t> encoded;
  encoded.push_back((baudrate >> 16) & 0xFF);
  encoded.push_back((baudrate >> 8) & 0xFF);
  encoded.push_back(baudrate & 0xFF);
  
  EXPECT_EQ(encoded.size(), 3u);
  EXPECT_EQ(encoded[0], 0x07);
  EXPECT_EQ(encoded[1], 0xA1);
  EXPECT_EQ(encoded[2], 0x20);
  
  // Decode back
  uint32_t decoded = (encoded[0] << 16) | (encoded[1] << 8) | encoded[2];
  EXPECT_EQ(decoded, baudrate);
}

// ============================================================================
// Link Request Tests
// ============================================================================

TEST(LinkTest, LinkRequestDefaults) {
  using namespace link;
  
  LinkRequest request;
  
  EXPECT_EQ(request.control_type, LinkControlType::VerifyBaudrateTransitionWithFixedBaudrate);
  EXPECT_FALSE(request.baudrate_id.has_value());
  EXPECT_FALSE(request.specific_baudrate_bps.has_value());
}

TEST(LinkTest, LinkRequestFixed) {
  using namespace link;
  
  LinkRequest request;
  request.control_type = LinkControlType::VerifyBaudrateTransitionWithFixedBaudrate;
  request.baudrate_id = static_cast<uint8_t>(FixedBaudrate::CAN_500kbps);
  
  EXPECT_EQ(request.control_type, LinkControlType::VerifyBaudrateTransitionWithFixedBaudrate);
  EXPECT_TRUE(request.baudrate_id.has_value());
  EXPECT_EQ(request.baudrate_id.value(), 0x03);
}

TEST(LinkTest, LinkRequestSpecific) {
  using namespace link;
  
  LinkRequest request;
  request.control_type = LinkControlType::VerifyBaudrateTransitionWithSpecificBaudrate;
  request.specific_baudrate_bps = 833333;  // 833.333 kbps
  
  EXPECT_EQ(request.control_type, LinkControlType::VerifyBaudrateTransitionWithSpecificBaudrate);
  EXPECT_TRUE(request.specific_baudrate_bps.has_value());
  EXPECT_EQ(request.specific_baudrate_bps.value(), 833333u);
}

// ============================================================================
// Link Response Tests
// ============================================================================

TEST(LinkTest, LinkResponseDefaults) {
  using namespace link;
  
  LinkResponse response;
  
  EXPECT_TRUE(response.link_baudrate_record.empty());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
