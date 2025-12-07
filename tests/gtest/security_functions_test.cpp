/**
 * @file security_functions_test.cpp
 * @brief Comprehensive tests for security functions (uds_security.cpp)
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "uds_security.hpp"
#include <queue>

using namespace uds;
using namespace uds::security;

// Mock Transport
class MockTransport : public Transport {
public:
  void set_address(const Address& addr) override { addr_ = addr; }
  const Address& address() const override { return addr_; }
  
  bool request_response(const std::vector<uint8_t>& tx,
                        std::vector<uint8_t>& rx,
                        std::chrono::milliseconds) override {
    last_request_ = tx;
    if (!responses_.empty()) { rx = responses_.front(); responses_.pop(); return true; }
    return false;
  }
  
  void queue_response(const std::vector<uint8_t>& r) { responses_.push(r); }
  const std::vector<uint8_t>& last_request() const { return last_request_; }
  void reset() { while (!responses_.empty()) responses_.pop(); last_request_.clear(); }

private:
  Address addr_;
  std::queue<std::vector<uint8_t>> responses_;
  std::vector<uint8_t> last_request_;
};

class SecurityTest : public ::testing::Test {
protected:
  void SetUp() override { transport_.reset(); }
  MockTransport transport_;
};

// ============================================================================
// XOR Algorithm Tests
// ============================================================================

TEST(XORAlgorithmTest, CalculateKeyWithSecret) {
  XORAlgorithm algo;
  
  std::vector<uint8_t> seed = {0x12, 0x34, 0x56, 0x78};
  std::vector<uint8_t> secret = {0xFF, 0xFF, 0xFF, 0xFF};
  
  auto key = algo.calculate_key(seed, 0x01, secret);
  
  EXPECT_EQ(key.size(), seed.size());
  EXPECT_EQ(key[0], 0x12 ^ 0xFF);
  EXPECT_EQ(key[1], 0x34 ^ 0xFF);
}

TEST(XORAlgorithmTest, CalculateKeyWithoutSecret) {
  XORAlgorithm algo;
  
  std::vector<uint8_t> seed = {0x12, 0x34, 0x56, 0x78};
  
  auto key = algo.calculate_key(seed, 0x01, {});
  
  EXPECT_EQ(key.size(), seed.size());
  // Level-based transformation: key[i] = seed[i] ^ (level + i + 1)
  EXPECT_EQ(key[0], 0x12 ^ (0x01 + 0 + 1));
}

TEST(XORAlgorithmTest, Encrypt) {
  XORAlgorithm algo;
  
  std::vector<uint8_t> plaintext = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> key = {0xFF, 0xFF, 0xFF, 0xFF};
  
  auto ciphertext = algo.encrypt(plaintext, key);
  
  EXPECT_EQ(ciphertext.size(), plaintext.size());
  EXPECT_EQ(ciphertext[0], 0x01 ^ 0xFF);
}

TEST(XORAlgorithmTest, EncryptEmptyKey) {
  XORAlgorithm algo;
  
  std::vector<uint8_t> plaintext = {0x01, 0x02, 0x03, 0x04};
  
  auto ciphertext = algo.encrypt(plaintext, {});
  
  EXPECT_EQ(ciphertext, plaintext);  // No change with empty key
}

TEST(XORAlgorithmTest, DecryptSymmetric) {
  XORAlgorithm algo;
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> key = {0xAB, 0xCD, 0xEF, 0x12};
  
  auto encrypted = algo.encrypt(data, key);
  auto decrypted = algo.decrypt(encrypted, key);
  
  EXPECT_EQ(decrypted, data);
}

// ============================================================================
// AES128 Algorithm Tests (Stub Implementation)
// ============================================================================

TEST(AES128AlgorithmTest, CalculateKey) {
  AES128Algorithm algo;
  
  std::vector<uint8_t> seed = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> secret = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
  
  auto key = algo.calculate_key(seed, 0x01, secret);
  
  EXPECT_EQ(key.size(), 16u);  // AES-128 = 16 bytes
}

TEST(AES128AlgorithmTest, EncryptDecrypt) {
  AES128Algorithm algo;
  
  std::vector<uint8_t> plaintext = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  std::vector<uint8_t> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                               0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
  
  auto encrypted = algo.encrypt(plaintext, key);
  auto decrypted = algo.decrypt(encrypted, key);
  
  EXPECT_EQ(decrypted, plaintext);
}

TEST(AES128AlgorithmTest, EncryptInvalidKey) {
  AES128Algorithm algo;
  
  std::vector<uint8_t> plaintext = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> short_key = {0x01, 0x02};  // Too short
  
  auto result = algo.encrypt(plaintext, short_key);
  
  EXPECT_EQ(result, plaintext);  // Returns unchanged on invalid key
}

TEST(AES128AlgorithmTest, DecryptEmpty) {
  AES128Algorithm algo;
  
  std::vector<uint8_t> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                               0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
  
  auto result = algo.decrypt({}, key);
  
  EXPECT_TRUE(result.empty());
}

// ============================================================================
// OEM Seed-Key Algorithm Tests
// ============================================================================

TEST(OEMSeedKeyTest, CalculateKey) {
  OEMSeedKeyAlgorithm algo(0x12345678, 3);
  
  std::vector<uint8_t> seed = {0xAB, 0xCD, 0xEF, 0x01};
  
  auto key = algo.calculate_key(seed, 0x01, {});
  
  EXPECT_EQ(key.size(), 4u);
}

TEST(OEMSeedKeyTest, CalculateKeyWithSecret) {
  OEMSeedKeyAlgorithm algo(0x12345678, 3);
  
  std::vector<uint8_t> seed = {0xAB, 0xCD, 0xEF, 0x01};
  std::vector<uint8_t> secret = {0x11, 0x22, 0x33, 0x44};
  
  auto key = algo.calculate_key(seed, 0x01, secret);
  
  EXPECT_EQ(key.size(), 4u);
}

TEST(OEMSeedKeyTest, EncryptDecrypt) {
  OEMSeedKeyAlgorithm algo(0x12345678, 3);
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> key = {0xAB, 0xCD, 0xEF, 0x12};
  
  auto encrypted = algo.encrypt(data, key);
  auto decrypted = algo.decrypt(encrypted, key);
  
  EXPECT_EQ(decrypted, data);
}

// ============================================================================
// SecurityManager Tests
// ============================================================================

TEST_F(SecurityTest, SecurityManagerConstruction) {
  SecurityManager mgr;
  
  // Default algorithm should be XOR
  EXPECT_FALSE(mgr.is_locked_out());
}

TEST_F(SecurityTest, SetAlgorithm) {
  SecurityManager mgr;
  
  mgr.set_algorithm(std::make_unique<AES128Algorithm>());
  
  // Should not throw
  EXPECT_TRUE(true);
}

TEST_F(SecurityTest, SetAndGetKey) {
  SecurityManager mgr;
  
  std::vector<uint8_t> key = {0x01, 0x02, 0x03, 0x04};
  mgr.set_key(0x01, key);
  
  auto retrieved = mgr.get_key(0x01);
  EXPECT_EQ(retrieved, key);
}

TEST_F(SecurityTest, GetKeyNotSet) {
  SecurityManager mgr;
  
  auto key = mgr.get_key(0x99);
  EXPECT_TRUE(key.empty());
}

TEST_F(SecurityTest, RotateKey) {
  SecurityManager mgr;
  mgr.set_audit_enabled(true);
  
  std::vector<uint8_t> old_key = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> new_key = {0x05, 0x06, 0x07, 0x08};
  
  mgr.set_key(0x01, old_key);
  mgr.rotate_key(0x01, new_key);
  
  EXPECT_EQ(mgr.get_key(0x01), new_key);
}

TEST_F(SecurityTest, RequestSeed) {
  Client client(transport_);
  SecurityManager mgr;
  
  transport_.queue_response({0x67, 0x01, 0xAB, 0xCD, 0xEF, 0x12});
  
  auto result = mgr.request_seed(client, 0x01);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.value.size(), 4u);
}

TEST_F(SecurityTest, RequestSeedAlreadyUnlocked) {
  Client client(transport_);
  SecurityManager mgr;
  
  // Zero seed means already unlocked
  transport_.queue_response({0x67, 0x01, 0x00, 0x00, 0x00, 0x00});
  
  auto result = mgr.request_seed(client, 0x01);
  
  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(mgr.is_unlocked(0x01));
}

TEST_F(SecurityTest, RequestSeedFailure) {
  Client client(transport_);
  SecurityManager mgr;
  
  transport_.queue_response({0x7F, 0x27, 0x12});  // SubFunctionNotSupported
  
  auto result = mgr.request_seed(client, 0x01);
  
  EXPECT_FALSE(result.ok);
}

TEST_F(SecurityTest, SendKey) {
  Client client(transport_);
  SecurityManager mgr;
  
  transport_.queue_response({0x67, 0x02});
  
  std::vector<uint8_t> key = {0x12, 0x34, 0x56, 0x78};
  auto result = mgr.send_key(client, 0x02, key);
  
  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(mgr.is_unlocked(0x02));
}

TEST_F(SecurityTest, SendKeyInvalid) {
  Client client(transport_);
  SecurityManager mgr;
  
  transport_.queue_response({0x7F, 0x27, 0x35});  // InvalidKey
  
  std::vector<uint8_t> key = {0x00, 0x00, 0x00, 0x00};
  auto result = mgr.send_key(client, 0x02, key);
  
  EXPECT_FALSE(result.ok);
}

TEST_F(SecurityTest, UnlockLevel) {
  Client client(transport_);
  SecurityManager mgr;
  mgr.set_secret(0x01, {0xFF, 0xFF, 0xFF, 0xFF});
  
  // Queue seed response
  transport_.queue_response({0x67, 0x01, 0x12, 0x34, 0x56, 0x78});
  // Queue key accepted response
  transport_.queue_response({0x67, 0x02});
  
  auto result = mgr.unlock_level(client, 0x01);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(SecurityTest, UnlockLevelWithCallback) {
  Client client(transport_);
  SecurityManager mgr;
  
  transport_.queue_response({0x67, 0x01, 0x12, 0x34, 0x56, 0x78});
  transport_.queue_response({0x67, 0x02});
  
  auto result = mgr.unlock_level(client, 0x01, 
    [](const std::vector<uint8_t>& seed, uint8_t) {
      // Simple XOR callback
      std::vector<uint8_t> key = seed;
      for (auto& b : key) b ^= 0xFF;
      return key;
    });
  
  EXPECT_TRUE(result.ok);
}

TEST_F(SecurityTest, IsUnlocked) {
  Client client(transport_);
  SecurityManager mgr;
  
  EXPECT_FALSE(mgr.is_unlocked(0x01));
  
  transport_.queue_response({0x67, 0x01, 0x00, 0x00, 0x00, 0x00});  // Already unlocked
  mgr.request_seed(client, 0x01);
  
  EXPECT_TRUE(mgr.is_unlocked(0x01));
}

// ============================================================================
// Lockout Tests
// ============================================================================

TEST_F(SecurityTest, LockoutAfterMaxAttempts) {
  Client client(transport_);
  SecurityManager mgr;
  mgr.set_lockout_params(3, std::chrono::seconds(10));
  
  // Fail 3 times
  for (int i = 0; i < 3; i++) {
    transport_.queue_response({0x7F, 0x27, 0x35});  // InvalidKey
    mgr.send_key(client, 0x02, {0x00, 0x00, 0x00, 0x00});
  }
  
  EXPECT_TRUE(mgr.is_locked_out());
  EXPECT_GT(mgr.lockout_remaining().count(), 0);
}

TEST_F(SecurityTest, LockoutPreventsRequests) {
  Client client(transport_);
  SecurityManager mgr;
  mgr.set_lockout_params(1, std::chrono::seconds(10));
  
  // Fail once to trigger lockout
  transport_.queue_response({0x7F, 0x27, 0x35});
  mgr.send_key(client, 0x02, {0x00});
  
  // Next request should fail due to lockout
  auto result = mgr.request_seed(client, 0x01);
  
  EXPECT_FALSE(result.ok);
}

TEST_F(SecurityTest, ResetState) {
  Client client(transport_);
  SecurityManager mgr;
  
  transport_.queue_response({0x67, 0x01, 0x00, 0x00, 0x00, 0x00});
  mgr.request_seed(client, 0x01);
  
  EXPECT_TRUE(mgr.is_unlocked(0x01));
  
  mgr.reset_state();
  
  EXPECT_FALSE(mgr.is_unlocked(0x01));
}

// ============================================================================
// Encryption/Decryption Tests
// ============================================================================

TEST_F(SecurityTest, EncryptWithKey) {
  SecurityManager mgr;
  mgr.set_key(0x01, {0xFF, 0xFF, 0xFF, 0xFF});
  
  std::vector<uint8_t> plaintext = {0x01, 0x02, 0x03, 0x04};
  auto encrypted = mgr.encrypt(plaintext, 0x01);
  
  EXPECT_NE(encrypted, plaintext);
}

TEST_F(SecurityTest, DecryptWithKey) {
  SecurityManager mgr;
  mgr.set_key(0x01, {0xFF, 0xFF, 0xFF, 0xFF});
  
  std::vector<uint8_t> plaintext = {0x01, 0x02, 0x03, 0x04};
  auto encrypted = mgr.encrypt(plaintext, 0x01);
  auto decrypted = mgr.decrypt(encrypted, 0x01);
  
  EXPECT_EQ(decrypted, plaintext);
}

TEST_F(SecurityTest, EncryptWithoutKey) {
  SecurityManager mgr;
  
  std::vector<uint8_t> plaintext = {0x01, 0x02, 0x03, 0x04};
  auto result = mgr.encrypt(plaintext, 0x99);  // No key set
  
  EXPECT_EQ(result, plaintext);  // Returns unchanged
}

// ============================================================================
// Secured Data Transmission Tests
// ============================================================================

TEST_F(SecurityTest, SecuredDataTransmission) {
  Client client(transport_);
  SecurityManager mgr;
  mgr.set_key(0x01, {0xFF, 0xFF, 0xFF, 0xFF});
  
  transport_.queue_response({0xC4, 0x01, 0x02, 0x03, 0x04});
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  auto result = mgr.secured_data_transmission(client, data);
  
  EXPECT_TRUE(result.ok);
}

// ============================================================================
// Audit Log Tests
// ============================================================================

TEST_F(SecurityTest, AuditLogEnabled) {
  Client client(transport_);
  SecurityManager mgr;
  mgr.set_audit_enabled(true);
  mgr.set_max_audit_entries(100);
  
  transport_.queue_response({0x67, 0x01, 0xAB, 0xCD, 0xEF, 0x12});
  mgr.request_seed(client, 0x01);
  
  auto log = mgr.audit_log();
  EXPECT_FALSE(log.empty());
}

TEST_F(SecurityTest, AuditLogDisabled) {
  Client client(transport_);
  SecurityManager mgr;
  mgr.set_audit_enabled(false);
  
  transport_.queue_response({0x67, 0x01, 0xAB, 0xCD, 0xEF, 0x12});
  mgr.request_seed(client, 0x01);
  
  auto log = mgr.audit_log();
  EXPECT_TRUE(log.empty());
}

TEST_F(SecurityTest, ClearAuditLog) {
  Client client(transport_);
  SecurityManager mgr;
  mgr.set_audit_enabled(true);
  
  transport_.queue_response({0x67, 0x01, 0xAB, 0xCD, 0xEF, 0x12});
  mgr.request_seed(client, 0x01);
  
  mgr.clear_audit_log();
  
  EXPECT_TRUE(mgr.audit_log().empty());
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST(SecurityHelperTest, LevelName) {
  EXPECT_STREQ(level_name(Level::Basic), "Basic");
  EXPECT_STREQ(level_name(Level::Extended), "Extended");
  EXPECT_STREQ(level_name(Level::Programming), "Programming");
  EXPECT_STREQ(level_name(Level::Calibration), "Calibration");
  EXPECT_STREQ(level_name(Level::EOL), "End-of-Line");
  EXPECT_STREQ(level_name(Level::Development), "Development");
}

TEST(SecurityHelperTest, LevelNameOEM) {
  EXPECT_STREQ(level_name(0x0D), "OEM-Specific");
  EXPECT_STREQ(level_name(0x41), "OEM-Specific");
}

TEST(SecurityHelperTest, LevelNameSupplier) {
  EXPECT_STREQ(level_name(0x43), "Supplier-Specific");
  EXPECT_STREQ(level_name(0x5E), "Supplier-Specific");
}

TEST(SecurityHelperTest, IsValidSeedLevel) {
  EXPECT_TRUE(is_valid_seed_level(0x01));
  EXPECT_TRUE(is_valid_seed_level(0x03));
  EXPECT_TRUE(is_valid_seed_level(0x05));
  EXPECT_FALSE(is_valid_seed_level(0x02));  // Even = key level
  EXPECT_FALSE(is_valid_seed_level(0x00));
}

TEST(SecurityHelperTest, SeedToKeyLevel) {
  EXPECT_EQ(seed_to_key_level(0x01), 0x02);
  EXPECT_EQ(seed_to_key_level(0x03), 0x04);
  EXPECT_EQ(seed_to_key_level(0x05), 0x06);
}

TEST(SecurityHelperTest, FormatAuditEntry) {
  SecurityAuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.security_level = 0x01;
  entry.action = SecurityAuditEntry::Action::SeedRequested;
  entry.success = true;
  entry.details = "Test";
  
  std::string formatted = format_audit_entry(entry);
  
  EXPECT_FALSE(formatted.empty());
  EXPECT_NE(formatted.find("SEED_REQUEST"), std::string::npos);
}

TEST(SecurityHelperTest, FormatAuditEntryAllActions) {
  SecurityAuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.security_level = 0x01;
  entry.success = true;
  
  entry.action = SecurityAuditEntry::Action::KeySent;
  EXPECT_NE(format_audit_entry(entry).find("KEY_SENT"), std::string::npos);
  
  entry.action = SecurityAuditEntry::Action::UnlockSuccess;
  EXPECT_NE(format_audit_entry(entry).find("UNLOCK_SUCCESS"), std::string::npos);
  
  entry.action = SecurityAuditEntry::Action::UnlockFailed;
  EXPECT_NE(format_audit_entry(entry).find("UNLOCK_FAILED"), std::string::npos);
  
  entry.action = SecurityAuditEntry::Action::Lockout;
  EXPECT_NE(format_audit_entry(entry).find("LOCKOUT"), std::string::npos);
  
  entry.action = SecurityAuditEntry::Action::SecuredTransmission;
  EXPECT_NE(format_audit_entry(entry).find("SECURED_TX"), std::string::npos);
  
  entry.action = SecurityAuditEntry::Action::KeyRotation;
  EXPECT_NE(format_audit_entry(entry).find("KEY_ROTATION"), std::string::npos);
}

// ============================================================================
// Additional SecurityManager Tests
// ============================================================================

TEST(SecurityManagerTest, LockoutRemainingNotLocked) {
  SecurityManager mgr;
  
  auto remaining = mgr.lockout_remaining();
  EXPECT_EQ(remaining.count(), 0);
}


int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
