/**
 * @file security_test.cpp
 * @brief Google Test suite for security functionality
 */

#include <gtest/gtest.h>
#include "uds_security.hpp"
#include "uds_auth.hpp"

using namespace uds;
using namespace uds::security;
using namespace uds::auth;

// ============================================================================
// Security Level Tests
// ============================================================================

TEST(SecurityLevelTest, LevelValues) {
  EXPECT_EQ(Level::Basic, 0x01);
  EXPECT_EQ(Level::Extended, 0x03);
  EXPECT_EQ(Level::Programming, 0x05);
  EXPECT_EQ(Level::Calibration, 0x07);
  EXPECT_EQ(Level::EOL, 0x09);
  EXPECT_EQ(Level::Development, 0x0B);
}

TEST(SecurityLevelTest, LevelName) {
  EXPECT_STREQ(level_name(Level::Basic), "Basic");
  EXPECT_STREQ(level_name(Level::Programming), "Programming");
  EXPECT_STREQ(level_name(Level::Development), "Development");
  EXPECT_STREQ(level_name(0x0D), "OEM-Specific");
}

TEST(SecurityLevelTest, ValidSeedLevel) {
  // Seed levels are odd
  EXPECT_TRUE(is_valid_seed_level(0x01));
  EXPECT_TRUE(is_valid_seed_level(0x03));
  EXPECT_TRUE(is_valid_seed_level(0x05));
  
  // Even levels are for key send
  EXPECT_FALSE(is_valid_seed_level(0x02));
  EXPECT_FALSE(is_valid_seed_level(0x04));
}

TEST(SecurityLevelTest, SeedToKeyLevel) {
  EXPECT_EQ(seed_to_key_level(0x01), 0x02);
  EXPECT_EQ(seed_to_key_level(0x03), 0x04);
  EXPECT_EQ(seed_to_key_level(0x05), 0x06);
}

// ============================================================================
// XOR Algorithm Tests
// ============================================================================

TEST(XORAlgorithmTest, CalculateKey) {
  XORAlgorithm algo;
  
  std::vector<uint8_t> seed = {0x12, 0x34, 0x56, 0x78};
  std::vector<uint8_t> secret = {0xFF, 0xFF, 0xFF, 0xFF};
  
  auto key = algo.calculate_key(seed, 0x01, secret);
  
  ASSERT_EQ(key.size(), seed.size());
  // XOR with 0xFF inverts all bits
  EXPECT_EQ(key[0], 0xED);
  EXPECT_EQ(key[1], 0xCB);
  EXPECT_EQ(key[2], 0xA9);
  EXPECT_EQ(key[3], 0x87);
}

TEST(XORAlgorithmTest, EncryptDecryptSymmetric) {
  XORAlgorithm algo;
  
  std::vector<uint8_t> plaintext = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> key = {0xAA, 0xBB, 0xCC, 0xDD};
  
  auto ciphertext = algo.encrypt(plaintext, key);
  auto decrypted = algo.decrypt(ciphertext, key);
  
  EXPECT_EQ(decrypted, plaintext);
}

TEST(XORAlgorithmTest, AlgorithmId) {
  XORAlgorithm algo;
  EXPECT_EQ(algo.algorithm_id(), 0x0001);
  EXPECT_STREQ(algo.name(), "XOR");
}

// ============================================================================
// AES Algorithm Tests
// ============================================================================

TEST(AES128AlgorithmTest, AlgorithmId) {
  AES128Algorithm algo;
  EXPECT_EQ(algo.algorithm_id(), 0x0002);
  EXPECT_STREQ(algo.name(), "AES-128");
}

TEST(AES128AlgorithmTest, KeyDerivation) {
  AES128Algorithm algo;
  
  std::vector<uint8_t> seed = {0x01, 0x02, 0x03, 0x04};
  auto key = algo.calculate_key(seed, 0x01, {});
  
  // AES-128 key should be 16 bytes
  EXPECT_EQ(key.size(), 16);
}

// ============================================================================
// OEM Algorithm Tests
// ============================================================================

