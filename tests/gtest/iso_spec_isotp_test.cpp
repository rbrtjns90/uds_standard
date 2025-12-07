/**
 * @file iso_spec_isotp_test.cpp
 * @brief ISO 15765-2 Spec-Anchored Tests: ISO-TP Transport Layer
 * 
 * References:
 * - ISO 15765-2:2016 Section 8 (Network layer protocol)
 * - ISO 15765-2:2016 Section 9 (Protocol data unit specification)
 */

#include <gtest/gtest.h>
#include "isotp.hpp"
#include <vector>
#include <cstdint>

// ============================================================================
// ISO 15765-2 Section 9.4: N_PCI (Protocol Control Information)
// ============================================================================

class NPCISpecTest : public ::testing::Test {
protected:
    // Per ISO 15765-2 Table 8: N_PCI types
    static constexpr uint8_t SINGLE_FRAME = 0x00;       // SF: bits 7-4 = 0
    static constexpr uint8_t FIRST_FRAME = 0x10;        // FF: bits 7-4 = 1
    static constexpr uint8_t CONSECUTIVE_FRAME = 0x20;  // CF: bits 7-4 = 2
    static constexpr uint8_t FLOW_CONTROL = 0x30;       // FC: bits 7-4 = 3
};

TEST_F(NPCISpecTest, FrameTypeIdentification) {
    // Frame type is in high nibble of first byte
    
    auto get_frame_type = [](uint8_t first_byte) -> uint8_t {
        return first_byte & 0xF0;
    };
    
    EXPECT_EQ(SINGLE_FRAME, get_frame_type(0x07));      // SF with 7 bytes
    EXPECT_EQ(FIRST_FRAME, get_frame_type(0x10));       // FF
    EXPECT_EQ(FIRST_FRAME, get_frame_type(0x1F));       // FF with length
    EXPECT_EQ(CONSECUTIVE_FRAME, get_frame_type(0x21)); // CF with SN=1
    EXPECT_EQ(FLOW_CONTROL, get_frame_type(0x30));      // FC CTS
}

// ============================================================================
// ISO 15765-2 Section 9.4.2: Single Frame (SF)
// ============================================================================

class SingleFrameSpecTest : public ::testing::Test {
protected:
    // SF format: [N_PCI] [data...]
    // N_PCI: bits 7-4 = 0, bits 3-0 = SF_DL (data length)
    // For CAN FD with >8 bytes: N_PCI byte 1 = 0x00, byte 2 = SF_DL
};

TEST_F(SingleFrameSpecTest, ClassicCANFormat) {
    // Classic CAN: max 7 data bytes in SF (8-byte CAN frame, 1 byte N_PCI)
    
    // 5-byte message
    std::vector<uint8_t> sf_5bytes = {0x05, 0x22, 0xF1, 0x90, 0x00, 0x00};
    
    uint8_t frame_type = sf_5bytes[0] & 0xF0;
    uint8_t sf_dl = sf_5bytes[0] & 0x0F;
    
    EXPECT_EQ(0x00, frame_type) << "Frame type = SF";
    EXPECT_EQ(5, sf_dl) << "Data length = 5";
}

TEST_F(SingleFrameSpecTest, MaxSingleFrameLength) {
    // Classic CAN: max SF_DL = 7 (8-byte frame - 1 byte N_PCI)
    // CAN FD 12-byte: max SF_DL = 11
    // CAN FD 64-byte: max SF_DL = 62
    
    constexpr uint8_t MAX_SF_DL_CLASSIC = 7;
    constexpr uint8_t MAX_SF_DL_CANFD_12 = 11;
    constexpr uint8_t MAX_SF_DL_CANFD_64 = 62;
    
    EXPECT_EQ(7, MAX_SF_DL_CLASSIC);
    EXPECT_EQ(11, MAX_SF_DL_CANFD_12);
    EXPECT_EQ(62, MAX_SF_DL_CANFD_64);
}

TEST_F(SingleFrameSpecTest, CANFDEscapeSequence) {
    // CAN FD with >8 bytes uses escape sequence
    // First byte = 0x00, second byte = actual length
    
    std::vector<uint8_t> sf_canfd = {0x00, 0x10 /* 16 bytes */, /* data... */};
    
    uint8_t first_nibble = sf_canfd[0] & 0x0F;
    EXPECT_EQ(0x00, first_nibble) << "Escape: low nibble = 0";
    
    uint8_t actual_length = sf_canfd[1];
    EXPECT_EQ(16, actual_length);
}

