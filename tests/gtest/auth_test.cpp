/**
 * @file auth_test.cpp
 * @brief Tests for authentication/authorization (uds_auth.cpp)
 */

#include <gtest/gtest.h>
#include "uds_auth.hpp"
#include <thread>

using namespace uds::auth;

// ============================================================================
// Permission Tests
// ============================================================================

TEST(PermissionTest, PermissionValues) {
  EXPECT_EQ(static_cast<uint32_t>(Permission::ReadDID), 0x00000001u);
  EXPECT_EQ(static_cast<uint32_t>(Permission::WriteDID), 0x00000100u);
  EXPECT_EQ(static_cast<uint32_t>(Permission::SecurityAccess), 0x00020000u);
}

TEST(PermissionTest, HasPermission) {
  Permission combined = Permission::ReadDID | Permission::WriteDID;
  EXPECT_TRUE(has_permission(combined, Permission::ReadDID));
  EXPECT_TRUE(has_permission(combined, Permission::WriteDID));
  EXPECT_FALSE(has_permission(combined, Permission::SecurityAccess));
}

TEST(PermissionTest, CombinePermissions) {
  Permission perm = Permission::ReadDID | Permission::ReadDTC;
  EXPECT_TRUE(has_permission(perm, Permission::ReadDID));
  EXPECT_TRUE(has_permission(perm, Permission::ReadDTC));
  EXPECT_FALSE(has_permission(perm, Permission::WriteDID));
}

TEST(PermissionTest, AllPermissions) {
  Permission perm = Permission::All;
  EXPECT_TRUE(has_permission(perm, Permission::ReadDID));
  EXPECT_TRUE(has_permission(perm, Permission::WriteDID));
  EXPECT_TRUE(has_permission(perm, Permission::SecurityAccess));
  EXPECT_TRUE(has_permission(perm, Permission::AllProgramming));
}

// ============================================================================
// Role Tests
// ============================================================================

TEST(RoleTest, RoleValues) {
  EXPECT_EQ(static_cast<uint8_t>(Role::None), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(Role::Viewer), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(Role::Technician), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(Role::Programmer), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(Role::Engineer), 0x05);
  EXPECT_EQ(static_cast<uint8_t>(Role::OEM), 0x06);
}

// ============================================================================
// RoleDefinition Tests
// ============================================================================

TEST(RoleDefinitionTest, RoleDefinitionHas) {
  RoleDefinition def;
  def.permissions = Permission::ReadDID | Permission::ReadDTC;
  
  EXPECT_TRUE(def.has(Permission::ReadDID));
  EXPECT_TRUE(def.has(Permission::ReadDTC));
  EXPECT_FALSE(def.has(Permission::WriteDID));
}


// ============================================================================
// UserInfo Tests
// ============================================================================

TEST(UserInfoTest, DefaultUser) {
  UserInfo user;
  EXPECT_TRUE(user.user_id.empty());
  EXPECT_EQ(user.role, Role::None);
}

TEST(UserInfoTest, CreateUser) {
  UserInfo user;
  user.user_id = "tech001";
  user.name = "Test Technician";
  user.role = Role::Technician;
  
  EXPECT_EQ(user.user_id, "tech001");
  EXPECT_EQ(user.role, Role::Technician);
}

// ============================================================================
// SessionInfo Tests
// ============================================================================

TEST(SessionInfoTest, DefaultSession) {
  SessionInfo session;
  EXPECT_FALSE(session.is_active);
  EXPECT_FALSE(session.security_unlocked);
  EXPECT_EQ(session.security_level, 0u);
}

// ============================================================================
// AuthResult Tests
// ============================================================================

TEST(AuthResultTest, AllowResult) {
  auto result = AuthResult::allow();
  EXPECT_TRUE(result.authorized);
  EXPECT_TRUE(result.reason.empty());
}

TEST(AuthResultTest, DenyResult) {
  auto result = AuthResult::deny("Access denied");
  EXPECT_FALSE(result.authorized);
  EXPECT_EQ(result.reason, "Access denied");
}

TEST(AuthResultTest, DenyWithPermission) {
  auto result = AuthResult::deny("No permission", Permission::AllProgramming);
  EXPECT_FALSE(result.authorized);
  EXPECT_EQ(static_cast<uint32_t>(result.missing_permissions), 
            static_cast<uint32_t>(Permission::AllProgramming));
}

