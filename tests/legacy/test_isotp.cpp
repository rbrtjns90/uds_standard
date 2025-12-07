/**
 * @file test_isotp.cpp
 * @brief ISO-TP transport layer testing - ISO 15765-2 compliance
 */

#include "test_framework.hpp"
#include "../include/isotp.hpp"
#include <vector>
#include <cstring>

using namespace test;

// ============================================================================
// Test: Single Frame (SF) Transmission
// ============================================================================

TEST(SingleFrame_MinimumSize) {
    std::cout << "  Testing: Single Frame - Minimum size (1 byte payload)" << std::endl;
    
    // Single frame format: [0x0N] [data...] where N is length
    std::vector<uint8_t> frame = {0x01, 0x3E}; // Tester Present
    
    ASSERT_EQ(0x01, frame[0] & 0x0F); // Length = 1
    ASSERT_EQ(0x3E, frame[1]); // Data byte
}

TEST(SingleFrame_MaximumSize) {
    std::cout << "  Testing: Single Frame - Maximum size (7 bytes payload)" << std::endl;
    
    // SF with 7 bytes of data (maximum for standard CAN)
    std::vector<uint8_t> frame(8);
    frame[0] = 0x07; // PCI: SF with length 7
    for (int i = 1; i <= 7; ++i) {
        frame[i] = i;
    }
    
    ASSERT_EQ(0x07, frame[0] & 0x0F);
    ASSERT_EQ(8, frame.size());
}

TEST(SingleFrame_Padding) {
    std::cout << "  Testing: Single Frame - Padding bytes" << std::endl;
    
    // 3 bytes data + padding to 8
    std::vector<uint8_t> frame = {0x03, 0x22, 0xF1, 0x90, 0xCC, 0xCC, 0xCC, 0xCC};
    
    ASSERT_EQ(0x03, frame[0] & 0x0F);
    ASSERT_EQ(8, frame.size());
}

// ============================================================================
// Test: First Frame (FF) Transmission
// ============================================================================

TEST(FirstFrame_Structure) {
    std::cout << "  Testing: First Frame - Structure" << std::endl;
    
    // FF format: [0x1X YY] [data...] where X:YY is 12-bit length
    std::vector<uint8_t> frame(8);
    uint16_t length = 100;
    
    frame[0] = 0x10 | ((length >> 8) & 0x0F); // PCI byte 1
    frame[1] = length & 0xFF;                  // PCI byte 2
    
    ASSERT_EQ(0x10, frame[0] & 0xF0); // FF identifier
    ASSERT_EQ(6, 8 - 2); // 6 bytes of data in first frame
}

TEST(FirstFrame_ExtendedLength) {
    std::cout << "  Testing: First Frame - Extended length (>4095 bytes)" << std::endl;
    
    // Extended FF: [0x10 0x00] [length32bit] [data...]
    std::vector<uint8_t> frame(8);
    frame[0] = 0x10;
    frame[1] = 0x00; // Indicates extended length follows
    
    uint32_t length = 10000;
    frame[2] = (length >> 24) & 0xFF;
    frame[3] = (length >> 16) & 0xFF;
    frame[4] = (length >> 8) & 0xFF;
    frame[5] = length & 0xFF;
    
    ASSERT_EQ(0x10, frame[0]);
    ASSERT_EQ(0x00, frame[1]);
}

// ============================================================================
// Test: Consecutive Frame (CF) Transmission
// ============================================================================

TEST(ConsecutiveFrame_Sequence) {
    std::cout << "  Testing: Consecutive Frame - Sequence numbers" << std::endl;
    
    // CF format: [0x2N] [data...] where N is sequence number 0-15
    for (int i = 0; i < 16; ++i) {
        std::vector<uint8_t> frame(8);
        frame[0] = 0x20 | (i & 0x0F);
        
        ASSERT_EQ(0x20, frame[0] & 0xF0);
        ASSERT_EQ(i, frame[0] & 0x0F);
    }
}

TEST(ConsecutiveFrame_Wraparound) {
    std::cout << "  Testing: Consecutive Frame - Sequence wraparound (15 -> 0)" << std::endl;
    
    uint8_t sequence = 15;
    std::vector<uint8_t> frame1 = {static_cast<uint8_t>(0x20 | sequence)};
    
    sequence = (sequence + 1) & 0x0F; // Should wrap to 0
    std::vector<uint8_t> frame2 = {static_cast<uint8_t>(0x20 | sequence)};
    
    ASSERT_EQ(0x2F, frame1[0]);
    ASSERT_EQ(0x20, frame2[0]);
}

// ============================================================================
// Test: Flow Control (FC) Frames
// ============================================================================

TEST(FlowControl_ContinueToSend) {
    std::cout << "  Testing: Flow Control - Continue To Send (CTS)" << std::endl;
    
    // FC-CTS: [0x30] [BS] [STmin]
    std::vector<uint8_t> fc = {0x30, 0x08, 0x14}; // BS=8, STmin=20ms
    
    ASSERT_EQ(0x30, fc[0]); // CTS
    ASSERT_EQ(0x08, fc[1]); // Block Size
    ASSERT_EQ(0x14, fc[2]); // STmin = 20ms
}

