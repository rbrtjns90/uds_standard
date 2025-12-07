#include "ecu_programming.hpp"
#include <thread>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace uds {

// ================================================================
// ECUProgrammer Implementation
// ================================================================

ECUProgrammer::ECUProgrammer(Client& client)
  : client_(client) {}

const char* ECUProgrammer::state_name(ProgrammingState state) {
  switch (state) {
    case ProgrammingState::Idle: return "Idle";
    case ProgrammingState::EnteringProgrammingSession: return "Entering Programming Session";
    case ProgrammingState::UnlockingSecurity: return "Unlocking Security";
    case ProgrammingState::DisablingDTC: return "Disabling DTC Setting";
    case ProgrammingState::DisablingCommunications: return "Disabling Communications";
    case ProgrammingState::ErasingMemory: return "Erasing Memory";
    case ProgrammingState::RequestingDownload: return "Requesting Download";
    case ProgrammingState::TransferringData: return "Transferring Data";
    case ProgrammingState::ExitingTransfer: return "Exiting Transfer";
    case ProgrammingState::ReenablingServices: return "Re-enabling Services";
    case ProgrammingState::ResettingECU: return "Resetting ECU";
    case ProgrammingState::Completed: return "Completed";
    case ProgrammingState::Failed: return "Failed";
    case ProgrammingState::Aborted: return "Aborted";
    default: return "Unknown";
  }
}

void ECUProgrammer::log(const std::string& message) {
  result_.log_messages.push_back(message);
}

void ECUProgrammer::update_state(ProgrammingState new_state, const std::string& message) {
  state_ = new_state;
  std::string log_msg = std::string(state_name(new_state));
  if (!message.empty()) {
    log_msg += ": " + message;
  }
  log(log_msg);
  
  if (config_.state_callback) {
    config_.state_callback(new_state, message);
  }
}

void ECUProgrammer::report_progress(uint32_t bytes_transferred, uint32_t total_bytes) {
  result_.bytes_transferred = bytes_transferred;
  result_.total_bytes = total_bytes;
  
  if (config_.progress_callback && total_bytes > 0) {
    float progress = static_cast<float>(bytes_transferred) / static_cast<float>(total_bytes);
    config_.progress_callback(bytes_transferred, total_bytes, progress);
  }
}

void ECUProgrammer::handle_failure(const std::string& error, NegativeResponseCode nrc) {
  result_.success = false;
  result_.error_message = error;
  result_.last_nrc = nrc;
  update_state(ProgrammingState::Failed, error);
  
  if (config_.completion_callback) {
    config_.completion_callback(false, error);
  }
}

void ECUProgrammer::abort_programming() {
  abort_requested_ = true;
  update_state(ProgrammingState::Aborted, "User abort requested");
}

// ================================================================
// Step 1: Enter Programming Session (0x10 0x02)
// ================================================================

bool ECUProgrammer::step_enter_programming_session() {
  update_state(ProgrammingState::EnteringProgrammingSession);
  
  auto resp = client_.diagnostic_session_control(Session::ProgrammingSession);
  
  if (!resp.ok) {
    handle_failure("Failed to enter programming session", resp.nrc.code);
    return false;
  }
  
  log("Programming session established");
  return true;
}

// ================================================================
// Step 2: Security Access (seed/key exchange)
// ================================================================

