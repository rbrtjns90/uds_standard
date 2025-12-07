#include "uds_security.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace uds {
namespace security {

// ============================================================================
// XOR Algorithm Implementation
// ============================================================================

std::vector<uint8_t> XORAlgorithm::calculate_key(
    const std::vector<uint8_t>& seed,
    uint8_t level,
    const std::vector<uint8_t>& secret) {
  
  std::vector<uint8_t> key = seed;
  
  // XOR with secret if provided, otherwise use level-based mask
  if (!secret.empty()) {
    for (size_t i = 0; i < key.size(); ++i) {
      key[i] ^= secret[i % secret.size()];
    }
  } else {
    // Simple level-based transformation
    for (size_t i = 0; i < key.size(); ++i) {
      key[i] ^= (level + i + 1);
    }
  }
  
  return key;
}

std::vector<uint8_t> XORAlgorithm::encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key) {
  
  if (key.empty()) return plaintext;
  
  std::vector<uint8_t> ciphertext = plaintext;
  for (size_t i = 0; i < ciphertext.size(); ++i) {
    ciphertext[i] ^= key[i % key.size()];
  }
  return ciphertext;
}

std::vector<uint8_t> XORAlgorithm::decrypt(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key) {
  // XOR is symmetric
  return encrypt(ciphertext, key);
}

// ============================================================================
// AES-128 Algorithm Implementation (Stub)
// ============================================================================

std::vector<uint8_t> AES128Algorithm::calculate_key(
    const std::vector<uint8_t>& seed,
    uint8_t level,
    const std::vector<uint8_t>& secret) {
  
  // In production, this would use a proper KDF (Key Derivation Function)
  // For now, provide a placeholder that combines seed and secret
  
  std::vector<uint8_t> key(16, 0);  // AES-128 = 16 bytes
  
  // Copy seed
  for (size_t i = 0; i < seed.size() && i < 16; ++i) {
    key[i] = seed[i];
  }
  
  // XOR with secret
  if (!secret.empty()) {
    for (size_t i = 0; i < 16; ++i) {
      key[i] ^= secret[i % secret.size()];
    }
  }
  
  // Mix in level
  key[0] ^= level;
  
  return key;
}

std::vector<uint8_t> AES128Algorithm::encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key) {
  
  // STUB: In production, link to OpenSSL/mbedTLS for real AES
  // This is a placeholder that just XORs (NOT SECURE!)
  
  if (key.size() < 16) {
    return plaintext;  // Invalid key
  }
  
  // Pad to 16-byte boundary
  std::vector<uint8_t> padded = plaintext;
  size_t padding = 16 - (padded.size() % 16);
  if (padding < 16) {
    padded.insert(padded.end(), padding, static_cast<uint8_t>(padding));
  }
  
  // Placeholder: XOR each block with key (NOT real AES!)
  std::vector<uint8_t> ciphertext(padded.size());
  for (size_t i = 0; i < padded.size(); ++i) {
    ciphertext[i] = padded[i] ^ key[i % 16];
  }
  
  return ciphertext;
}

std::vector<uint8_t> AES128Algorithm::decrypt(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key) {
  
  // STUB: Placeholder implementation
  if (key.size() < 16 || ciphertext.empty()) {
    return ciphertext;
  }
  
  // Placeholder: XOR each block with key
  std::vector<uint8_t> plaintext(ciphertext.size());
  for (size_t i = 0; i < ciphertext.size(); ++i) {
    plaintext[i] = ciphertext[i] ^ key[i % 16];
  }
  
  // Remove PKCS7 padding
  if (!plaintext.empty()) {
    uint8_t padding = plaintext.back();
    if (padding > 0 && padding <= 16) {
      plaintext.resize(plaintext.size() - padding);
    }
  }
  
  return plaintext;
}

// ============================================================================
// OEM Seed-Key Algorithm Implementation
// ============================================================================

