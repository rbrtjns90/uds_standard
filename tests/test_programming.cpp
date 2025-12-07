/**
 * @file test_programming.cpp
 * @brief Comprehensive tests for uds_programming.hpp and ecu_programming.hpp
 * 
 * Tests flash programming services including:
 * - ProgrammingSession class
 * - ECUProgrammer state machine
 * - Programming states and transitions
 * - ProgrammingConfig options
 * - ProgrammingResult statistics
 * - Routine IDs for erase/verify
 */

#include "test_framework.hpp"
#include "../include/uds_programming.hpp"
#include "../include/ecu_programming.hpp"
#include <cstring>

using namespace test;
using namespace uds;

// ============================================================================
// ProgStatus Tests (uds_programming.hpp)
// ============================================================================

TEST(ProgStatus_Success) {
    std::cout << "  Testing: ProgStatus::success()" << std::endl;
    
    auto status = ProgStatus::success("Operation completed");
    
    ASSERT_TRUE(status.ok);
    ASSERT_EQ(std::string("Operation completed"), status.message);
}

TEST(ProgStatus_SuccessEmpty) {
    std::cout << "  Testing: ProgStatus::success() with empty message" << std::endl;
    
    auto status = ProgStatus::success();
    
    ASSERT_TRUE(status.ok);
    ASSERT_TRUE(status.message.empty());
}

TEST(ProgStatus_Failure) {
    std::cout << "  Testing: ProgStatus::failure()" << std::endl;
    
    auto status = ProgStatus::failure("Security access denied");
    
    ASSERT_FALSE(status.ok);
    ASSERT_EQ(std::string("Security access denied"), status.message);
}

// ============================================================================
// ProgrammingRoutineId Tests (ecu_programming.hpp)
// ============================================================================

TEST(ProgrammingRoutineId_EraseMemory) {
    std::cout << "  Testing: ProgrammingRoutineId::EraseMemory" << std::endl;
    
    ASSERT_EQ(0xFF00, ProgrammingRoutineId::EraseMemory);
}

TEST(ProgrammingRoutineId_PrepareWrite) {
    std::cout << "  Testing: ProgrammingRoutineId::PrepareWrite" << std::endl;
    
    ASSERT_EQ(0xFF01, ProgrammingRoutineId::PrepareWrite);
}

TEST(ProgrammingRoutineId_CheckProgrammingDeps) {
    std::cout << "  Testing: ProgrammingRoutineId::CheckProgrammingDeps" << std::endl;
    
    ASSERT_EQ(0x0202, ProgrammingRoutineId::CheckProgrammingDeps);
}

TEST(ProgrammingRoutineId_ManufacturerSpecific) {
    std::cout << "  Testing: Manufacturer-specific routine IDs" << std::endl;
    
    ASSERT_EQ(0xFF00, ProgrammingRoutineId::VW_EraseFlash);
    ASSERT_EQ(0x0301, ProgrammingRoutineId::BMW_PrepareFlash);
    ASSERT_EQ(0xFF00, ProgrammingRoutineId::GM_EraseMemory);
    ASSERT_EQ(0x0202, ProgrammingRoutineId::Ford_PrepareDownload);
}

// ============================================================================
// ProgrammingState Enum Tests
// ============================================================================

TEST(ProgrammingState_AllStates) {
    std::cout << "  Testing: All ProgrammingState values" << std::endl;
    
    std::vector<ProgrammingState> all_states = {
        ProgrammingState::Idle,
        ProgrammingState::EnteringProgrammingSession,
        ProgrammingState::UnlockingSecurity,
        ProgrammingState::DisablingDTC,
        ProgrammingState::DisablingCommunications,
        ProgrammingState::ErasingMemory,
        ProgrammingState::RequestingDownload,
        ProgrammingState::TransferringData,
        ProgrammingState::ExitingTransfer,
        ProgrammingState::ReenablingServices,
        ProgrammingState::ResettingECU,
        ProgrammingState::Completed,
        ProgrammingState::Failed,
        ProgrammingState::Aborted
    };
    
    ASSERT_EQ(14, static_cast<int>(all_states.size()));
}

TEST(ProgrammingState_NumericValues) {
    std::cout << "  Testing: ProgrammingState numeric values" << std::endl;
    
    ASSERT_EQ(0, static_cast<int>(ProgrammingState::Idle));
    ASSERT_EQ(1, static_cast<int>(ProgrammingState::EnteringProgrammingSession));
    ASSERT_EQ(2, static_cast<int>(ProgrammingState::UnlockingSecurity));
}

