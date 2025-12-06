#ifndef ECU_PROGRAMMING_HPP
#define ECU_PROGRAMMING_HPP

/*
  ECU Programming State Machine — Professional Flash Tool Implementation
  
  Implements the complete 10-step programming procedure required by OEMs:
  1. Enter Programming Session (0x10 0x02)
  2. Security Access (seed/key exchange)
  3. Disable DTC Setting (0x85 0x02)
  4. Disable Communications (0x28 0x03 0xFF)
  5. Erase Memory (RoutineControl 0x31)
  6. Request Download (0x34) - parse block length
  7. Transfer Data (0x36) - handle block counter, NRC 0x73, NRC 0x78
  8. Request Transfer Exit (0x37)
  9. Re-enable DTC and Communications
  10. ECU Reset (0x11)
  
  Handles critical error conditions:
  - NRC 0x73 (Wrong Block Sequence Counter)
  - NRC 0x78 (Response Pending) with extended timeout
  - Multi-frame flow control wait states
  - Power interruption recovery
*/

#include "uds.hpp"
#include <functional>
#include <memory>
#include <string>

namespace uds {

// ================================================================
// Programming Routine IDs (OEM-specific, common examples)
// ================================================================
namespace ProgrammingRoutineId {
  constexpr RoutineId EraseMemory          = 0xFF00;  // Erase flash region
  constexpr RoutineId PrepareWrite         = 0xFF01;  // Prepare for flash write
  constexpr RoutineId CheckProgrammingDeps = 0x0202;  // Check programming dependencies
  constexpr RoutineId CheckMemory          = 0xFF01;  // Verify flash checksum/integrity
  constexpr RoutineId EraseFlash           = 0xFF00;  // Full erase
  
  // Manufacturer-specific variants
  constexpr RoutineId VW_EraseFlash        = 0xFF00;
  constexpr RoutineId BMW_PrepareFlash     = 0x0301;
  constexpr RoutineId GM_EraseMemory       = 0xFF00;
  constexpr RoutineId Ford_PrepareDownload = 0x0202;
}

// ================================================================
// Programming State Machine States
// ================================================================
enum class ProgrammingState : uint8_t {
  Idle = 0,
  EnteringProgrammingSession,
  UnlockingSecurity,
  DisablingDTC,
  DisablingCommunications,
  ErasingMemory,
  RequestingDownload,
  TransferringData,
  ExitingTransfer,
  ReenablingServices,
  ResettingECU,
  Completed,
  Failed,
  Aborted
};

// ================================================================
// Programming Configuration
// ================================================================
struct ProgrammingConfig {
  // Security
  uint8_t security_level{0x01};  // Security access level (0x01, 0x03, 0x11, etc.)
  std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)> key_calculator;
  
  // Memory regions
  uint32_t start_address{0x00000000};
  uint32_t memory_size{0x00000000};
  uint8_t address_length_format{0x44};  // 4 bytes addr, 4 bytes size
  uint8_t data_format_identifier{0x00}; // 0x00 = uncompressed/unencrypted
  
  // Erase routine
  RoutineId erase_routine_id{ProgrammingRoutineId::EraseMemory};
  std::vector<uint8_t> erase_option_record;  // Optional parameters for erase
  std::chrono::milliseconds erase_timeout{std::chrono::milliseconds(30000)}; // 30s
  
  // Transfer parameters
  uint16_t max_block_size{0};  // 0 = use ECU's reported maxNumberOfBlockLength
  uint8_t block_counter_start{1};  // UDS block counter starts at 1
  std::chrono::milliseconds transfer_timeout{std::chrono::milliseconds(5000)};
  std::chrono::milliseconds pending_timeout{std::chrono::milliseconds(60000)}; // For NRC 0x78
  uint32_t inter_block_delay_ms{10};  // Delay between blocks
  
  // Retries
  uint8_t max_transfer_retries{3};
  uint8_t max_security_attempts{3};
  
  // Callbacks
  std::function<void(ProgrammingState, const std::string&)> state_callback;
  std::function<void(uint32_t bytes_transferred, uint32_t total_bytes, float progress)> progress_callback;
  std::function<void(bool success, const std::string& message)> completion_callback;
  
  // Safety flags
  bool skip_erase{false};              // Skip erase (dangerous - use for testing only)
  bool skip_security{false};           // Skip security (only if ECU allows)
  bool skip_communication_disable{false}; // Keep comms enabled (less safe)
  bool perform_reset_after_flash{true}; // ECU reset after completion
};

// ================================================================
// Programming Session Result
// ================================================================
struct ProgrammingResult {
  bool success{false};
  ProgrammingState final_state{ProgrammingState::Idle};
  std::string error_message;
  NegativeResponseCode last_nrc{};
  
  // Statistics
  uint32_t bytes_transferred{0};
  uint32_t total_bytes{0};
  uint16_t blocks_transferred{0};
  uint16_t total_blocks{0};
  uint8_t retry_count{0};
  std::chrono::milliseconds elapsed_time{};
  
  // Diagnostics
  std::vector<std::string> log_messages;
};