std::vector<uint8_t> OEMSeedKeyAlgorithm::calculate_key(
    const std::vector<uint8_t>& seed,
    uint8_t level,
    const std::vector<uint8_t>& secret) {
  
  (void)level;  // May be used in some OEM variants
  
  // Common OEM pattern: treat seed as 32-bit value, XOR with mask, rotate
  uint32_t seed_val = 0;
  for (size_t i = 0; i < seed.size() && i < 4; ++i) {
    seed_val = (seed_val << 8) | seed[i];
  }
  
  // Apply mask (from secret if provided, otherwise use configured mask)
  uint32_t mask = mask_;
  if (secret.size() >= 4) {
    mask = (static_cast<uint32_t>(secret[0]) << 24) |
           (static_cast<uint32_t>(secret[1]) << 16) |
           (static_cast<uint32_t>(secret[2]) << 8) |
           static_cast<uint32_t>(secret[3]);
  }
  
  uint32_t key_val = seed_val ^ mask;
  
  // Apply rotations
  for (uint8_t i = 0; i < rotations_; ++i) {
    // Rotate left
    key_val = (key_val << 1) | (key_val >> 31);
  }
  
  // Convert back to bytes
  std::vector<uint8_t> key(4);
  key[0] = (key_val >> 24) & 0xFF;
  key[1] = (key_val >> 16) & 0xFF;
  key[2] = (key_val >> 8) & 0xFF;
  key[3] = key_val & 0xFF;
  
  return key;
}

std::vector<uint8_t> OEMSeedKeyAlgorithm::encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key) {
  // OEM algorithms typically don't do encryption, just seed-key
  // Fall back to XOR for any encryption needs
  std::vector<uint8_t> result = plaintext;
  for (size_t i = 0; i < result.size(); ++i) {
    result[i] ^= key[i % key.size()];
  }
  return result;
}

std::vector<uint8_t> OEMSeedKeyAlgorithm::decrypt(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key) {
  return encrypt(ciphertext, key);  // XOR is symmetric
}

// ============================================================================
// Security Manager Implementation
// ============================================================================

SecurityManager::SecurityManager() {
  // Default to XOR algorithm
  algorithm_ = std::make_unique<XORAlgorithm>();
}

void SecurityManager::set_algorithm(std::unique_ptr<ISecurityAlgorithm> algorithm) {
  algorithm_ = std::move(algorithm);
}

void SecurityManager::set_key(uint8_t level, const std::vector<uint8_t>& key) {
  keys_[level] = key;
}

void SecurityManager::set_secret(uint8_t level, const std::vector<uint8_t>& secret) {
  secrets_[level] = secret;
}

std::vector<uint8_t> SecurityManager::get_key(uint8_t level) const {
  auto it = keys_.find(level);
  if (it != keys_.end()) {
    return it->second;
  }
  return {};
}

void SecurityManager::rotate_key(uint8_t level, const std::vector<uint8_t>& new_key) {
  keys_[level] = new_key;
  log_audit(level, SecurityAuditEntry::Action::KeyRotation, true, 
            "Key rotated for level " + std::to_string(level));
}

Result<std::vector<uint8_t>> SecurityManager::request_seed(Client& client, 
                                                            uint8_t level) {
  // Check lockout
  if (is_locked_out()) {
    log_audit(level, SecurityAuditEntry::Action::SeedRequested, false, "Locked out");
    return Result<std::vector<uint8_t>>::error();
  }
  
  log_audit(level, SecurityAuditEntry::Action::SeedRequested, true);
  
  auto result = client.security_access_request_seed(level);
  
  if (!result.ok) {
    return Result<std::vector<uint8_t>>::error(result.nrc);
  }
  
  // Response: [subFunction][seed...]
  if (result.payload.empty()) {
    return Result<std::vector<uint8_t>>::error();
  }
  
  // Extract seed (skip sub-function echo)
  std::vector<uint8_t> seed;
  if (result.payload.size() > 1) {
    seed.assign(result.payload.begin() + 1, result.payload.end());
  }
  
  // Check for "already unlocked" (seed = 0x00 or empty)
  bool all_zero = true;
  for (uint8_t b : seed) {
    if (b != 0) {
      all_zero = false;
      break;
    }
  }
  
  if (seed.empty() || all_zero) {
    // Already unlocked at this level
    state_.level_unlocked[level] = true;
    state_.is_locked = false;
    state_.current_level = level;
  }
  
  return Result<std::vector<uint8_t>>::success(seed);
}