bool ECUProgrammer::step_security_access(uint8_t level,
                                        const std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>& key_calc) {
  update_state(ProgrammingState::UnlockingSecurity);
  
  if (!key_calc) {
    handle_failure("No key calculator provided", NegativeResponseCode::SecurityAccessDenied);
    return false;
  }
  
  for (uint8_t attempt = 0; attempt < config_.max_security_attempts; ++attempt) {
    // Request seed
    auto seed_resp = client_.security_access_request_seed(level);
    
    if (!seed_resp.ok) {
      if (seed_resp.nrc.code == NegativeResponseCode::RequiredTimeDelayNotExpired) {
        log("Security delay active, waiting...");
        std::this_thread::sleep_for(std::chrono::seconds(10));
        continue;
      }
      handle_failure("Failed to request security seed", seed_resp.nrc.code);
      return false;
    }
    
    // Check if already unlocked (seed = all zeros)
    bool unlocked = true;
    for (uint8_t b : seed_resp.payload) {
      if (b != 0) {
        unlocked = false;
        break;
      }
    }
    
    if (unlocked) {
      log("Security already unlocked");
      return true;
    }
    
    // Calculate key
    std::vector<uint8_t> key = key_calc(seed_resp.payload);
    if (key.empty()) {
      handle_failure("Key calculator returned empty key", NegativeResponseCode::InvalidKey);
      return false;
    }
    
    // Send key
    auto key_resp = client_.security_access_send_key(level, key);
    
    if (key_resp.ok) {
      log("Security unlocked successfully");
      return true;
    }
    
    if (key_resp.nrc.code == NegativeResponseCode::InvalidKey) {
      log("Invalid key, attempt " + std::to_string(attempt + 1));
      result_.retry_count++;
      continue;
    }
    
    handle_failure("Security access failed", key_resp.nrc.code);
    return false;
  }
  
  handle_failure("Exceeded maximum security attempts", NegativeResponseCode::ExceededNumberOfAttempts);
  return false;
}

// ================================================================
// Step 3: Disable DTC Setting (0x85 0x02)
// ================================================================

bool ECUProgrammer::step_disable_dtc_setting() {
  update_state(ProgrammingState::DisablingDTC);
  
  auto resp = client_.control_dtc_setting(static_cast<uint8_t>(DTCSettingType::Off));
  
  if (!resp.ok) {
    handle_failure("Failed to disable DTC setting", resp.nrc.code);
    return false;
  }
  
  log("DTC setting disabled");
  return true;
}

// ================================================================
// Step 4: Disable Communications (0x28 0x03 0xFF)
// ================================================================

bool ECUProgrammer::step_disable_communications() {
  update_state(ProgrammingState::DisablingCommunications);
  
  // 0x28 = CommunicationControl
  // 0x03 = DisableRxAndTx
  // 0xFF = All communication types
  auto resp = client_.communication_control(
    static_cast<uint8_t>(CommunicationControlType::DisableRxAndTx),
    0xFF  // All networks/message types
  );
  
  if (!resp.ok) {
    handle_failure("Failed to disable communications", resp.nrc.code);
    return false;
  }
  
  log("Communications disabled");
  return true;
}

// ================================================================
// Step 5: Erase Memory (RoutineControl 0x31)
// ================================================================

bool ECUProgrammer::step_erase_memory(RoutineId routine_id,
                                      const std::vector<uint8_t>& option_record,
                                      std::chrono::milliseconds timeout) {
  update_state(ProgrammingState::ErasingMemory);
  
  // Start erase routine
  auto resp = client_.routine_control(RoutineAction::Start, routine_id, option_record);
  
  if (!resp.ok) {
    if (resp.nrc.code == NegativeResponseCode::RequestCorrectlyReceived_ResponsePending) {
      // Handle NRC 0x78 - ECU is busy erasing
      log("Erase in progress (NRC 0x78), waiting...");
      return wait_for_routine_completion(routine_id, timeout);
    }
    handle_failure("Failed to start erase routine", resp.nrc.code);
    return false;
  }
  
  log("Memory erase completed");
  return true;
}

// ================================================================
// Step 6: Request Download (0x34)
// ================================================================