TEST(OEMAlgorithmTest, CalculateKeyWithMask) {
  OEMSeedKeyAlgorithm algo(0xCAFEBABE, 0);
  
  std::vector<uint8_t> seed = {0x12, 0x34, 0x56, 0x78};
  auto key = algo.calculate_key(seed, 0x01, {});
  
  EXPECT_EQ(key.size(), 4);
}

TEST(OEMAlgorithmTest, AlgorithmId) {
  OEMSeedKeyAlgorithm algo;
  EXPECT_EQ(algo.algorithm_id(), 0x8000);
  EXPECT_STREQ(algo.name(), "OEM Seed-Key");
}

// ============================================================================
// Security Manager Tests
// ============================================================================

TEST(SecurityManagerTest, DefaultAlgorithm) {
  SecurityManager mgr;
  EXPECT_NE(mgr.algorithm(), nullptr);
  // Default is XOR
  EXPECT_EQ(mgr.algorithm()->algorithm_id(), 0x0001);
}

TEST(SecurityManagerTest, SetAlgorithm) {
  SecurityManager mgr;
  mgr.set_algorithm(std::make_unique<AES128Algorithm>());
  
  EXPECT_EQ(mgr.algorithm()->algorithm_id(), 0x0002);
}

TEST(SecurityManagerTest, KeyManagement) {
  SecurityManager mgr;
  
  std::vector<uint8_t> key = {0x01, 0x02, 0x03, 0x04};
  mgr.set_key(Level::Basic, key);
  
  auto retrieved = mgr.get_key(Level::Basic);
  EXPECT_EQ(retrieved, key);
  
  // Non-existent key returns empty
  auto empty = mgr.get_key(Level::Programming);
  EXPECT_TRUE(empty.empty());
}

TEST(SecurityManagerTest, LockoutParams) {
  SecurityManager mgr;
  mgr.set_lockout_params(5, std::chrono::seconds(30));
  
  EXPECT_FALSE(mgr.is_locked_out());
}

TEST(SecurityManagerTest, ResetState) {
  SecurityManager mgr;
  mgr.reset_state();
  
  EXPECT_FALSE(mgr.is_unlocked(Level::Basic));
  EXPECT_FALSE(mgr.is_unlocked(Level::Programming));
}

TEST(SecurityManagerTest, AuditLogging) {
  SecurityManager mgr;
  mgr.set_audit_enabled(true);
  mgr.clear_audit_log();
  
  EXPECT_TRUE(mgr.audit_log().empty());
}

// ============================================================================
// Authorization Role Tests
// ============================================================================

TEST(AuthRoleTest, RoleValues) {
  EXPECT_EQ(static_cast<uint8_t>(Role::None), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(Role::Viewer), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(Role::Technician), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(Role::Programmer), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(Role::Engineer), 0x05);
  EXPECT_EQ(static_cast<uint8_t>(Role::OEM), 0x06);
}

TEST(AuthRoleTest, RoleName) {
  EXPECT_STREQ(role_name(Role::None), "None");
  EXPECT_STREQ(role_name(Role::Viewer), "Viewer");
  EXPECT_STREQ(role_name(Role::Technician), "Technician");
  EXPECT_STREQ(role_name(Role::Programmer), "Programmer");
  EXPECT_STREQ(role_name(Role::Engineer), "Engineer");
  EXPECT_STREQ(role_name(Role::OEM), "OEM");
}

// ============================================================================
// Permission Tests
// ============================================================================

TEST(PermissionTest, PermissionValues) {
  EXPECT_EQ(static_cast<uint32_t>(Permission::ReadDID), 0x00000001);
  EXPECT_EQ(static_cast<uint32_t>(Permission::ReadDTC), 0x00000002);
  EXPECT_EQ(static_cast<uint32_t>(Permission::WriteDID), 0x00000100);
  EXPECT_EQ(static_cast<uint32_t>(Permission::ClearDTC), 0x00000400);
}