Result<void> SecurityManager::send_key(Client& client, uint8_t level,
                                       const std::vector<uint8_t>& key) {
  log_audit(level, SecurityAuditEntry::Action::KeySent, true);
  
  auto result = client.security_access_send_key(level, key);
  
  if (!result.ok) {
    state_.failed_attempts++;
    
    // Check for lockout
    if (state_.failed_attempts >= max_attempts_) {
      state_.lockout_until = std::chrono::steady_clock::now() + lockout_duration_;
      log_audit(level, SecurityAuditEntry::Action::Lockout, false,
                "Max attempts exceeded");
    }
    
    log_audit(level, SecurityAuditEntry::Action::UnlockFailed, false,
              "NRC: " + std::to_string(static_cast<int>(result.nrc.code)));
    
    return Result<void>::error(result.nrc);
  }
  
  // Success
  state_.level_unlocked[level] = true;
  state_.is_locked = false;
  state_.current_level = level;
  state_.failed_attempts = 0;
  state_.last_activity = std::chrono::steady_clock::now();
  
  log_audit(level, SecurityAuditEntry::Action::UnlockSuccess, true);
  
  return Result<void>::success();
}

Result<void> SecurityManager::unlock_level(Client& client, uint8_t level) {
  if (!algorithm_) {
    return Result<void>::error();
  }
  
  // Request seed
  auto seed_result = request_seed(client, level);
  if (!seed_result.ok) {
    return Result<void>::error(seed_result.nrc);
  }
  
  // Check if already unlocked
  if (seed_result.value.empty() || is_unlocked(level)) {
    return Result<void>::success();
  }
  
  // Calculate key
  auto secret = secrets_.find(level);
  std::vector<uint8_t> key = algorithm_->calculate_key(
      seed_result.value,
      level,
      secret != secrets_.end() ? secret->second : std::vector<uint8_t>{});
  
  // Send key (level + 1 for sendKey)
  return send_key(client, level + 1, key);
}

Result<void> SecurityManager::unlock_level(Client& client, uint8_t level,
                                           SeedKeyCallback callback) {
  // Request seed
  auto seed_result = request_seed(client, level);
  if (!seed_result.ok) {
    return Result<void>::error(seed_result.nrc);
  }
  
  // Check if already unlocked
  if (seed_result.value.empty() || is_unlocked(level)) {
    return Result<void>::success();
  }
  
  // Calculate key using callback
  std::vector<uint8_t> key = callback(seed_result.value, level);
  
  // Send key
  return send_key(client, level + 1, key);
}

bool SecurityManager::is_unlocked(uint8_t level) const {
  return state_.is_level_unlocked(level);
}

Result<std::vector<uint8_t>> SecurityManager::secured_data_transmission(
    Client& client,
    const std::vector<uint8_t>& data) {
  
  // Encrypt the data
  auto encrypted = encrypt(data, state_.current_level);
  
  // Build request for SecuredDataTransmission (0x84)
  // Format: [securityDataRequestRecord]
  auto result = client.exchange(SID::SecuredDataTransmission, encrypted);
  
  log_audit(state_.current_level, SecurityAuditEntry::Action::SecuredTransmission,
            result.ok);
  
  if (!result.ok) {
    return Result<std::vector<uint8_t>>::error(result.nrc);
  }
  
  // Decrypt response
  auto decrypted = decrypt(result.payload, state_.current_level);
  
  return Result<std::vector<uint8_t>>::success(decrypted);
}

std::vector<uint8_t> SecurityManager::encrypt(const std::vector<uint8_t>& plaintext,
                                               uint8_t level) {
  if (!algorithm_) {
    return plaintext;
  }
  
  auto key = get_key(level);
  if (key.empty()) {
    key = get_key(0);  // Try default key
  }
  
  if (key.empty()) {
    return plaintext;
  }
  
  return algorithm_->encrypt(plaintext, key);
}