bool ECUProgrammer::step_request_download(uint32_t address, uint32_t size,
                                          uint8_t addr_len_fmt,
                                          uint8_t data_fmt) {
  update_state(ProgrammingState::RequestingDownload);
  
  // Decode number of address and size bytes from addr_len_fmt
  // High nibble = address length, low nibble = size length (ISO-14229)
  uint8_t addr_bytes = (addr_len_fmt >> 4) & 0x0F;
  uint8_t size_bytes = addr_len_fmt & 0x0F;
  
  if (addr_bytes == 0 || addr_bytes > 4 || size_bytes == 0 || size_bytes > 4) {
    handle_failure("Invalid address/size length format", NegativeResponseCode::GeneralReject);
    return false;
  }
  
  // Build address and size vectors (big-endian)
  std::vector<uint8_t> addr_vec;
  std::vector<uint8_t> size_vec;
  addr_vec.reserve(addr_bytes);
  size_vec.reserve(size_bytes);
  
  for (int i = addr_bytes - 1; i >= 0; --i) {
    addr_vec.push_back(static_cast<uint8_t>((address >> (i * 8)) & 0xFF));
  }
  for (int i = size_bytes - 1; i >= 0; --i) {
    size_vec.push_back(static_cast<uint8_t>((size >> (i * 8)) & 0xFF));
  }
  
  auto resp = client_.request_download(data_fmt, addr_vec, size_vec);
  
  if (!resp.ok) {
    handle_failure("Failed to request download", resp.nrc.code);
    return false;
  }
  
  // Parse maxNumberOfBlockLength from response
  max_block_length_ = parse_max_block_length(resp.payload);
  
  if (max_block_length_ == 0) {
    handle_failure("Invalid maxNumberOfBlockLength in response", NegativeResponseCode::GeneralReject);
    return false;
  }
  
  // Override with user-specified max block size if provided
  if (config_.max_block_size > 0 && config_.max_block_size < max_block_length_) {
    max_block_length_ = config_.max_block_size;
  }
  
  std::ostringstream oss;
  oss << "Download requested, max block length: " << max_block_length_ << " bytes";
  log(oss.str());
  
  return true;
}

// ================================================================
// Step 7: Transfer Data (0x36)
// ================================================================

bool ECUProgrammer::step_transfer_data(const std::vector<uint8_t>& firmware_data) {
  update_state(ProgrammingState::TransferringData);
  
  if (max_block_length_ == 0) {
    handle_failure("Max block length not set", NegativeResponseCode::RequestSequenceError);
    return false;
  }
  
  uint32_t total_bytes = firmware_data.size();
  uint16_t total_blocks = calculate_block_count(total_bytes, max_block_length_);
  result_.total_bytes = total_bytes;
  result_.total_blocks = total_blocks;
  
  block_counter_ = config_.block_counter_start;
  uint32_t offset = 0;
  
  for (uint16_t block_num = 0; block_num < total_blocks; ++block_num) {
    if (abort_requested_) {
      handle_failure("Transfer aborted by user", NegativeResponseCode::GeneralReject);
      return false;
    }
    
    // Calculate block size
    uint32_t remaining = total_bytes - offset;
    uint16_t block_size = (remaining < max_block_length_) ? remaining : max_block_length_;
    
    // Extract block data
    std::vector<uint8_t> block_data(firmware_data.begin() + offset,
                                    firmware_data.begin() + offset + block_size);
    
    // Transfer with retry
    if (!transfer_block_with_retry(block_counter_, block_data)) {
      return false;
    }
    
    // Update progress
    offset += block_size;
    result_.blocks_transferred++;
    report_progress(offset, total_bytes);
    
    // Increment block counter (wraps from 0xFF to 0x00)
    block_counter_++;
    if (block_counter_ == 0) {
      block_counter_ = 1;  // Skip 0, restart at 1
    }
    
    // Inter-block delay
    if (config_.inter_block_delay_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(config_.inter_block_delay_ms));
    }
  }
  
  log("All data blocks transferred successfully");
  return true;
}

bool ECUProgrammer::transfer_block_with_retry(BlockCounter block, const std::vector<uint8_t>& block_data) {
  for (uint8_t retry = 0; retry < config_.max_transfer_retries; ++retry) {
    auto resp = client_.transfer_data(block, block_data);
    
    if (resp.ok) {
      return true;
    }
    
    // Handle NRC 0x73 (Wrong Block Sequence Counter)
    if (resp.nrc.code == NegativeResponseCode::WrongBlockSequenceCounter) {
      log("Wrong block sequence counter, retrying...");
      result_.retry_count++;
      continue;
    }
    
    // Handle NRC 0x78 (Response Pending)
    if (resp.nrc.code == NegativeResponseCode::RequestCorrectlyReceived_ResponsePending) {
      log("Transfer pending, waiting...");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      result_.retry_count++;
      continue;
    }
    
    // Other error
    std::ostringstream oss;
    oss << "Transfer data failed at block " << static_cast<int>(block);
    handle_failure(oss.str(), resp.nrc.code);
    return false;
  }
  
  handle_failure("Exceeded transfer retries", NegativeResponseCode::GeneralProgrammingFailure);
  return false;
}