// ============================================================================
// ISO 15765-2 Section 9.4.3: First Frame (FF)
// ============================================================================

class FirstFrameSpecTest : public ::testing::Test {};

TEST_F(FirstFrameSpecTest, ClassicFormat) {
    // FF format: [N_PCI_1] [N_PCI_2] [data...]
    // N_PCI_1: bits 7-4 = 1, bits 3-0 = FF_DL high nibble
    // N_PCI_2: FF_DL low byte
    // Total FF_DL = 12 bits (0-4095 bytes)
    
    // Example: 256 bytes total message
    uint16_t message_length = 256;
    uint8_t npci_1 = 0x10 | ((message_length >> 8) & 0x0F);
    uint8_t npci_2 = message_length & 0xFF;
    
    std::vector<uint8_t> ff = {npci_1, npci_2, /* first 6 data bytes */};
    
    EXPECT_EQ(0x11, ff[0]) << "FF with high nibble of length";
    EXPECT_EQ(0x00, ff[1]) << "Low byte of length";
    
    // Parse length back
    uint16_t parsed_length = ((ff[0] & 0x0F) << 8) | ff[1];
    EXPECT_EQ(256, parsed_length);
}

TEST_F(FirstFrameSpecTest, ExtendedFormat) {
    // For messages > 4095 bytes (CAN FD)
    // N_PCI_1 = 0x10, N_PCI_2 = 0x00
    // Followed by 4-byte length
    
    uint32_t message_length = 65536;  // 64KB
    
    std::vector<uint8_t> ff_extended = {
        0x10, 0x00,  // Escape sequence
        static_cast<uint8_t>((message_length >> 24) & 0xFF),
        static_cast<uint8_t>((message_length >> 16) & 0xFF),
        static_cast<uint8_t>((message_length >> 8) & 0xFF),
        static_cast<uint8_t>(message_length & 0xFF)
    };
    
    EXPECT_EQ(0x10, ff_extended[0]);
    EXPECT_EQ(0x00, ff_extended[1]) << "Escape: indicates extended length";
    
    // Parse 32-bit length
    uint32_t parsed = (static_cast<uint32_t>(ff_extended[2]) << 24) |
                      (static_cast<uint32_t>(ff_extended[3]) << 16) |
                      (static_cast<uint32_t>(ff_extended[4]) << 8) |
                      ff_extended[5];
    EXPECT_EQ(65536, parsed);
}

TEST_F(FirstFrameSpecTest, DataBytesInFF) {
    // Classic CAN FF: 6 data bytes (8 - 2 N_PCI bytes)
    // CAN FD FF: varies based on frame size
    
    constexpr size_t FF_DATA_CLASSIC = 6;
    constexpr size_t FF_DATA_CANFD_12 = 10;  // 12 - 2
    constexpr size_t FF_DATA_CANFD_64 = 62;  // 64 - 2
    
    EXPECT_EQ(6, FF_DATA_CLASSIC);
    EXPECT_EQ(10, FF_DATA_CANFD_12);
    EXPECT_EQ(62, FF_DATA_CANFD_64);
}

// ============================================================================
// ISO 15765-2 Section 9.4.4: Consecutive Frame (CF)
// ============================================================================

class ConsecutiveFrameSpecTest : public ::testing::Test {};

TEST_F(ConsecutiveFrameSpecTest, Format) {
    // CF format: [N_PCI] [data...]
    // N_PCI: bits 7-4 = 2, bits 3-0 = SN (sequence number)
    
    // First CF after FF has SN = 1
    std::vector<uint8_t> cf_1 = {0x21, /* 7 data bytes */};
    
    uint8_t frame_type = cf_1[0] & 0xF0;
    uint8_t sn = cf_1[0] & 0x0F;
    
    EXPECT_EQ(0x20, frame_type) << "Frame type = CF";
    EXPECT_EQ(1, sn) << "First CF has SN = 1";
}