TEST(PermissionTest, PermissionCombination) {
  Permission combined = Permission::ReadDID | Permission::ReadDTC;
  
  EXPECT_TRUE(has_permission(combined, Permission::ReadDID));
  EXPECT_TRUE(has_permission(combined, Permission::ReadDTC));
  EXPECT_FALSE(has_permission(combined, Permission::WriteDID));
}

TEST(PermissionTest, PermissionName) {
  EXPECT_STREQ(permission_name(Permission::ReadDID), "ReadDID");
  EXPECT_STREQ(permission_name(Permission::ClearDTC), "ClearDTC");
  EXPECT_STREQ(permission_name(Permission::SecurityAccess), "SecurityAccess");
}

// ============================================================================
// Auth Manager Tests
// ============================================================================

TEST(AuthManagerTest, DefaultState) {
  AuthManager mgr;
  EXPECT_FALSE(mgr.has_active_session());
}

TEST(AuthManagerTest, SetRole) {
  AuthManager mgr;
  mgr.set_current_role(Role::Technician);
  
  EXPECT_EQ(mgr.current_role(), Role::Technician);
  EXPECT_TRUE(mgr.has_active_session());
}

TEST(AuthManagerTest, TechnicianPermissions) {
  AuthManager mgr;
  mgr.set_current_role(Role::Technician);
  
  // Technician should have read and clear DTC permissions
  EXPECT_TRUE(mgr.can_perform(Permission::ReadDID));
  EXPECT_TRUE(mgr.can_perform(Permission::ReadDTC));
  EXPECT_TRUE(mgr.can_perform(Permission::ClearDTC));
  
  // But not programming
  EXPECT_FALSE(mgr.can_perform(Permission::RequestDownload));
}

TEST(AuthManagerTest, ViewerPermissions) {
  AuthManager mgr;
  mgr.set_current_role(Role::Viewer);
  
  // Viewer has read-only
  EXPECT_TRUE(mgr.can_perform(Permission::ReadDID));
  EXPECT_TRUE(mgr.can_perform(Permission::ReadDTC));
  
  // No write or clear
  EXPECT_FALSE(mgr.can_perform(Permission::WriteDID));
  EXPECT_FALSE(mgr.can_perform(Permission::ClearDTC));
}

TEST(AuthManagerTest, SessionManagement) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "test_user";
  user.name = "Test User";
  user.role = Role::Engineer;
  
  auto session_id = mgr.start_session(user);
  
  EXPECT_FALSE(session_id.empty());
  EXPECT_TRUE(mgr.has_active_session());
  EXPECT_EQ(mgr.current_session().user.user_id, "test_user");
  
  mgr.end_session();
  EXPECT_FALSE(mgr.has_active_session());
}

// ============================================================================
// Default Role Definitions Tests
// ============================================================================

TEST(DefaultRolesTest, ViewerDefinition) {
  auto def = DefaultRoles::viewer();
  EXPECT_EQ(def.role, Role::Viewer);
  EXPECT_TRUE(def.has(Permission::ReadDID));
  EXPECT_FALSE(def.has(Permission::WriteDID));
}

TEST(DefaultRolesTest, TechnicianDefinition) {
  auto def = DefaultRoles::technician();
  EXPECT_EQ(def.role, Role::Technician);
  EXPECT_TRUE(def.has(Permission::ClearDTC));
  EXPECT_TRUE(def.has(Permission::RoutineControl));
}

TEST(DefaultRolesTest, ProgrammerDefinition) {
  auto def = DefaultRoles::programmer();
  EXPECT_EQ(def.role, Role::Programmer);
  EXPECT_TRUE(def.has(Permission::RequestDownload));
  EXPECT_TRUE(def.has(Permission::TransferData));
  EXPECT_EQ(def.required_security_level, Level::Programming);
}

// ============================================================================
// Session ID Generation Test
// ============================================================================

TEST(SessionTest, GenerateUniqueSessionIds) {
  auto id1 = generate_session_id();
  auto id2 = generate_session_id();
  
  EXPECT_FALSE(id1.empty());
  EXPECT_FALSE(id2.empty());
  EXPECT_NE(id1, id2);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
