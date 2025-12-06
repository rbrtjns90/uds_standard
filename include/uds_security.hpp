#ifndef UDS_SECURITY_HPP
#define UDS_SECURITY_HPP

/**
 * @file uds_security.hpp
 * @brief Security Services - ISO 14229-1:2013 Sections 9.4, 9.8
 * 
 * This header provides types and functions for:
 * - SecurityAccess (0x27) - Section 9.4 (p. 47)
 * - SecuredDataTransmission (0x84) - Section 9.8 (p. 66)
 * 
 * ============================================================================
 * ISO 14229-1:2013 SECURITY REFERENCE
 * ============================================================================
 * 
 * SecurityAccess (0x27) - Section 9.4 (p. 47):
 * 
 *   Purpose: Unlock protected services via seed/key authentication
 * 
 *   Sub-functions:
 *     Odd values (0x01, 0x03, ...): requestSeed - Request random seed
 *     Even values (0x02, 0x04, ...): sendKey - Send calculated key
 * 
 *   Security Levels:
 *     0x01-0x02: Level 1 - Basic security (read protected data)
 *     0x03-0x04: Level 2 - Extended security (write calibration)
 *     0x05-0x06: Level 3 - Programming security (flash ECU)
 *     0x07-0x42: Additional levels (OEM-specific)
 *     0x43-0x5E: ISO Reserved
 *     0x5F-0x60: ISO Reserved for extended levels
 *     0x61-0x7E: System supplier specific
 * 
 *   Request Format:
 *     requestSeed: [0x27] [securityAccessType (odd)]
 *     sendKey:     [0x27] [securityAccessType (even)] [securityKey...]
 * 
 *   Response Format:
 *     requestSeed: [0x67] [securityAccessType] [securitySeed...]
 *     sendKey:     [0x67] [securityAccessType]
 * 
 *   Security NRCs (Annex A, Table A.1):
 *     0x35: invalidKey - Wrong key sent
 *     0x36: exceededNumberOfAttempts - Too many failed attempts
 *     0x37: requiredTimeDelayNotExpired - Lockout timer active
 * 
 * SecuredDataTransmission (0x84) - Section 9.8 (p. 66):
 * 
 *   Purpose: Encrypt/decrypt diagnostic data for secure transmission
 * 
 *   Request Format:
 *     [0x84] [securityDataRequestRecord...]
 * 
 *   Response Format (Section 8.25.3, p. 395):
 *     [0xC4] [securityDataResponseRecord...]
 * 
 * Usage:
 * @code
 *   uds::security::SecurityManager mgr;
 *   mgr.set_algorithm(std::make_unique<uds::security::AES128Algorithm>());
 *   mgr.set_key(0x01, key_bytes);
 *   
 *   auto result = mgr.unlock_security_level(client, 0x01, seed_key_callback);
 *   auto encrypted = mgr.secured_data_transmission(client, plaintext);
 * @endcode
 */

#include "uds.hpp"
#include <memory>
#include <functional>
#include <chrono>
#include <map>

namespace uds {
namespace security {

// ============================================================================
// Security Levels
// ISO 14229-1:2013 Section 8.3.2.2, Table 36 (p. 102)
// ============================================================================

/**
 * @brief Standard security level definitions
 * 
 * ISO 14229-1:2013 Table 36 (p. 102):
 * Odd values = requestSeed, Even values = sendKey
 */
namespace Level {
  constexpr uint8_t Basic           = 0x01;  // Basic read protection
  constexpr uint8_t Extended        = 0x03;  // Extended diagnostics
  constexpr uint8_t Programming     = 0x05;  // Flash programming
  constexpr uint8_t Calibration     = 0x07;  // Calibration access
  constexpr uint8_t EOL             = 0x09;  // End-of-line programming
  constexpr uint8_t Development     = 0x0B;  // Development access
  // 0x0D-0x41: OEM-specific levels
  // 0x43-0x5E: System supplier specific
  // 0x61-0x7E: Reserved
}

// ============================================================================
// Security Algorithm Interface
// ============================================================================

/**
 * Abstract interface for security algorithms.
 * Implement this to support custom OEM algorithms.
 */
class ISecurityAlgorithm {
public:
  virtual ~ISecurityAlgorithm() = default;
  