// ================================================================
// Step 8: Request Transfer Exit (0x37)
// ================================================================

bool ECUProgrammer::step_request_transfer_exit() {
  update_state(ProgrammingState::ExitingTransfer);
  
  auto resp = client_.request_transfer_exit({});
  
  if (!resp.ok) {
    handle_failure("Failed to exit transfer", resp.nrc.code);
    return false;
  }
  
  log("Transfer exit completed");
  return true;
}

// ================================================================
// Step 9: Re-enable DTC and Communications
// ================================================================

bool ECUProgrammer::step_reenable_services() {
  update_state(ProgrammingState::ReenablingServices);
  
  // Re-enable DTC setting (0x85 0x01)
  auto dtc_resp = client_.control_dtc_setting(static_cast<uint8_t>(DTCSettingType::On));
  if (!dtc_resp.ok) {
    log("Warning: Failed to re-enable DTC setting");
    // Don't fail - this is cleanup
  }
  
  // Re-enable communications (0x28 0x00)
  auto comm_resp = client_.communication_control(
    static_cast<uint8_t>(CommunicationControlType::EnableRxAndTx),
    0x01  // Normal messages
  );
  if (!comm_resp.ok) {
    log("Warning: Failed to re-enable communications");
    // Don't fail - this is cleanup
  }
  
  log("Services re-enabled");
  return true;
}

// ================================================================
// Step 10: ECU Reset (0x11)
// ================================================================

bool ECUProgrammer::step_ecu_reset(EcuResetType reset_type) {
  update_state(ProgrammingState::ResettingECU);
  
  auto resp = client_.ecu_reset(reset_type);
  
  if (!resp.ok) {
    log("Warning: ECU reset failed (may have reset anyway)");
    // Don't fail - ECU may have reset before sending response
  }
  
  log("ECU reset command sent");
  return true;
}

// ================================================================
// Main Programming Function
// ================================================================

ProgrammingResult ECUProgrammer::program_ecu(const std::vector<uint8_t>& firmware_data,
                                             const ProgrammingConfig& config) {
  // Initialize
  config_ = config;
  result_ = ProgrammingResult{};
  state_ = ProgrammingState::Idle;
  abort_requested_ = false;
  block_counter_ = config.block_counter_start;
  max_block_length_ = 0;
  
  auto start_time = std::chrono::steady_clock::now();
  
  // Step 1: Enter programming session
  if (!step_enter_programming_session()) {
    return result_;
  }
  
  // Step 2: Security access
  if (!config.skip_security) {
    if (!step_security_access(config.security_level, config.key_calculator)) {
      return result_;
    }
  }
  
  // Step 3: Disable DTC setting
  if (!step_disable_dtc_setting()) {
    return result_;
  }
  
  // Step 4: Disable communications (optional)
  if (!config.skip_communication_disable) {
    if (!step_disable_communications()) {
      return result_;
    }
  }
  
  // Step 5: Erase memory
  if (!config.skip_erase) {
    if (!step_erase_memory(config.erase_routine_id, config.erase_option_record, config.erase_timeout)) {
      return result_;
    }
  }
  
  // Step 6: Request download
  if (!step_request_download(config.start_address, firmware_data.size(),
                             config.address_length_format, config.data_format_identifier)) {
    return result_;
  }
  
  // Step 7: Transfer data
  if (!step_transfer_data(firmware_data)) {
    return result_;
  }
  
  // Step 8: Request transfer exit
  if (!step_request_transfer_exit()) {
    return result_;
  }
  
  // Step 9: Re-enable services
  step_reenable_services();  // Best effort
  
  // Step 10: ECU reset
  if (config.perform_reset_after_flash) {
    step_ecu_reset();
  }
  
  // Success!
  auto end_time = std::chrono::steady_clock::now();
  result_.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  result_.success = true;
  result_.final_state = ProgrammingState::Completed;
  update_state(ProgrammingState::Completed, "Programming completed successfully");
  
  if (config.completion_callback) {
    config.completion_callback(true, "Programming completed successfully");
  }
  
  return result_;
}