std::vector<uint8_t> SecurityManager::decrypt(const std::vector<uint8_t>& ciphertext,
                                               uint8_t level) {
  if (!algorithm_) {
    return ciphertext;
  }
  
  auto key = get_key(level);
  if (key.empty()) {
    key = get_key(0);
  }
  
  if (key.empty()) {
    return ciphertext;
  }
  
  return algorithm_->decrypt(ciphertext, key);
}

void SecurityManager::set_lockout_params(uint8_t max_attempts,
                                         std::chrono::seconds lockout_duration) {
  max_attempts_ = max_attempts;
  lockout_duration_ = lockout_duration;
}

bool SecurityManager::is_locked_out() const {
  if (state_.failed_attempts < max_attempts_) {
    return false;
  }
  return std::chrono::steady_clock::now() < state_.lockout_until;
}

std::chrono::seconds SecurityManager::lockout_remaining() const {
  if (!is_locked_out()) {
    return std::chrono::seconds(0);
  }
  auto remaining = state_.lockout_until - std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(remaining);
}

void SecurityManager::reset_state() {
  state_ = SecurityState{};
}

void SecurityManager::log_audit(uint8_t level, SecurityAuditEntry::Action action,
                                bool success, const std::string& details) {
  if (!audit_enabled_) {
    return;
  }
  
  SecurityAuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.security_level = level;
  entry.action = action;
  entry.success = success;
  entry.details = details;
  
  audit_log_.push_back(entry);
  
  // Trim if over limit
  if (max_audit_entries_ > 0 && audit_log_.size() > max_audit_entries_) {
    audit_log_.erase(audit_log_.begin());
  }
}

// ============================================================================
// Helper Functions
// ============================================================================

const char* level_name(uint8_t level) {
  switch (level) {
    case Level::Basic:        return "Basic";
    case Level::Extended:     return "Extended";
    case Level::Programming:  return "Programming";
    case Level::Calibration:  return "Calibration";
    case Level::EOL:          return "End-of-Line";
    case Level::Development:  return "Development";
    default:
      if (level >= 0x0D && level <= 0x41) return "OEM-Specific";
      if (level >= 0x43 && level <= 0x5E) return "Supplier-Specific";
      return "Unknown";
  }
}

bool is_valid_seed_level(uint8_t level) {
  // Seed request levels are odd: 0x01, 0x03, 0x05, etc.
  return (level & 0x01) == 0x01 && level <= 0x7E;
}

uint8_t seed_to_key_level(uint8_t seed_level) {
  // Key send level is seed level + 1
  return seed_level + 1;
}

std::string format_audit_entry(const SecurityAuditEntry& entry) {
  std::ostringstream oss;
  
  // Format timestamp
  auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
  oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  
  oss << " [Level " << static_cast<int>(entry.security_level) << "] ";
  
  switch (entry.action) {
    case SecurityAuditEntry::Action::SeedRequested:
      oss << "SEED_REQUEST";
      break;
    case SecurityAuditEntry::Action::KeySent:
      oss << "KEY_SENT";
      break;
    case SecurityAuditEntry::Action::UnlockSuccess:
      oss << "UNLOCK_SUCCESS";
      break;
    case SecurityAuditEntry::Action::UnlockFailed:
      oss << "UNLOCK_FAILED";
      break;
    case SecurityAuditEntry::Action::Lockout:
      oss << "LOCKOUT";
      break;
    case SecurityAuditEntry::Action::SecuredTransmission:
      oss << "SECURED_TX";
      break;
    case SecurityAuditEntry::Action::KeyRotation:
      oss << "KEY_ROTATION";
      break;
  }
  
  oss << " " << (entry.success ? "OK" : "FAIL");
  
  if (!entry.details.empty()) {
    oss << " - " << entry.details;
  }
  
  return oss.str();
}

} // namespace security
} // namespace uds