  /**
   * Calculate key from seed for SecurityAccess
   * 
   * @param seed Seed bytes from ECU
   * @param level Security level being unlocked
   * @param secret Optional secret/mask for key calculation
   * @return Calculated key bytes
   */
  virtual std::vector<uint8_t> calculate_key(
      const std::vector<uint8_t>& seed,
      uint8_t level,
      const std::vector<uint8_t>& secret = {}) = 0;
  
  /**
   * Encrypt data for SecuredDataTransmission
   */
  virtual std::vector<uint8_t> encrypt(
      const std::vector<uint8_t>& plaintext,
      const std::vector<uint8_t>& key) = 0;
  
  /**
   * Decrypt data from SecuredDataTransmission
   */
  virtual std::vector<uint8_t> decrypt(
      const std::vector<uint8_t>& ciphertext,
      const std::vector<uint8_t>& key) = 0;
  
  /**
   * Get algorithm identifier
   */
  virtual uint16_t algorithm_id() const = 0;
  
  /**
   * Get algorithm name
   */
  virtual const char* name() const = 0;
};

// ============================================================================
// Built-in Security Algorithms
// ============================================================================

/**
 * Simple XOR-based algorithm (for testing only, NOT secure!)
 */
class XORAlgorithm : public ISecurityAlgorithm {
public:
  std::vector<uint8_t> calculate_key(
      const std::vector<uint8_t>& seed,
      uint8_t level,
      const std::vector<uint8_t>& secret = {}) override;
  
  std::vector<uint8_t> encrypt(
      const std::vector<uint8_t>& plaintext,
      const std::vector<uint8_t>& key) override;
  
  std::vector<uint8_t> decrypt(
      const std::vector<uint8_t>& ciphertext,
      const std::vector<uint8_t>& key) override;
  
  uint16_t algorithm_id() const override { return 0x0001; }
  const char* name() const override { return "XOR"; }
};

/**
 * AES-128 algorithm (requires external crypto library in production)
 * This implementation provides the interface; actual AES operations
 * should be linked to a proper crypto library (OpenSSL, mbedTLS, etc.)
 */
class AES128Algorithm : public ISecurityAlgorithm {
public:
  std::vector<uint8_t> calculate_key(
      const std::vector<uint8_t>& seed,
      uint8_t level,
      const std::vector<uint8_t>& secret = {}) override;
  
  std::vector<uint8_t> encrypt(
      const std::vector<uint8_t>& plaintext,
      const std::vector<uint8_t>& key) override;
  
  std::vector<uint8_t> decrypt(
      const std::vector<uint8_t>& ciphertext,
      const std::vector<uint8_t>& key) override;
  
  uint16_t algorithm_id() const override { return 0x0002; }
  const char* name() const override { return "AES-128"; }
};

/**
 * Common OEM seed-key algorithm pattern
 * Many OEMs use variations of: key = f(seed XOR mask) with rotations
 */
class OEMSeedKeyAlgorithm : public ISecurityAlgorithm {
public:
  explicit OEMSeedKeyAlgorithm(uint32_t mask = 0xFFFFFFFF, 
                               uint8_t rotations = 0)
      : mask_(mask), rotations_(rotations) {}
  
  std::vector<uint8_t> calculate_key(
      const std::vector<uint8_t>& seed,
      uint8_t level,
      const std::vector<uint8_t>& secret = {}) override;
  
  std::vector<uint8_t> encrypt(
      const std::vector<uint8_t>& plaintext,
      const std::vector<uint8_t>& key) override;
  
  std::vector<uint8_t> decrypt(
      const std::vector<uint8_t>& ciphertext,
      const std::vector<uint8_t>& key) override;
  