// ================================================================
// ECU Programming State Machine
// ================================================================
class ECUProgrammer {
public:
  explicit ECUProgrammer(Client& client);
  
  // ========================================================================
  // Main Programming Interface
  // ========================================================================
  
  /// Execute complete programming sequence
  /// @param firmware_data Complete firmware binary to flash
  /// @param config Programming configuration
  /// @return Result with success status and diagnostics
  ProgrammingResult program_ecu(const std::vector<uint8_t>& firmware_data,
                                const ProgrammingConfig& config);
  
  /// Abort programming in progress (safe cleanup)
  void abort_programming();
  
  /// Check if programming is currently active
  bool is_programming_active() const { return state_ != ProgrammingState::Idle; }
  
  /// Get current state
  ProgrammingState current_state() const { return state_; }
  
  /// Get last result
  const ProgrammingResult& last_result() const { return result_; }
  
  // ========================================================================
  // Individual State Machine Steps (for advanced users)
  // ========================================================================
  
  /// Step 1: Enter programming session (0x10 0x02)
  bool step_enter_programming_session();
  
  /// Step 2: Security access (seed/key exchange)
  bool step_security_access(uint8_t level,
                           const std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>& key_calc);
  
  /// Step 3: Disable DTC setting (0x85 0x02)
  bool step_disable_dtc_setting();
  
  /// Step 4: Disable communications (0x28 0x03 0xFF)
  bool step_disable_communications();
  
  /// Step 5: Erase memory (RoutineControl 0x31)
  bool step_erase_memory(RoutineId routine_id,
                        const std::vector<uint8_t>& option_record = {},
                        std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
  
  /// Step 6: Request download (0x34)
  bool step_request_download(uint32_t address, uint32_t size,
                             uint8_t addr_len_fmt = 0x44,
                             uint8_t data_fmt = 0x00);
  
  /// Step 7: Transfer data blocks (0x36)
  bool step_transfer_data(const std::vector<uint8_t>& firmware_data);
  
  /// Step 8: Request transfer exit (0x37)
  bool step_request_transfer_exit();
  
  /// Step 9: Re-enable DTC and communications
  bool step_reenable_services();
  
  /// Step 10: ECU reset (0x11)
  bool step_ecu_reset(EcuResetType reset_type = EcuResetType::HardReset);
  
  // ========================================================================
  // Utility Functions
  // ========================================================================
  
  /// Get human-readable state name
  static const char* state_name(ProgrammingState state);
  
  /// Parse maxNumberOfBlockLength from RequestDownload response
  static uint32_t parse_max_block_length(const std::vector<uint8_t>& response);
  
  /// Calculate number of blocks needed
  static uint16_t calculate_block_count(uint32_t data_size, uint16_t block_size);
  
  /// Encode address and size for RequestDownload
  static std::vector<uint8_t> encode_address_and_size(uint32_t address, uint32_t size,
                                                      uint8_t addr_len_fmt);

private:
  Client& client_;
  ProgrammingState state_{ProgrammingState::Idle};
  ProgrammingConfig config_;
  ProgrammingResult result_;
  
  // Transfer state
  uint8_t block_counter_{1};
  uint16_t max_block_length_{0};
  bool abort_requested_{false};
  
  // Helpers
  void log(const std::string& message);
  void update_state(ProgrammingState new_state, const std::string& message = "");
  void report_progress(uint32_t bytes_transferred, uint32_t total_bytes);
  void handle_failure(const std::string& error, NegativeResponseCode nrc = NegativeResponseCode::GeneralReject);
  
  /// Handle NRC 0x78 (Response Pending) - retry with extended timeout
  PositiveOrNegative handle_response_pending(const std::function<PositiveOrNegative()>& request_fn,
                                             std::chrono::milliseconds extended_timeout);
  
  /// Transfer single block with retry logic for NRC 0x73 (Wrong Block Sequence)
  bool transfer_block_with_retry(BlockCounter block, const std::vector<uint8_t>& block_data);
  
  /// Wait for routine completion (handles NRC 0x78)
  bool wait_for_routine_completion(RoutineId routine_id,
                                   std::chrono::milliseconds timeout);
};

// ================================================================
// High-Level Programming Functions
// ================================================================

/// Execute complete ECU flash programming with defaults
/// @param client UDS client instance
/// @param firmware_data Complete firmware binary
/// @param start_address Flash start address
/// @param key_calculator Security access key calculator
/// @return Programming result
ProgrammingResult flash_ecu(Client& client,
                            const std::vector<uint8_t>& firmware_data,
                            uint32_t start_address,
                            const std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>& key_calculator);

/// Verify ECU memory region (read back and compare)
/// @param client UDS client instance
/// @param address Start address
/// @param expected_data Data to verify against
/// @param key_calculator Security access key calculator (if needed)
/// @return true if verification succeeded
bool verify_ecu_memory(Client& client,
                      uint32_t address,
                      const std::vector<uint8_t>& expected_data,
                      const std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>& key_calculator = nullptr);

} // namespace uds

#endif // ECU_PROGRAMMING_HPP