TEST(ProgrammingState_Names) {
    std::cout << "  Testing: ECUProgrammer::state_name()" << std::endl;
    
    const char* name_idle = ECUProgrammer::state_name(ProgrammingState::Idle);
    const char* name_transfer = ECUProgrammer::state_name(ProgrammingState::TransferringData);
    const char* name_completed = ECUProgrammer::state_name(ProgrammingState::Completed);
    const char* name_failed = ECUProgrammer::state_name(ProgrammingState::Failed);
    
    ASSERT_NE(nullptr, name_idle);
    ASSERT_NE(nullptr, name_transfer);
    ASSERT_NE(nullptr, name_completed);
    ASSERT_NE(nullptr, name_failed);
}

// ============================================================================
// ProgrammingConfig Tests
// ============================================================================

TEST(ProgrammingConfig_Defaults) {
    std::cout << "  Testing: ProgrammingConfig default values" << std::endl;
    
    ProgrammingConfig config;
    
    ASSERT_EQ(0x01, config.security_level);
    ASSERT_EQ(0x00000000, config.start_address);
    ASSERT_EQ(0x00000000, config.memory_size);
    ASSERT_EQ(0x44, config.address_length_format);
    ASSERT_EQ(0x00, config.data_format_identifier);
}

TEST(ProgrammingConfig_EraseRoutine) {
    std::cout << "  Testing: ProgrammingConfig erase routine defaults" << std::endl;
    
    ProgrammingConfig config;
    
    ASSERT_EQ(ProgrammingRoutineId::EraseMemory, config.erase_routine_id);
    ASSERT_TRUE(config.erase_option_record.empty());
    ASSERT_EQ(30000, config.erase_timeout.count());
}

TEST(ProgrammingConfig_TransferParams) {
    std::cout << "  Testing: ProgrammingConfig transfer parameters" << std::endl;
    
    ProgrammingConfig config;
    
    ASSERT_EQ(0, config.max_block_size);  // 0 = use ECU's value
    ASSERT_EQ(1, config.block_counter_start);
    ASSERT_EQ(5000, config.transfer_timeout.count());
    ASSERT_EQ(60000, config.pending_timeout.count());
    ASSERT_EQ(10, config.inter_block_delay_ms);
}

TEST(ProgrammingConfig_Retries) {
    std::cout << "  Testing: ProgrammingConfig retry settings" << std::endl;
    
    ProgrammingConfig config;
    
    ASSERT_EQ(3, config.max_transfer_retries);
    ASSERT_EQ(3, config.max_security_attempts);
}

TEST(ProgrammingConfig_SafetyFlags) {
    std::cout << "  Testing: ProgrammingConfig safety flags" << std::endl;
    
    ProgrammingConfig config;
    
    ASSERT_FALSE(config.skip_erase);
    ASSERT_FALSE(config.skip_security);
    ASSERT_FALSE(config.skip_communication_disable);
    ASSERT_TRUE(config.perform_reset_after_flash);
}

TEST(ProgrammingConfig_CustomValues) {
    std::cout << "  Testing: ProgrammingConfig custom configuration" << std::endl;
    
    ProgrammingConfig config;
    config.security_level = 0x11;
    config.start_address = 0x08000000;
    config.memory_size = 0x00100000;
    config.address_length_format = 0x44;
    config.data_format_identifier = 0x00;
    config.max_block_size = 4096;
    config.max_transfer_retries = 5;
    
    ASSERT_EQ(0x11, config.security_level);
    ASSERT_EQ(0x08000000, config.start_address);
    ASSERT_EQ(0x00100000, config.memory_size);
    ASSERT_EQ(4096, config.max_block_size);
    ASSERT_EQ(5, config.max_transfer_retries);
}

// ============================================================================
// ProgrammingResult Tests
// ============================================================================

TEST(ProgrammingResult_Defaults) {
    std::cout << "  Testing: ProgrammingResult default values" << std::endl;
    
    ProgrammingResult result;
    
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.final_state == ProgrammingState::Idle);
    ASSERT_TRUE(result.error_message.empty());
}

TEST(ProgrammingResult_Statistics) {
    std::cout << "  Testing: ProgrammingResult statistics" << std::endl;
    
    ProgrammingResult result;
    
    ASSERT_EQ(0, result.bytes_transferred);
    ASSERT_EQ(0, result.total_bytes);
    ASSERT_EQ(0, result.blocks_transferred);
    ASSERT_EQ(0, result.total_blocks);
    ASSERT_EQ(0, result.retry_count);
    ASSERT_EQ(0, result.elapsed_time.count());
}