TEST(AuthResultTest, RequireSecurity) {
  auto result = AuthResult::require_security(0x03);
  EXPECT_FALSE(result.authorized);
  EXPECT_EQ(result.required_security_level, 0x03);
}

// ============================================================================
// RoleBasedPolicy Tests
// ============================================================================

TEST(RoleBasedPolicyTest, SetAndGetRoleDefinition) {
  RoleBasedPolicy policy;
  
  RoleDefinition def;
  def.role = Role::Viewer;
  def.permissions = Permission::ReadDID | Permission::ReadDTC;
  
  policy.set_role_definition(Role::Viewer, def);
  
  auto retrieved = policy.get_role_definition(Role::Viewer);
  EXPECT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved->role, Role::Viewer);
  
  auto missing = policy.get_role_definition(Role::OEM);
  EXPECT_EQ(missing, nullptr);
}

TEST(RoleBasedPolicyTest, AuthorizeSuccess) {
  RoleBasedPolicy policy;
  
  RoleDefinition def;
  def.role = Role::Viewer;
  def.permissions = Permission::ReadDID | Permission::ReadDTC;
  policy.set_role_definition(Role::Viewer, def);
  
  SessionInfo session;
  session.is_active = true;
  session.user.role = Role::Viewer;
  
  auto result = policy.authorize(session, Permission::ReadDID, {});
  EXPECT_TRUE(result.authorized);
}

TEST(RoleBasedPolicyTest, AuthorizeDenied) {
  RoleBasedPolicy policy;
  
  RoleDefinition def;
  def.role = Role::Viewer;
  def.permissions = Permission::ReadDID;
  policy.set_role_definition(Role::Viewer, def);
  
  SessionInfo session;
  session.is_active = true;
  session.user.role = Role::Viewer;
  
  auto result = policy.authorize(session, Permission::WriteDID, {});
  EXPECT_FALSE(result.authorized);
}

TEST(RoleBasedPolicyTest, UndefinedRole) {
  RoleBasedPolicy policy;
  
  SessionInfo session;
  session.is_active = true;
  session.user.role = Role::Viewer;  // Not defined in policy
  
  auto result = policy.authorize(session, Permission::ReadDID, {});
  EXPECT_FALSE(result.authorized);
}

TEST(RoleBasedPolicyTest, SecurityLevelRequired) {
  RoleBasedPolicy policy;
  
  RoleDefinition def;
  def.role = Role::Programmer;
  def.permissions = Permission::AllProgramming;
  def.required_security_level = 0x03;
  policy.set_role_definition(Role::Programmer, def);
  
  SessionInfo session;
  session.is_active = true;
  session.user.role = Role::Programmer;
  session.security_unlocked = false;
  
  auto result = policy.authorize(session, Permission::RequestDownload, {});
  EXPECT_FALSE(result.authorized);
  EXPECT_EQ(result.required_security_level, 0x03);
}

TEST(RoleBasedPolicyTest, SecurityLevelMet) {
  RoleBasedPolicy policy;
  
  RoleDefinition def;
  def.role = Role::Programmer;
  def.permissions = Permission::AllProgramming;
  def.required_security_level = 0x03;
  policy.set_role_definition(Role::Programmer, def);
  
  SessionInfo session;
  session.is_active = true;
  session.user.role = Role::Programmer;
  session.security_unlocked = true;
  session.security_level = 0x03;
  
  auto result = policy.authorize(session, Permission::RequestDownload, {});
  EXPECT_TRUE(result.authorized);
}

// ============================================================================
// AuthManager Tests
// ============================================================================

TEST(AuthManagerTest, Construction) {
  AuthManager mgr;
  EXPECT_FALSE(mgr.has_active_session());
}

TEST(AuthManagerTest, StartSession) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Technician;
  
  std::string session_id = mgr.start_session(user);
  
  EXPECT_FALSE(session_id.empty());
  EXPECT_TRUE(mgr.has_active_session());
}

TEST(AuthManagerTest, EndSession) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  EXPECT_TRUE(mgr.has_active_session());
  
  mgr.end_session();
  EXPECT_FALSE(mgr.has_active_session());
}