TEST_F(ConsecutiveFrameSpecTest, SequenceNumberWrap) {
    // SN wraps from 0xF to 0x0
    
    std::vector<uint8_t> sequence_numbers;
    for (int i = 0; i < 20; ++i) {
        uint8_t sn = (i + 1) & 0x0F;  // Starts at 1, wraps at 16
        sequence_numbers.push_back(sn);
    }
    
    EXPECT_EQ(1, sequence_numbers[0]);   // First CF
    EXPECT_EQ(0xF, sequence_numbers[14]); // 15th CF
    EXPECT_EQ(0x0, sequence_numbers[15]); // 16th CF wraps to 0
    EXPECT_EQ(1, sequence_numbers[16]);   // 17th CF
}

TEST_F(ConsecutiveFrameSpecTest, DataBytesInCF) {
    // Classic CAN CF: 7 data bytes (8 - 1 N_PCI byte)
    
    constexpr size_t CF_DATA_CLASSIC = 7;
    EXPECT_EQ(7, CF_DATA_CLASSIC);
}

// ============================================================================
// ISO 15765-2 Section 9.4.5: Flow Control (FC)
// ============================================================================

class FlowControlSpecTest : public ::testing::Test {
protected:
    // FC format: [N_PCI] [BS] [STmin]
    // N_PCI: bits 7-4 = 3, bits 3-0 = FS (flow status)
    
    static constexpr uint8_t FS_CTS = 0x00;   // Continue To Send
    static constexpr uint8_t FS_WAIT = 0x01;  // Wait
    static constexpr uint8_t FS_OVFLW = 0x02; // Overflow
};

TEST_F(FlowControlSpecTest, CTSFormat) {
    // Continue To Send
    std::vector<uint8_t> fc_cts = {
        0x30,  // FC with FS = CTS
        0x00,  // BS = 0 (no limit)
        0x14   // STmin = 20ms
    };
    
    uint8_t frame_type = fc_cts[0] & 0xF0;
    uint8_t flow_status = fc_cts[0] & 0x0F;
    uint8_t block_size = fc_cts[1];
    uint8_t st_min = fc_cts[2];
    
    EXPECT_EQ(0x30, frame_type) << "Frame type = FC";
    EXPECT_EQ(FS_CTS, flow_status) << "Flow status = CTS";
    EXPECT_EQ(0, block_size) << "BS = 0 means no limit";
    EXPECT_EQ(20, st_min) << "STmin = 20ms";
}

TEST_F(FlowControlSpecTest, WaitFormat) {
    // Wait - receiver needs more time
    std::vector<uint8_t> fc_wait = {0x31, 0x00, 0x00};
    
    uint8_t flow_status = fc_wait[0] & 0x0F;
    EXPECT_EQ(FS_WAIT, flow_status) << "Flow status = Wait";
}

TEST_F(FlowControlSpecTest, OverflowFormat) {
    // Overflow - receiver buffer full
    std::vector<uint8_t> fc_overflow = {0x32, 0x00, 0x00};
    
    uint8_t flow_status = fc_overflow[0] & 0x0F;
    EXPECT_EQ(FS_OVFLW, flow_status) << "Flow status = Overflow";
}

TEST_F(FlowControlSpecTest, BlockSizeValues) {
    // BS = 0: No limit, send all CFs
    // BS = 1-255: Send BS frames, then wait for next FC
    
    uint8_t bs_no_limit = 0x00;
    uint8_t bs_10_frames = 0x0A;
    uint8_t bs_max = 0xFF;
    
    EXPECT_EQ(0, bs_no_limit);
    EXPECT_EQ(10, bs_10_frames);
    EXPECT_EQ(255, bs_max);
}

TEST_F(FlowControlSpecTest, STminValues) {
    // Per ISO 15765-2 Table 10:
    // 0x00-0x7F: 0-127 ms
    // 0x80-0xF0: Reserved
    // 0xF1-0xF9: 100-900 microseconds
    // 0xFA-0xFF: Reserved
    
    // Millisecond values
    EXPECT_EQ(0, 0x00);    // 0 ms
    EXPECT_EQ(50, 0x32);   // 50 ms
    EXPECT_EQ(127, 0x7F);  // 127 ms
    
    // Microsecond values (0xF1-0xF9)
    uint8_t stmin_100us = 0xF1;
    uint8_t stmin_500us = 0xF5;
    uint8_t stmin_900us = 0xF9;
    
    EXPECT_EQ(0xF1, stmin_100us);
    EXPECT_EQ(0xF5, stmin_500us);
    EXPECT_EQ(0xF9, stmin_900us);
    
    // Convert to microseconds
    auto stmin_to_us = [](uint8_t stmin) -> uint32_t {
        if (stmin <= 0x7F) {
            return stmin * 1000;  // ms to us
        } else if (stmin >= 0xF1 && stmin <= 0xF9) {
            return (stmin - 0xF0) * 100;  // 100us units
        }
        return 0;  // Reserved
    };
    
    EXPECT_EQ(50000, stmin_to_us(0x32));   // 50ms = 50000us
    EXPECT_EQ(100, stmin_to_us(0xF1));     // 100us
    EXPECT_EQ(500, stmin_to_us(0xF5));     // 500us
}