  uint16_t algorithm_id() const override { return 0x8000; }  // OEM range
  const char* name() const override { return "OEM Seed-Key"; }

private:
  uint32_t mask_;
  uint8_t rotations_;
};

// ============================================================================
// Security Parameters
// ============================================================================

struct SecurityParameters {
  uint16_t key_identifier{0};
  uint16_t algorithm_id{0};
  std::vector<uint8_t> encryption_key;
  std::vector<uint8_t> secret_mask;  // For seed-key calculation
  
  // Session-specific
  uint8_t security_level{0};
  bool is_unlocked{false};
  std::chrono::steady_clock::time_point unlock_time;
};

// ============================================================================
// Security Access State
// ============================================================================

struct SecurityState {
  uint8_t current_level{0};
  bool is_locked{true};
  uint8_t failed_attempts{0};
  std::chrono::steady_clock::time_point lockout_until;
  std::chrono::steady_clock::time_point last_activity;
  
  // Per-level unlock status
  std::map<uint8_t, bool> level_unlocked;
  
  bool is_level_unlocked(uint8_t level) const {
    auto it = level_unlocked.find(level);
    return it != level_unlocked.end() && it->second;
  }
};

// ============================================================================
// Audit Log Entry
// ============================================================================

struct SecurityAuditEntry {
  std::chrono::system_clock::time_point timestamp;
  uint8_t security_level{0};
  enum class Action {
    SeedRequested,
    KeySent,
    UnlockSuccess,
    UnlockFailed,
    Lockout,
    SecuredTransmission,
    KeyRotation
  } action;
  bool success{false};
  std::string details;
};

// ============================================================================
// Result Type
// ============================================================================

template<typename T>
struct Result {
  bool ok{false};
  T value{};
  NegativeResponse nrc{};
  
  static Result success(const T& v) {
    Result r; r.ok = true; r.value = v; return r;
  }
  
  static Result error(const NegativeResponse& n) {
    Result r; r.ok = false; r.nrc = n; return r;
  }
  
  static Result error() {
    Result r; r.ok = false; return r;
  }
};

template<>
struct Result<void> {
  bool ok{false};
  NegativeResponse nrc{};
  
  static Result success() {
    Result r; r.ok = true; return r;
  }
  
  static Result error(const NegativeResponse& n) {
    Result r; r.ok = false; r.nrc = n; return r;
  }
  
  static Result error() {
    Result r; r.ok = false; return r;
  }
};

// ============================================================================
// Seed-Key Callback Type
// ============================================================================

/**
 * Callback for custom seed-to-key calculation.
 * Used when the algorithm is not known or is OEM-specific.
 * 
 * @param seed Seed bytes from ECU
 * @param level Security level
 * @return Calculated key bytes
 */
using SeedKeyCallback = std::function<std::vector<uint8_t>(
    const std::vector<uint8_t>& seed, uint8_t level)>;

// ============================================================================
// Security Manager
// ============================================================================

class SecurityManager {
public:
  SecurityManager();
  ~SecurityManager() = default;
  
  // ==========================================================================
  // Algorithm Management
  // ==========================================================================
  
  /**
   * Set the security algorithm to use
   */
  void set_algorithm(std::unique_ptr<ISecurityAlgorithm> algorithm);
  
  /**
   * Get current algorithm
   */
  ISecurityAlgorithm* algorithm() const { return algorithm_.get(); }
  
  // ==========================================================================
  // Key Management
  // ==========================================================================
  
  /**
   * Set encryption key for a security level
   */
  void set_key(uint8_t level, const std::vector<uint8_t>& key);
  
  /**
   * Set secret/mask for seed-key calculation
   */
  void set_secret(uint8_t level, const std::vector<uint8_t>& secret);
  
  /**
   * Get key for a security level
   */
  std::vector<uint8_t> get_key(uint8_t level) const;
  
  /**
   * Rotate key for a security level
   */
  void rotate_key(uint8_t level, const std::vector<uint8_t>& new_key);
  
  // ==========================================================================
  // SecurityAccess (0x27) Operations
  // ==========================================================================
  
  /**
   * Request seed from ECU
   * 
   * @param client UDS client
   * @param level Security level (odd number: 0x01, 0x03, etc.)
   * @return Seed bytes on success
   */
  Result<std::vector<uint8_t>> request_seed(Client& client, uint8_t level);
  