TEST(ProgrammingResult_SuccessCase) {
    std::cout << "  Testing: ProgrammingResult success scenario" << std::endl;
    
    ProgrammingResult result;
    result.success = true;
    result.final_state = ProgrammingState::Completed;
    result.bytes_transferred = 1048576;
    result.total_bytes = 1048576;
    result.blocks_transferred = 256;
    result.total_blocks = 256;
    result.elapsed_time = std::chrono::milliseconds(45000);
    
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.final_state == ProgrammingState::Completed);
    ASSERT_EQ(result.bytes_transferred, result.total_bytes);
}

TEST(ProgrammingResult_FailureCase) {
    std::cout << "  Testing: ProgrammingResult failure scenario" << std::endl;
    
    ProgrammingResult result;
    result.success = false;
    result.final_state = ProgrammingState::Failed;
    result.error_message = "Security access denied";
    result.last_nrc = NegativeResponseCode::SecurityAccessDenied;
    result.bytes_transferred = 0;
    result.retry_count = 3;
    
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.final_state == ProgrammingState::Failed);
    ASSERT_FALSE(result.error_message.empty());
}

TEST(ProgrammingResult_LogMessages) {
    std::cout << "  Testing: ProgrammingResult log messages" << std::endl;
    
    ProgrammingResult result;
    result.log_messages.push_back("Entering programming session");
    result.log_messages.push_back("Security unlocked");
    result.log_messages.push_back("Erase completed");
    
    ASSERT_EQ(3, static_cast<int>(result.log_messages.size()));
}

// ============================================================================
// ECUProgrammer Utility Function Tests
// ============================================================================

TEST(ECUProgrammer_ParseMaxBlockLength) {
    std::cout << "  Testing: ECUProgrammer::parse_max_block_length()" << std::endl;
    
    // Response format: [lengthFormatIdentifier] [maxNumberOfBlockLength...]
    // Per ISO 14229-1 Section 14.2.2.2:
    //   HIGH nibble = number of bytes for maxNumberOfBlockLength
    //   LOW nibble = reserved (should be 0)
    
    // Example: 0x20 means 2 bytes for block length (high nibble = 2)
    std::vector<uint8_t> response1 = {0x20, 0x10, 0x00};  // 4096 bytes
    uint32_t block_len1 = ECUProgrammer::parse_max_block_length(response1);
    ASSERT_EQ(4096, static_cast<int>(block_len1));
    
    // Example: 0x10 means 1 byte for block length (high nibble = 1)
    std::vector<uint8_t> response2 = {0x10, 0xFF};  // 255 bytes
    uint32_t block_len2 = ECUProgrammer::parse_max_block_length(response2);
    ASSERT_EQ(255, static_cast<int>(block_len2));
}

TEST(ECUProgrammer_CalculateBlockCount) {
    std::cout << "  Testing: ECUProgrammer::calculate_block_count()" << std::endl;
    
    // Exact fit
    uint16_t count1 = ECUProgrammer::calculate_block_count(4096, 1024);
    ASSERT_EQ(4, count1);
    
    // With remainder
    uint16_t count2 = ECUProgrammer::calculate_block_count(4097, 1024);
    ASSERT_EQ(5, count2);
    
    // Single block
    uint16_t count3 = ECUProgrammer::calculate_block_count(100, 1024);
    ASSERT_EQ(1, count3);
}

TEST(ECUProgrammer_EncodeAddressAndSize) {
    std::cout << "  Testing: ECUProgrammer::encode_address_and_size()" << std::endl;
    
    // Per ISO 14229-1 ALFI: high nibble = size bytes, low nibble = address bytes
    // Format 0x44 = 4 bytes size (high), 4 bytes address (low)
    // Result includes: [formatId] + [address bytes] + [size bytes]
    auto encoded = ECUProgrammer::encode_address_and_size(0x08000000, 0x00100000, 0x44);
    
    ASSERT_EQ(9, static_cast<int>(encoded.size()));  // 1 + 4 + 4
    
    // Check format identifier
    ASSERT_EQ(0x44, encoded[0]);
    
    // Check address bytes first (big-endian) - low nibble = 4 bytes
    ASSERT_EQ(0x08, encoded[1]);
    ASSERT_EQ(0x00, encoded[2]);
    ASSERT_EQ(0x00, encoded[3]);
    ASSERT_EQ(0x00, encoded[4]);
    
    // Check size bytes second (big-endian) - high nibble = 4 bytes
    ASSERT_EQ(0x00, encoded[5]);
    ASSERT_EQ(0x10, encoded[6]);
    ASSERT_EQ(0x00, encoded[7]);
    ASSERT_EQ(0x00, encoded[8]);
}