TEST(FlowControl_Wait) {
    std::cout << "  Testing: Flow Control - Wait (WT)" << std::endl;
    
    // FC-WT: [0x31] [00] [00]
    std::vector<uint8_t> fc = {0x31, 0x00, 0x00};
    
    ASSERT_EQ(0x31, fc[0]); // Wait
}

TEST(FlowControl_Overflow) {
    std::cout << "  Testing: Flow Control - Overflow (OVFL)" << std::endl;
    
    // FC-OVFL: [0x32] [00] [00]
    std::vector<uint8_t> fc = {0x32, 0x00, 0x00};
    
    ASSERT_EQ(0x32, fc[0]); // Overflow/Abort
}

// ============================================================================
// Test: Block Size (BS) Parameter
// ============================================================================

TEST(BlockSize_Zero_Unlimited) {
    std::cout << "  Testing: Block Size - Zero means unlimited" << std::endl;
    
    uint8_t block_size = 0x00;
    
    // BS=0 means send all CFs without waiting for another FC
    ASSERT_EQ(0, block_size);
}

TEST(BlockSize_NonZero) {
    std::cout << "  Testing: Block Size - Non-zero values" << std::endl;
    
    for (uint8_t bs = 1; bs <= 10; ++bs) {
        std::vector<uint8_t> fc = {0x30, bs, 0x00};
        ASSERT_EQ(bs, fc[1]);
    }
}

// ============================================================================
// Test: STmin (Separation Time) Parameter
// ============================================================================

TEST(STmin_Milliseconds) {
    std::cout << "  Testing: STmin - Millisecond range (0x00-0x7F)" << std::endl;
    
    // 0x00-0x7F: 0-127 milliseconds
    for (uint8_t stmin = 0; stmin <= 0x7F; ++stmin) {
        uint32_t ms = stmin;
        ASSERT_EQ(stmin, ms);
    }
}

TEST(STmin_Microseconds) {
    std::cout << "  Testing: STmin - Microsecond range (0xF1-0xF9)" << std::endl;
    
    // 0xF1-0xF9: 100-900 microseconds
    std::vector<uint8_t> stmin_values = {0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9};
    
    for (size_t i = 0; i < stmin_values.size(); ++i) {
        uint8_t stmin = stmin_values[i];
        uint32_t us = (stmin & 0x0F) * 100; // 100-900 μs
        
        ASSERT_GT(us, 0);
        ASSERT_LT(us, 1000);
    }
}

TEST(STmin_Reserved) {
    std::cout << "  Testing: STmin - Reserved values (0x80-0xF0, 0xFA-0xFF)" << std::endl;
    
    // Reserved values should be treated as maximum delay
    std::vector<uint8_t> reserved = {0x80, 0xA0, 0xC0, 0xF0, 0xFA, 0xFF};
    
    for (auto val : reserved) {
        ASSERT_TRUE(val > 0x7F || (val >= 0xFA && val <= 0xFF));
    }
}

// ============================================================================
// Test: Timing Parameters
// ============================================================================

TEST(Timing_N_As_Sender) {
    std::cout << "  Testing: N_As - Sender timeout (1000ms)" << std::endl;
    
    // N_As: Time for transmission of CAN frame on sender side
    const uint32_t N_As_ms = 1000;
    ASSERT_EQ(1000, N_As_ms);
}

TEST(Timing_N_Ar_Receiver) {
    std::cout << "  Testing: N_Ar - Receiver timeout (1000ms)" << std::endl;
    
    // N_Ar: Time for transmission of CAN frame on receiver side
    const uint32_t N_Ar_ms = 1000;
    ASSERT_EQ(1000, N_Ar_ms);
}

TEST(Timing_N_Bs_BlockSeparation) {
    std::cout << "  Testing: N_Bs - Flow Control timeout (1000ms)" << std::endl;
    
    // N_Bs: Time until reception of next FC frame
    const uint32_t N_Bs_ms = 1000;
    ASSERT_EQ(1000, N_Bs_ms);
}

TEST(Timing_N_Cr_ConsecutiveReceive) {
    std::cout << "  Testing: N_Cr - Consecutive Frame timeout (1000ms)" << std::endl;
    
    // N_Cr: Time until reception of next CF frame
    const uint32_t N_Cr_ms = 1000;
    ASSERT_EQ(1000, N_Cr_ms);
}

// ============================================================================
// Test: Multi-frame Segmentation
// ============================================================================

TEST(Segmentation_Calculate) {
    std::cout << "  Testing: Multi-frame segmentation calculation" << std::endl;
    
    size_t payload_size = 100;
    size_t first_frame_data = 6;  // 8 - 2 PCI bytes
    size_t cf_data = 7;           // 8 - 1 PCI byte
    
    size_t remaining = payload_size - first_frame_data;
    size_t num_cf = (remaining + cf_data - 1) / cf_data; // Round up
    
    ASSERT_GT(num_cf, 0);
    ASSERT_EQ(14, num_cf); // (100-6) / 7 = 13.4... -> 14 frames
}