// ================================================================
// Helper Functions
// ================================================================

PositiveOrNegative ECUProgrammer::handle_response_pending(
    const std::function<PositiveOrNegative()>& request_fn,
    std::chrono::milliseconds extended_timeout) {
  
  auto start = std::chrono::steady_clock::now();
  
  while (true) {
    auto resp = request_fn();
    
    if (resp.ok || resp.nrc.code != NegativeResponseCode::RequestCorrectlyReceived_ResponsePending) {
      return resp;
    }
    
    // Check timeout
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed > extended_timeout) {
      resp.ok = false;
      resp.nrc.code = NegativeResponseCode::GeneralReject;
      return resp;
    }
    
    // Wait before retry
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

bool ECUProgrammer::wait_for_routine_completion(RoutineId routine_id,
                                                std::chrono::milliseconds timeout) {
  auto start = std::chrono::steady_clock::now();
  
  while (true) {
    // Check timeout
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed > timeout) {
      handle_failure("Routine timeout", NegativeResponseCode::GeneralReject);
      return false;
    }
    
    // Request routine results
    auto resp = client_.routine_control(RoutineAction::Result, routine_id, {});
    
    if (resp.ok) {
      return true;
    }
    
    if (resp.nrc.code == NegativeResponseCode::RequestCorrectlyReceived_ResponsePending) {
      // Still running, wait and retry
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }
    
    // Error
    handle_failure("Routine failed", resp.nrc.code);
    return false;
  }
}

uint32_t ECUProgrammer::parse_max_block_length(const std::vector<uint8_t>& response) {
  // Response format: [lengthFormatId, maxNumberOfBlockLength...]
  if (response.empty()) {
    return 0;
  }
  
  uint8_t length_fmt = response[0];
  uint8_t num_bytes = length_fmt & 0x0F;  // Low nibble
  
  if (num_bytes == 0 || num_bytes > 4 || response.size() < (1 + num_bytes)) {
    return 0;
  }
  
  uint32_t max_length = 0;
  for (uint8_t i = 0; i < num_bytes; ++i) {
    max_length = (max_length << 8) | response[1 + i];
  }
  
  return max_length;
}

uint16_t ECUProgrammer::calculate_block_count(uint32_t data_size, uint16_t block_size) {
  if (block_size == 0) {
    return 0;
  }
  return (data_size + block_size - 1) / block_size;
}

std::vector<uint8_t> ECUProgrammer::encode_address_and_size(uint32_t address, uint32_t size,
                                                             uint8_t addr_len_fmt) {
  std::vector<uint8_t> result;
  
  // Add format identifier
  result.push_back(addr_len_fmt);
  
  // High nibble = address length, low nibble = size length (ISO-14229)
  uint8_t addr_bytes = (addr_len_fmt >> 4) & 0x0F;
  uint8_t size_bytes = addr_len_fmt & 0x0F;
  
  // Encode address (big-endian)
  for (int i = addr_bytes - 1; i >= 0; --i) {
    result.push_back((address >> (i * 8)) & 0xFF);
  }
  
  // Encode size (big-endian)
  for (int i = size_bytes - 1; i >= 0; --i) {
    result.push_back((size >> (i * 8)) & 0xFF);
  }
  
  return result;
}

// ================================================================
// High-Level Programming Functions
// ================================================================

ProgrammingResult flash_ecu(Client& client,
                            const std::vector<uint8_t>& firmware_data,
                            uint32_t start_address,
                            const std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>& key_calculator) {
  ProgrammingConfig config;
  config.start_address = start_address;
  config.memory_size = firmware_data.size();
  config.key_calculator = key_calculator;
  
  ECUProgrammer programmer(client);
  return programmer.program_ecu(firmware_data, config);
}

bool verify_ecu_memory([[maybe_unused]] Client& client,
                      [[maybe_unused]] uint32_t address,
                      [[maybe_unused]] const std::vector<uint8_t>& expected_data,
                      [[maybe_unused]] const std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>& key_calculator) {
  // TODO: Implement using RequestUpload (0x35) and TransferData
  // This is a placeholder for future implementation
  return false;
}

} // namespace uds