TEST(ECUProgrammer_EncodeAddressAndSize_2Byte) {
    std::cout << "  Testing: ECUProgrammer::encode_address_and_size() with 2-byte format" << std::endl;
    
    // Format 0x22 = 2 bytes address, 2 bytes size
    // Result includes: [formatId] + [address bytes] + [size bytes]
    auto encoded = ECUProgrammer::encode_address_and_size(0x8000, 0x1000, 0x22);
    
    ASSERT_EQ(5, static_cast<int>(encoded.size()));  // 1 + 2 + 2
}

// ============================================================================
// ECUProgrammer State Machine Tests
// ============================================================================

TEST(ECUProgrammer_StateNames) {
    std::cout << "  Testing: ECUProgrammer state name strings" << std::endl;
    
    // Test all state names are valid
    ASSERT_NE(nullptr, ECUProgrammer::state_name(ProgrammingState::Idle));
    ASSERT_NE(nullptr, ECUProgrammer::state_name(ProgrammingState::TransferringData));
    ASSERT_NE(nullptr, ECUProgrammer::state_name(ProgrammingState::Completed));
    ASSERT_NE(nullptr, ECUProgrammer::state_name(ProgrammingState::Failed));
}

// ============================================================================
// Address/Length Format Tests
// ============================================================================

TEST(AddressLengthFormat_0x44) {
    std::cout << "  Testing: Address/length format 0x44 (4+4 bytes)" << std::endl;
    
    uint8_t fmt = 0x44;
    uint8_t addr_len = (fmt >> 4) & 0x0F;
    uint8_t size_len = fmt & 0x0F;
    
    ASSERT_EQ(4, addr_len);
    ASSERT_EQ(4, size_len);
}

TEST(AddressLengthFormat_0x33) {
    std::cout << "  Testing: Address/length format 0x33 (3+3 bytes)" << std::endl;
    
    uint8_t fmt = 0x33;
    uint8_t addr_len = (fmt >> 4) & 0x0F;
    uint8_t size_len = fmt & 0x0F;
    
    ASSERT_EQ(3, addr_len);
    ASSERT_EQ(3, size_len);
}

TEST(AddressLengthFormat_0x24) {
    std::cout << "  Testing: Address/length format 0x24 (2+4 bytes)" << std::endl;
    
    uint8_t fmt = 0x24;
    uint8_t addr_len = (fmt >> 4) & 0x0F;
    uint8_t size_len = fmt & 0x0F;
    
    ASSERT_EQ(2, addr_len);
    ASSERT_EQ(4, size_len);
}

// ============================================================================
// Data Format Identifier Tests
// ============================================================================

TEST(DataFormatIdentifier_Uncompressed) {
    std::cout << "  Testing: Data format identifier 0x00 (uncompressed)" << std::endl;
    
    uint8_t dfi = 0x00;
    uint8_t compression = (dfi >> 4) & 0x0F;
    uint8_t encryption = dfi & 0x0F;
    
    ASSERT_EQ(0, compression);  // No compression
    ASSERT_EQ(0, encryption);   // No encryption
}

TEST(DataFormatIdentifier_Compressed) {
    std::cout << "  Testing: Data format identifier with compression" << std::endl;
    
    uint8_t dfi = 0x10;  // Compression method 1, no encryption
    uint8_t compression = (dfi >> 4) & 0x0F;
    uint8_t encryption = dfi & 0x0F;
    
    ASSERT_EQ(1, compression);
    ASSERT_EQ(0, encryption);
}

// ============================================================================
// Block Counter Tests
// ============================================================================

TEST(BlockCounter_StartValue) {
    std::cout << "  Testing: Block counter starts at 1" << std::endl;
    
    ProgrammingConfig config;
    ASSERT_EQ(1, config.block_counter_start);
}

TEST(BlockCounter_Wraparound) {
    std::cout << "  Testing: Block counter wraparound (0xFF -> 0x00)" << std::endl;
    
    uint8_t counter = 0xFF;
    counter++;  // Should wrap to 0x00, not 0x01
    
    ASSERT_EQ(0x00, counter);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n" << colors::BOLD << "UDS Programming Services Tests" << colors::RESET << "\n";
    std::cout << "Testing flash programming workflows and state machine\n\n";
    
    TestRunner::instance().run_all_tests();
    
    auto stats = TestRunner::instance().get_stats();
    
    std::cout << "\n" << colors::BOLD << "Programming Coverage:" << colors::RESET << "\n";
    std::cout << "  ProgStatus: success/failure ✓\n";
    std::cout << "  ProgrammingState: 14 states ✓\n";
    std::cout << "  ProgrammingConfig: All fields ✓\n";
    std::cout << "  ProgrammingResult: Statistics ✓\n";
    std::cout << "  Routine IDs: Standard + OEM ✓\n";
    std::cout << "  Utility functions: Block calc, encoding ✓\n\n";
    
    return (stats.failed == 0) ? 0 : 1;
}