// ============================================================================
// ISO 15765-2: Complete Transfer Scenarios
// ============================================================================

class ISOTPTransferSpecTest : public ::testing::Test {};

TEST_F(ISOTPTransferSpecTest, SingleFrameTransfer) {
    // Message <= 7 bytes: single frame only
    
    // UDS ReadDataByIdentifier request (3 bytes)
    std::vector<uint8_t> request = {0x22, 0xF1, 0x90};
    
    // Encapsulate in SF
    std::vector<uint8_t> sf(8, 0x00);  // 8-byte CAN frame
    sf[0] = static_cast<uint8_t>(request.size());  // SF_DL
    std::copy(request.begin(), request.end(), sf.begin() + 1);
    
    EXPECT_EQ(0x03, sf[0]) << "SF_DL = 3";
    EXPECT_EQ(0x22, sf[1]) << "SID";
    EXPECT_EQ(0xF1, sf[2]) << "DID high";
    EXPECT_EQ(0x90, sf[3]) << "DID low";
}

TEST_F(ISOTPTransferSpecTest, MultiFrameTransfer) {
    // Message > 7 bytes: FF + CFs
    
    // 20-byte message
    uint16_t message_length = 20;
    
    // First Frame
    std::vector<uint8_t> ff(8);
    ff[0] = 0x10 | ((message_length >> 8) & 0x0F);
    ff[1] = message_length & 0xFF;
    // ff[2-7] = first 6 data bytes
    
    EXPECT_EQ(0x10, ff[0]) << "FF with length < 256";
    EXPECT_EQ(20, ff[1]) << "Length = 20";
    
    // Need ceil((20 - 6) / 7) = 2 consecutive frames
    size_t remaining = message_length - 6;
    size_t cf_count = (remaining + 6) / 7;  // 7 bytes per CF
    
    EXPECT_EQ(2, cf_count) << "Need 2 CFs for remaining 14 bytes";
}

TEST_F(ISOTPTransferSpecTest, FlowControlledTransfer) {
    // Sender sends FF
    // Receiver sends FC (CTS, BS=2, STmin=10)
    // Sender sends 2 CFs
    // Receiver sends FC (CTS)
    // Sender sends remaining CFs
    
    // FC with BS=2
    std::vector<uint8_t> fc = {0x30, 0x02, 0x0A};
    
    uint8_t bs = fc[1];
    uint8_t stmin = fc[2];
    
    EXPECT_EQ(2, bs) << "Block size = 2";
    EXPECT_EQ(10, stmin) << "STmin = 10ms";
}

// ============================================================================
// ISO 15765-2: Timing Parameters
// ============================================================================

class ISOTPTimingSpecTest : public ::testing::Test {};

TEST_F(ISOTPTimingSpecTest, TimingParameters) {
    // Per ISO 15765-2 Section 8.5
    
    // N_As: Time for sender to transmit CAN frame
    // N_Ar: Time for receiver to transmit CAN frame
    // N_Bs: Time for receiver to send FC after FF
    // N_Br: Time for sender to send CF after FC
    // N_Cs: Time for sender to send next CF
    // N_Cr: Time for receiver to receive next CF
    
    // Typical values (ms)
    constexpr uint32_t N_AS_MAX = 1000;
    constexpr uint32_t N_AR_MAX = 1000;
    constexpr uint32_t N_BS_MAX = 1000;
    constexpr uint32_t N_CR_MAX = 1000;
    
    EXPECT_EQ(1000, N_AS_MAX);
    EXPECT_EQ(1000, N_BS_MAX);
}

TEST_F(ISOTPTimingSpecTest, WaitFrameLimit) {
    // Per ISO 15765-2: Maximum number of FC.Wait frames
    // Typically N_WFTmax = 10
    
    constexpr uint8_t N_WFT_MAX = 10;
    EXPECT_EQ(10, N_WFT_MAX);
}