TEST(AuthManagerTest, TouchSession) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  auto before = mgr.current_session().last_activity;
  
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  mgr.touch_session();
  
  auto after = mgr.current_session().last_activity;
  EXPECT_GT(after, before);
}

TEST(AuthManagerTest, SetSecurityLevel) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Programmer;
  
  mgr.start_session(user);
  mgr.set_security_level(0x03, true);
  
  EXPECT_TRUE(mgr.current_session().security_unlocked);
  EXPECT_EQ(mgr.current_session().security_level, 0x03);
}

TEST(AuthManagerTest, MeetsSecurityLevel) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Programmer;
  
  mgr.start_session(user);
  mgr.set_security_level(0x03, true);
  
  EXPECT_TRUE(mgr.meets_security_level(0x01));
  EXPECT_TRUE(mgr.meets_security_level(0x03));
  EXPECT_FALSE(mgr.meets_security_level(0x05));
}

TEST(AuthManagerTest, CanPerform) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "tech001";
  user.role = Role::Technician;
  
  mgr.start_session(user);
  
  EXPECT_TRUE(mgr.can_perform(Permission::ReadDID));
}

TEST(AuthManagerTest, CheckAuthorization) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "tech001";
  user.role = Role::Technician;
  
  mgr.start_session(user);
  
  auto result = mgr.check_authorization(Permission::ReadDID);
  EXPECT_TRUE(result.authorized);
}

TEST(AuthManagerTest, SetCurrentRole) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  mgr.set_current_role(Role::Technician);
  
  EXPECT_EQ(mgr.current_role(), Role::Technician);
}

TEST(AuthManagerTest, DefineRole) {
  AuthManager mgr;
  
  RoleDefinition custom;
  custom.role = Role::Custom1;
  custom.name = "Custom Role";
  custom.permissions = Permission::ReadDID | Permission::ReadDTC;
  
  mgr.define_role(custom);
  
  auto def = mgr.get_role(Role::Custom1);
  EXPECT_NE(def, nullptr);
  EXPECT_EQ(def->name, "Custom Role");
}

// ============================================================================
// Audit Log Tests
// ============================================================================

TEST(AuthManagerTest, AuditLogEnabled) {
  AuthManager mgr;
  mgr.set_audit_enabled(true);
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  
  auto log = mgr.audit_log();
  EXPECT_FALSE(log.empty());
}

TEST(AuthManagerTest, AuditLogDisabled) {
  AuthManager mgr;
  mgr.set_audit_enabled(false);
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  
  auto log = mgr.audit_log();
  EXPECT_TRUE(log.empty());
}

TEST(AuthManagerTest, ClearAuditLog) {
  AuthManager mgr;
  mgr.set_audit_enabled(true);
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  mgr.clear_audit_log();
  
  EXPECT_TRUE(mgr.audit_log().empty());
}

TEST(AuthManagerTest, MaxAuditEntries) {
  AuthManager mgr;
  mgr.set_audit_enabled(true);
  mgr.set_max_audit_entries(10);
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  EXPECT_TRUE(mgr.has_active_session());
}

TEST(AuthManagerTest, RequirePermission) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "tech001";
  user.role = Role::Technician;
  
  mgr.start_session(user);
  
  auto result = mgr.require(Permission::ReadDID);
  EXPECT_TRUE(result.authorized);
}

TEST(AuthManagerTest, SetCurrentRoleWithoutSession) {
  AuthManager mgr;
  
  mgr.set_current_role(Role::Technician);
  
  EXPECT_TRUE(mgr.has_active_session());
  EXPECT_EQ(mgr.current_role(), Role::Technician);
}

TEST(AuthManagerTest, LinkSecurityManager) {
  AuthManager mgr;
  uds::security::SecurityManager sec_mgr;
  
  mgr.link_security_manager(&sec_mgr);
  // Just verify it doesn't crash
  EXPECT_TRUE(true);
}

TEST(AuthManagerTest, SetCustomPolicy) {
  AuthManager mgr;
  
  auto policy = std::make_unique<RoleBasedPolicy>();
  mgr.set_policy(std::move(policy));
  
  EXPECT_NE(mgr.policy(), nullptr);
}