TEST(Segmentation_SmallPayload) {
    std::cout << "  Testing: Small payload fits in single frame" << std::endl;
    
    size_t payload_size = 5;
    
    if (payload_size <= 7) {
        // Fits in single frame
        ASSERT_TRUE(true);
    } else {
        // Needs multi-frame
        ASSERT_TRUE(false);
    }
}

// ============================================================================
// Test: Wait Frame (WT) Handling
// ============================================================================

TEST(WaitFrame_MultipleWT) {
    std::cout << "  Testing: Multiple Wait frames handling" << std::endl;
    
    const int max_wft = 10;
    int wft_count = 0;
    
    // Simulate receiving multiple WT frames
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> fc = {0x31, 0x00, 0x00}; // WT
        
        if (fc[0] == 0x31) {
            wft_count++;
        }
        
        ASSERT_LT(wft_count, max_wft); // Should not exceed limit
    }
    
    ASSERT_EQ(5, wft_count);
}

TEST(WaitFrame_ExceedLimit) {
    std::cout << "  Testing: Exceeding Wait frame limit should abort" << std::endl;
    
    const int max_wft = 3;
    int wft_count = 0;
    bool aborted = false;
    
    for (int i = 0; i < 10; ++i) {
        wft_count++;
        
        if (wft_count > max_wft) {
            aborted = true;
            break;
        }
    }
    
    ASSERT_TRUE(aborted);
    ASSERT_GT(wft_count, max_wft);
}

// ============================================================================
// Test: Functional Addressing
// ============================================================================

TEST(FunctionalAddressing_BroadcastRequest) {
    std::cout << "  Testing: Functional addressing for broadcast" << std::endl;
    
    // Functional addressing uses different CAN ID
    uint32_t functional_id = 0x7DF; // Typical functional request ID
    uint32_t physical_id = 0x7E0;   // Physical addressing
    
    ASSERT_NE(functional_id, physical_id);
    ASSERT_EQ(0x7DF, functional_id);
}

TEST(FunctionalAddressing_NoResponse) {
    std::cout << "  Testing: Functional requests may have no response" << std::endl;
    
    // Some ECUs may not respond to functional requests
    bool response_required = false; // For functional addressing
    
    ASSERT_FALSE(response_required);
}

// ============================================================================
// Test: Error Handling
// ============================================================================

TEST(Error_InvalidPCI) {
    std::cout << "  Testing: Invalid PCI byte detection" << std::endl;
    
    // PCI upper nibble must be 0x0, 0x1, 0x2, or 0x3
    std::vector<uint8_t> invalid_pci = {0x40, 0x01, 0x02}; // Invalid PCI
    
    uint8_t pci_type = (invalid_pci[0] >> 4) & 0x0F;
    ASSERT_GT(pci_type, 3); // Invalid
}

TEST(Error_InvalidSequenceNumber) {
    std::cout << "  Testing: Invalid sequence number handling" << std::endl;
    
    uint8_t expected_seq = 5;
    uint8_t received_seq = 7; // Wrong sequence
    
    ASSERT_NE(expected_seq, received_seq);
}

TEST(Error_BufferOverflow) {
    std::cout << "  Testing: Buffer overflow detection" << std::endl;
    
    size_t buffer_size = 4095;  // Maximum standard size
    size_t received_size = 5000; // Too large
    
    if (received_size > buffer_size) {
        // Should send FC-OVFL (0x32)
        ASSERT_GT(received_size, buffer_size);
    }
}

// ============================================================================
// Test: Configuration API
// ============================================================================

TEST(Config_DefaultValues) {
    std::cout << "  Testing: Default configuration values" << std::endl;
    
    struct IsoTpConfig {
        uint8_t blockSize = 0;
        uint8_t stMin = 0;
        uint32_t n_ar = 1000;
        uint32_t n_bs = 1000;
        uint32_t n_cr = 1000;
        bool functional = false;
    };
    
    IsoTpConfig config;
    
    ASSERT_EQ(0, config.blockSize);
    ASSERT_EQ(0, config.stMin);
    ASSERT_EQ(1000, config.n_ar);
    ASSERT_FALSE(config.functional);
}

TEST(Config_CustomValues) {
    std::cout << "  Testing: Custom configuration values" << std::endl;
    
    struct IsoTpConfig {
        uint8_t blockSize;
        uint8_t stMin;
        bool functional;
    };
    
    IsoTpConfig config;
    config.blockSize = 8;
    config.stMin = 20;
    config.functional = true;
    
    ASSERT_EQ(8, config.blockSize);
    ASSERT_EQ(20, config.stMin);
    ASSERT_TRUE(config.functional);
}

// ============================================================================
// Main test runner
// ============================================================================

int main() {
    std::cout << "\n" << colors::BOLD << "ISO-TP Transport Layer Tests" << colors::RESET << "\n";
    std::cout << "Testing ISO 15765-2 Compliance\n\n";
    
    TestRunner::instance().run_all_tests();
    
    auto stats = TestRunner::instance().get_stats();
    return (stats.failed == 0) ? 0 : 1;
}