  /**
   * Send key to ECU
   * 
   * @param client UDS client
   * @param level Security level (even number: 0x02, 0x04, etc.)
   * @param key Calculated key bytes
   * @return Success/failure
   */
  Result<void> send_key(Client& client, uint8_t level, 
                        const std::vector<uint8_t>& key);
  
  /**
   * Unlock a security level using the configured algorithm
   * 
   * @param client UDS client
   * @param level Security level to unlock
   * @return Success/failure
   */
  Result<void> unlock_level(Client& client, uint8_t level);
  
  /**
   * Unlock a security level using a custom callback
   * 
   * @param client UDS client
   * @param level Security level to unlock
   * @param callback Custom seed-to-key function
   * @return Success/failure
   */
  Result<void> unlock_level(Client& client, uint8_t level,
                            SeedKeyCallback callback);
  
  /**
   * Check if a security level is currently unlocked
   */
  bool is_unlocked(uint8_t level) const;
  
  /**
   * Get current security state
   */
  const SecurityState& state() const { return state_; }
  
  // ==========================================================================
  // SecuredDataTransmission (0x84) Operations
  // ==========================================================================
  
  /**
   * Send data using SecuredDataTransmission
   * 
   * @param client UDS client
   * @param data Data to send (will be encrypted)
   * @return Response data (decrypted) on success
   */
  Result<std::vector<uint8_t>> secured_data_transmission(
      Client& client,
      const std::vector<uint8_t>& data);
  
  /**
   * Encrypt data for transmission
   */
  std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
                               uint8_t level = 0);
  
  /**
   * Decrypt received data
   */
  std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                               uint8_t level = 0);
  
  // ==========================================================================
  // Audit Logging
  // ==========================================================================
  
  /**
   * Enable/disable audit logging
   */
  void set_audit_enabled(bool enabled) { audit_enabled_ = enabled; }
  
  /**
   * Get audit log entries
   */
  const std::vector<SecurityAuditEntry>& audit_log() const { return audit_log_; }
  
  /**
   * Clear audit log
   */
  void clear_audit_log() { audit_log_.clear(); }
  
  /**
   * Set maximum audit log size (0 = unlimited)
   */
  void set_max_audit_entries(size_t max) { max_audit_entries_ = max; }
  
  // ==========================================================================
  // Configuration
  // ==========================================================================
  
  /**
   * Set lockout parameters
   */
  void set_lockout_params(uint8_t max_attempts, 
                          std::chrono::seconds lockout_duration);
  
  /**
   * Check if currently locked out
   */
  bool is_locked_out() const;
  
  /**
   * Get time remaining in lockout
   */
  std::chrono::seconds lockout_remaining() const;
  
  /**
   * Reset security state (e.g., after session change)
   */
  void reset_state();

private:
  void log_audit(uint8_t level, SecurityAuditEntry::Action action,
                 bool success, const std::string& details = "");
  
  std::unique_ptr<ISecurityAlgorithm> algorithm_;
  std::map<uint8_t, std::vector<uint8_t>> keys_;      // level -> key
  std::map<uint8_t, std::vector<uint8_t>> secrets_;   // level -> secret
  SecurityState state_;
  
  // Audit
  bool audit_enabled_{true};
  std::vector<SecurityAuditEntry> audit_log_;
  size_t max_audit_entries_{1000};
  
  // Lockout
  uint8_t max_attempts_{3};
  std::chrono::seconds lockout_duration_{10};
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get human-readable name for security level
 */
const char* level_name(uint8_t level);

/**
 * Check if level is valid (odd for seed request)
 */
bool is_valid_seed_level(uint8_t level);

/**
 * Get corresponding key level for seed level
 */
uint8_t seed_to_key_level(uint8_t seed_level);

/**
 * Format security audit entry for logging
 */
std::string format_audit_entry(const SecurityAuditEntry& entry);

} // namespace security
} // namespace uds

#endif // UDS_SECURITY_HPP