TEST(AuthManagerTest, AuditCallback) {
  AuthManager mgr;
  mgr.set_audit_enabled(true);
  
  int callback_count = 0;
  mgr.set_audit_callback([&](const AuthAuditEntry&) {
    callback_count++;
  });
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  mgr.require(Permission::ReadDID);
  
  EXPECT_GT(callback_count, 0);
}

TEST(AuthManagerTest, AuditLogTrimming) {
  AuthManager mgr;
  mgr.set_audit_enabled(true);
  mgr.set_max_audit_entries(2);
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  mgr.require(Permission::ReadDID);
  mgr.require(Permission::ReadDTC);
  mgr.require(Permission::WriteDID);
  
  EXPECT_LE(mgr.audit_log().size(), 2u);
}

TEST(AuthManagerTest, MeetsSecurityLevelZero) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  
  // Level 0 always passes
  EXPECT_TRUE(mgr.meets_security_level(0));
}

TEST(AuthManagerTest, CheckAuthorizationNoPolicy) {
  AuthManager mgr;
  mgr.set_policy(nullptr);
  
  UserInfo user;
  user.user_id = "test_user";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  
  auto result = mgr.check_authorization(Permission::ReadDID);
  EXPECT_FALSE(result.authorized);
}

// ============================================================================
// AuthGuard Tests
// ============================================================================

TEST(AuthGuardTest, AuthorizedOperation) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "tech001";
  user.role = Role::Technician;
  
  mgr.start_session(user);
  
  {
    AuthGuard guard(mgr, Permission::ReadDID, "ReadDID");
    EXPECT_TRUE(guard.is_authorized());
    guard.complete(true);
  }
}

TEST(AuthGuardTest, DeniedOperation) {
  AuthManager mgr;
  
  UserInfo user;
  user.user_id = "viewer001";
  user.role = Role::Viewer;
  
  mgr.start_session(user);
  
  AuthGuard guard(mgr, Permission::AllProgramming, "Programming");
  EXPECT_FALSE(guard.is_authorized());
}

// ============================================================================
// Role Name Tests
// ============================================================================

TEST(RoleNameTest, AllRoleNames) {
  EXPECT_STREQ(role_name(Role::None), "None");
  EXPECT_STREQ(role_name(Role::Viewer), "Viewer");
  EXPECT_STREQ(role_name(Role::Technician), "Technician");
  EXPECT_STREQ(role_name(Role::Programmer), "Programmer");
  EXPECT_STREQ(role_name(Role::Calibration), "Calibration");
  EXPECT_STREQ(role_name(Role::Engineer), "Engineer");
  EXPECT_STREQ(role_name(Role::OEM), "OEM");
  EXPECT_STREQ(role_name(Role::Development), "Development");
  EXPECT_STREQ(role_name(Role::Custom1), "Custom1");
  EXPECT_STREQ(role_name(Role::Custom2), "Custom2");
  EXPECT_STREQ(role_name(Role::Custom3), "Custom3");
}

TEST(RoleNameTest, UnknownRole) {
  EXPECT_STREQ(role_name(static_cast<Role>(0xFF)), "Unknown");
}

// ============================================================================
// Permission Name Tests
// ============================================================================

TEST(PermissionNameTest, AllPermissionNames) {
  EXPECT_STREQ(permission_name(Permission::ReadDID), "ReadDID");
  EXPECT_STREQ(permission_name(Permission::WriteDID), "WriteDID");
  EXPECT_STREQ(permission_name(Permission::ReadDTC), "ReadDTC");
  EXPECT_STREQ(permission_name(Permission::ClearDTC), "ClearDTC");
  EXPECT_STREQ(permission_name(Permission::RoutineControl), "RoutineControl");
  EXPECT_STREQ(permission_name(Permission::IOControl), "IOControl");
  EXPECT_STREQ(permission_name(Permission::SecurityAccess), "SecurityAccess");
  EXPECT_STREQ(permission_name(Permission::RequestDownload), "RequestDownload");
  EXPECT_STREQ(permission_name(Permission::RequestUpload), "RequestUpload");
  EXPECT_STREQ(permission_name(Permission::TransferData), "TransferData");
  EXPECT_STREQ(permission_name(Permission::ECUReset), "ECUReset");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
