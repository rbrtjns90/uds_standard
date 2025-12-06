#ifndef UDS_AUTH_HPP
#define UDS_AUTH_HPP

/*
  Authentication & Authorization Module
  
  This header provides role-based access control (RBAC) for UDS operations:
  - Define roles with specific permissions
  - Enforce access control before sensitive operations
  - Audit all authorization decisions
  - Support for multi-user diagnostic environments
  
  Roles (typical hierarchy):
  - Viewer: Read-only access to non-sensitive data
  - Technician: Basic diagnostics, read DTCs, clear codes
  - Programmer: Flash programming, calibration
  - Engineer: Full access including development features
  - OEM: Manufacturer-level access
  
  Permissions:
  - ReadDID: Read data identifiers
  - WriteDID: Write data identifiers
  - ReadDTC: Read diagnostic trouble codes
  - ClearDTC: Clear diagnostic trouble codes
  - RoutineControl: Execute routines
  - Programming: Flash programming operations
  - SecurityAccess: Unlock security levels
  - IOControl: Input/output control
  - Configuration: ECU configuration changes
  
  Usage:
    uds::auth::AuthManager auth;
    auth.set_current_role(uds::auth::Role::Technician);
    
    // Check permission before operation
    if (auth.can_perform(uds::auth::Permission::ClearDTC)) {
      uds::dtc::clear_all_dtcs(client);
    }
    
    // Or use the guard pattern
    auto guard = auth.require(uds::auth::Permission::Programming);
    if (guard.is_authorized()) {
      // Perform programming...
    }
*/

#include "uds.hpp"
#include "uds_security.hpp"
#include <set>
#include <map>
#include <functional>
#include <chrono>

namespace uds {
namespace auth {

// ============================================================================
// Roles
// ============================================================================

enum class Role : uint8_t {
  None        = 0x00,  // No access
  Viewer      = 0x01,  // Read-only, non-sensitive
  Technician  = 0x02,  // Basic diagnostics
  Programmer  = 0x03,  // Flash programming
  Calibration = 0x04,  // Calibration access
  Engineer    = 0x05,  // Full diagnostic access
  OEM         = 0x06,  // Manufacturer access
  Development = 0x07,  // Development/debug access
  
  // Custom roles
  Custom1     = 0x10,
  Custom2     = 0x11,
  Custom3     = 0x12
};

// ============================================================================
// Permissions
// ============================================================================

enum class Permission : uint32_t {
  // Read operations
  ReadDID               = 0x00000001,
  ReadDTC               = 0x00000002,
  ReadMemory            = 0x00000004,
  ReadScaling           = 0x00000008,
  ReadPeriodicData      = 0x00000010,
  
  // Write operations
  WriteDID              = 0x00000100,
  WriteMemory           = 0x00000200,
  ClearDTC              = 0x00000400,
  
  // Control operations
  RoutineControl        = 0x00001000,
  IOControl             = 0x00002000,
  CommunicationControl  = 0x00004000,
  DTCSettingControl     = 0x00008000,
  
  // Session/security
  SessionControl        = 0x00010000,
  SecurityAccess        = 0x00020000,
  LinkControl           = 0x00040000,
  
  // Programming
  RequestDownload       = 0x00100000,
  RequestUpload         = 0x00200000,
  TransferData          = 0x00400000,
  ECUReset              = 0x00800000,
  
  // Advanced
  Configuration         = 0x01000000,
  Development           = 0x02000000,
  OEMSpecific           = 0x04000000,
  
  // Composite permissions
  AllRead               = ReadDID | ReadDTC | ReadMemory | ReadScaling | ReadPeriodicData,
  AllWrite              = WriteDID | WriteMemory | ClearDTC,
  AllControl            = RoutineControl | IOControl | CommunicationControl | DTCSettingControl,
  AllProgramming        = RequestDownload | RequestUpload | TransferData | ECUReset,
  All                   = 0xFFFFFFFF
};

// Bitwise operators for Permission
inline Permission operator|(Permission a, Permission b) {
  return static_cast<Permission>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline Permission operator&(Permission a, Permission b) {
  return static_cast<Permission>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline Permission operator~(Permission a) {
  return static_cast<Permission>(~static_cast<uint32_t>(a));
}

inline bool has_permission(Permission set, Permission check) {
  return (static_cast<uint32_t>(set) & static_cast<uint32_t>(check)) != 0;
}

// ============================================================================
// Role Definition
// ============================================================================

struct RoleDefinition {
  Role role{Role::None};
  std::string name;
  std::string description;
  Permission permissions{};
  uint8_t required_security_level{0};  // Minimum security level needed
  
  bool has(Permission p) const {
    return has_permission(permissions, p);
  }
};

// ============================================================================
// User/Session Information
// ============================================================================

struct UserInfo {
  std::string user_id;
  std::string name;
  Role role{Role::None};
  std::chrono::system_clock::time_point login_time;
  std::string workstation;
  
  // Additional attributes
  std::map<std::string, std::string> attributes;
};

struct SessionInfo {
  std::string session_id;
  UserInfo user;
  std::chrono::system_clock::time_point start_time;
  std::chrono::system_clock::time_point last_activity;
  bool is_active{false};
  
  // Security state
  uint8_t security_level{0};
  bool security_unlocked{false};
};

// ============================================================================
// Authorization Result
// ============================================================================

struct AuthResult {
  bool authorized{false};
  std::string reason;
  Permission missing_permissions{};
  uint8_t required_security_level{0};
  
  static AuthResult allow() {
    AuthResult r;
    r.authorized = true;
    return r;
  }
  
  static AuthResult deny(const std::string& reason, 
                         Permission missing = Permission{}) {
    AuthResult r;
    r.authorized = false;
    r.reason = reason;
    r.missing_permissions = missing;
    return r;
  }
  
  static AuthResult require_security(uint8_t level) {
    AuthResult r;
    r.authorized = false;
    r.reason = "Security level " + std::to_string(level) + " required";
    r.required_security_level = level;
    return r;
  }
};

// ============================================================================
// Authorization Audit Entry
// ============================================================================

struct AuthAuditEntry {
  std::chrono::system_clock::time_point timestamp;
  std::string user_id;
  Role role{Role::None};
  Permission requested_permission{};
  bool authorized{false};
  std::string reason;
  std::string operation;
  std::string target;  // e.g., DID, routine ID, memory address
};

// ============================================================================
// Authorization Policy
// ============================================================================

/**
 * Custom authorization policy interface.
 * Implement this for complex authorization logic.
 */
class IAuthPolicy {
public:
  virtual ~IAuthPolicy() = default;
  
  /**
   * Check if operation is authorized
   * 
   * @param session Current session info
   * @param permission Required permission
   * @param context Additional context (e.g., target DID)
   * @return Authorization result
   */
  virtual AuthResult authorize(const SessionInfo& session,
                               Permission permission,
                               const std::map<std::string, std::string>& context) = 0;
};

/**
 * Default role-based policy
 */
class RoleBasedPolicy : public IAuthPolicy {
public:
  AuthResult authorize(const SessionInfo& session,
                       Permission permission,
                       const std::map<std::string, std::string>& context) override;
  
  void set_role_definition(Role role, const RoleDefinition& def);
  const RoleDefinition* get_role_definition(Role role) const;

private:
  std::map<Role, RoleDefinition> role_definitions_;
};

// ============================================================================
// Authorization Manager
// ============================================================================

class AuthManager {
public:
  AuthManager();
  ~AuthManager() = default;
  
  // ==========================================================================
  // Session Management
  // ==========================================================================
  
  /**
   * Start a new session
   */
  std::string start_session(const UserInfo& user);
  
  /**
   * End current session
   */
  void end_session();
  
  /**
   * Get current session info
   */
  const SessionInfo& current_session() const { return session_; }
  
  /**
   * Check if session is active
   */
  bool has_active_session() const { return session_.is_active; }
  
  /**
   * Update session activity timestamp
   */
  void touch_session();
  
  // ==========================================================================
  // Role Management
  // ==========================================================================
  
  /**
   * Set current role (for simple role-based access)
   */
  void set_current_role(Role role);
  
  /**
   * Get current role
   */
  Role current_role() const { return session_.user.role; }
  
  /**
   * Define a custom role
   */
  void define_role(const RoleDefinition& definition);
  
  /**
   * Get role definition
   */
  const RoleDefinition* get_role(Role role) const;
  
  // ==========================================================================
  // Authorization Checks
  // ==========================================================================
  
  /**
   * Check if current user can perform an operation
   */
  bool can_perform(Permission permission) const;
  
  /**
   * Check authorization with full result
   */
  AuthResult check_authorization(Permission permission,
                                 const std::map<std::string, std::string>& context = {}) const;
  
  /**
   * Require permission (throws or returns error if denied)
   */
  AuthResult require(Permission permission,
                     const std::map<std::string, std::string>& context = {});
  
  // ==========================================================================
  // Security Level Integration
  // ==========================================================================
  
  /**
   * Update security level from SecurityManager
   */
  void set_security_level(uint8_t level, bool unlocked);
  
  /**
   * Check if required security level is met
   */
  bool meets_security_level(uint8_t required) const;
  
  /**
   * Link to SecurityManager for automatic level tracking
   */
  void link_security_manager(security::SecurityManager* mgr);
  
  // ==========================================================================
  // Policy Management
  // ==========================================================================
  
  /**
   * Set custom authorization policy
   */
  void set_policy(std::unique_ptr<IAuthPolicy> policy);
  
  /**
   * Get current policy
   */
  IAuthPolicy* policy() const { return policy_.get(); }
  
  // ==========================================================================
  // Audit Logging
  // ==========================================================================
  
  /**
   * Enable/disable audit logging
   */
  void set_audit_enabled(bool enabled) { audit_enabled_ = enabled; }
  
  /**
   * Get audit log
   */
  const std::vector<AuthAuditEntry>& audit_log() const { return audit_log_; }
  
  /**
   * Clear audit log
   */
  void clear_audit_log() { audit_log_.clear(); }
  
  /**
   * Set audit callback for real-time logging
   */
  using AuditCallback = std::function<void(const AuthAuditEntry&)>;
  void set_audit_callback(AuditCallback callback) { audit_callback_ = callback; }
  
  /**
   * Set maximum audit entries
   */
  void set_max_audit_entries(size_t max) { max_audit_entries_ = max; }

private:
  void log_audit(Permission permission, bool authorized,
                 const std::string& reason = "",
                 const std::string& operation = "",
                 const std::string& target = "");
  
  void setup_default_roles();
  
  SessionInfo session_;
  std::unique_ptr<IAuthPolicy> policy_;
  std::map<Role, RoleDefinition> role_definitions_;
  security::SecurityManager* security_mgr_{nullptr};
  
  // Audit
  bool audit_enabled_{true};
  std::vector<AuthAuditEntry> audit_log_;
  size_t max_audit_entries_{1000};
  AuditCallback audit_callback_;
};

// ============================================================================
// RAII Authorization Guard
// ============================================================================

/**
 * RAII guard that checks authorization and logs the operation.
 * 
 * Usage:
 *   {
 *     AuthGuard guard(auth_mgr, Permission::Programming, "Flash ECU");
 *     if (!guard.is_authorized()) {
 *       return guard.result();  // Contains denial reason
 *     }
 *     // Perform operation...
 *   } // Operation completion logged
 */
class AuthGuard {
public:
  AuthGuard(AuthManager& mgr, Permission permission,
            const std::string& operation = "",
            const std::map<std::string, std::string>& context = {});
  
  ~AuthGuard();
  
  bool is_authorized() const { return result_.authorized; }
  const AuthResult& result() const { return result_; }
  
  // Mark operation as completed (for audit)
  void complete(bool success = true);
  
  // Non-copyable
  AuthGuard(const AuthGuard&) = delete;
  AuthGuard& operator=(const AuthGuard&) = delete;

private:
  AuthManager& mgr_;
  [[maybe_unused]] Permission permission_;
  std::string operation_;
  AuthResult result_;
  bool completed_{false};
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get human-readable name for role
 */
const char* role_name(Role role);

/**
 * Get human-readable name for permission
 */
const char* permission_name(Permission permission);

/**
 * Get permission required for a UDS service
 */
Permission permission_for_service(SID service);

/**
 * Format audit entry for logging
 */
std::string format_audit_entry(const AuthAuditEntry& entry);

/**
 * Generate unique session ID
 */
std::string generate_session_id();

// ============================================================================
// Default Role Definitions
// ============================================================================

namespace DefaultRoles {

inline RoleDefinition viewer() {
  RoleDefinition def;
  def.role = Role::Viewer;
  def.name = "Viewer";
  def.description = "Read-only access to non-sensitive data";
  def.permissions = Permission::ReadDID | Permission::ReadDTC | Permission::ReadScaling;
  def.required_security_level = 0;
  return def;
}

inline RoleDefinition technician() {
  RoleDefinition def;
  def.role = Role::Technician;
  def.name = "Technician";
  def.description = "Basic diagnostic operations";
  def.permissions = Permission::AllRead | Permission::ClearDTC | 
                    Permission::RoutineControl | Permission::SessionControl;
  def.required_security_level = 0;
  return def;
}

inline RoleDefinition programmer() {
  RoleDefinition def;
  def.role = Role::Programmer;
  def.name = "Programmer";
  def.description = "Flash programming access";
  def.permissions = Permission::AllRead | Permission::AllWrite |
                    Permission::AllControl | Permission::AllProgramming |
                    Permission::SecurityAccess;
  def.required_security_level = security::Level::Programming;
  return def;
}

inline RoleDefinition engineer() {
  RoleDefinition def;
  def.role = Role::Engineer;
  def.name = "Engineer";
  def.description = "Full diagnostic access";
  def.permissions = Permission::All & ~Permission::OEMSpecific;
  def.required_security_level = security::Level::Extended;
  return def;
}

inline RoleDefinition oem() {
  RoleDefinition def;
  def.role = Role::OEM;
  def.name = "OEM";
  def.description = "Manufacturer-level access";
  def.permissions = Permission::All;
  def.required_security_level = security::Level::Development;
  return def;
}

} // namespace DefaultRoles

} // namespace auth
} // namespace uds

#endif // UDS_AUTH_HPP
