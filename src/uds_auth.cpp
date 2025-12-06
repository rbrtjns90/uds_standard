#include "uds_auth.hpp"
#include <sstream>
#include <iomanip>
#include <random>
#include <ctime>

namespace uds {
namespace auth {

// ============================================================================
// RoleBasedPolicy Implementation
// ============================================================================

AuthResult RoleBasedPolicy::authorize(const SessionInfo& session,
                                      Permission permission,
                                      const std::map<std::string, std::string>& context) {
  (void)context;  // May be used for fine-grained control
  
  auto it = role_definitions_.find(session.user.role);
  if (it == role_definitions_.end()) {
    return AuthResult::deny("Role not defined");
  }
  
  const auto& role_def = it->second;
  
  // Check permission
  if (!role_def.has(permission)) {
    return AuthResult::deny("Permission denied for role " + role_def.name, permission);
  }
  
  // Check security level
  if (role_def.required_security_level > 0) {
    if (!session.security_unlocked || 
        session.security_level < role_def.required_security_level) {
      return AuthResult::require_security(role_def.required_security_level);
    }
  }
  
  return AuthResult::allow();
}

void RoleBasedPolicy::set_role_definition(Role role, const RoleDefinition& def) {
  role_definitions_[role] = def;
}

const RoleDefinition* RoleBasedPolicy::get_role_definition(Role role) const {
  auto it = role_definitions_.find(role);
  return it != role_definitions_.end() ? &it->second : nullptr;
}

// ============================================================================
// AuthManager Implementation
// ============================================================================

AuthManager::AuthManager() {
  // Setup default role-based policy
  auto policy = std::make_unique<RoleBasedPolicy>();
  setup_default_roles();
  
  // Copy role definitions to policy
  for (const auto& [role, def] : role_definitions_) {
    policy->set_role_definition(role, def);
  }
  
  policy_ = std::move(policy);
}

void AuthManager::setup_default_roles() {
  role_definitions_[Role::None] = RoleDefinition{
    Role::None, "None", "No access", Permission{}, 0
  };
  role_definitions_[Role::Viewer] = DefaultRoles::viewer();
  role_definitions_[Role::Technician] = DefaultRoles::technician();
  role_definitions_[Role::Programmer] = DefaultRoles::programmer();
  role_definitions_[Role::Engineer] = DefaultRoles::engineer();
  role_definitions_[Role::OEM] = DefaultRoles::oem();
}

std::string AuthManager::start_session(const UserInfo& user) {
  session_.session_id = generate_session_id();
  session_.user = user;
  session_.start_time = std::chrono::system_clock::now();
  session_.last_activity = session_.start_time;
  session_.is_active = true;
  session_.security_level = 0;
  session_.security_unlocked = false;
  
  log_audit(Permission{}, true, "Session started", "LOGIN", user.user_id);
  
  return session_.session_id;
}

void AuthManager::end_session() {
  if (session_.is_active) {
    log_audit(Permission{}, true, "Session ended", "LOGOUT", session_.user.user_id);
  }
  session_ = SessionInfo{};
}

void AuthManager::touch_session() {
  session_.last_activity = std::chrono::system_clock::now();
}

void AuthManager::set_current_role(Role role) {
  session_.user.role = role;
  session_.is_active = true;
  
  if (session_.user.user_id.empty()) {
    session_.user.user_id = "default";
  }
  if (session_.session_id.empty()) {
    session_.session_id = generate_session_id();
    session_.start_time = std::chrono::system_clock::now();
  }
  
  touch_session();
}

void AuthManager::define_role(const RoleDefinition& definition) {
  role_definitions_[definition.role] = definition;
  
  // Update policy if it's role-based
  auto* rbp = dynamic_cast<RoleBasedPolicy*>(policy_.get());
  if (rbp) {
    rbp->set_role_definition(definition.role, definition);
  }
}

const RoleDefinition* AuthManager::get_role(Role role) const {
  auto it = role_definitions_.find(role);
  return it != role_definitions_.end() ? &it->second : nullptr;
}

bool AuthManager::can_perform(Permission permission) const {
  return check_authorization(permission).authorized;
}

AuthResult AuthManager::check_authorization(Permission permission,
                                            const std::map<std::string, std::string>& context) const {
  if (!session_.is_active) {
    return AuthResult::deny("No active session");
  }
  
  if (!policy_) {
    return AuthResult::deny("No authorization policy configured");
  }
  
  return policy_->authorize(session_, permission, context);
}

AuthResult AuthManager::require(Permission permission,
                                const std::map<std::string, std::string>& context) {
  auto result = check_authorization(permission, context);
  
  log_audit(permission, result.authorized, result.reason);
  
  return result;
}

void AuthManager::set_security_level(uint8_t level, bool unlocked) {
  session_.security_level = level;
  session_.security_unlocked = unlocked;
}

bool AuthManager::meets_security_level(uint8_t required) const {
  if (required == 0) return true;
  return session_.security_unlocked && session_.security_level >= required;
}

void AuthManager::link_security_manager(security::SecurityManager* mgr) {
  security_mgr_ = mgr;
}

void AuthManager::set_policy(std::unique_ptr<IAuthPolicy> policy) {
  policy_ = std::move(policy);
}

void AuthManager::log_audit(Permission permission, bool authorized,
                            const std::string& reason,
                            const std::string& operation,
                            const std::string& target) {
  if (!audit_enabled_) return;
  
  AuthAuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.user_id = session_.user.user_id;
  entry.role = session_.user.role;
  entry.requested_permission = permission;
  entry.authorized = authorized;
  entry.reason = reason;
  entry.operation = operation;
  entry.target = target;
  
  audit_log_.push_back(entry);
  
  // Trim if over limit
  if (max_audit_entries_ > 0 && audit_log_.size() > max_audit_entries_) {
    audit_log_.erase(audit_log_.begin());
  }
  
  // Call callback if set
  if (audit_callback_) {
    audit_callback_(entry);
  }
}

// ============================================================================
// AuthGuard Implementation
// ============================================================================

AuthGuard::AuthGuard(AuthManager& mgr, Permission permission,
                     const std::string& operation,
                     const std::map<std::string, std::string>& context)
    : mgr_(mgr), permission_(permission), operation_(operation) {
  
  result_ = mgr_.require(permission, context);
}

AuthGuard::~AuthGuard() {
  if (!completed_ && result_.authorized) {
    // Operation was authorized but not explicitly completed
    // Log as incomplete/abandoned
  }
}

void AuthGuard::complete(bool success) {
  completed_ = true;
  // Could log completion status here
  (void)success;
}

// ============================================================================
// Helper Functions
// ============================================================================

const char* role_name(Role role) {
  switch (role) {
    case Role::None:        return "None";
    case Role::Viewer:      return "Viewer";
    case Role::Technician:  return "Technician";
    case Role::Programmer:  return "Programmer";
    case Role::Calibration: return "Calibration";
    case Role::Engineer:    return "Engineer";
    case Role::OEM:         return "OEM";
    case Role::Development: return "Development";
    case Role::Custom1:     return "Custom1";
    case Role::Custom2:     return "Custom2";
    case Role::Custom3:     return "Custom3";
    default:                return "Unknown";
  }
}

const char* permission_name(Permission permission) {
  switch (permission) {
    case Permission::ReadDID:              return "ReadDID";
    case Permission::ReadDTC:              return "ReadDTC";
    case Permission::ReadMemory:           return "ReadMemory";
    case Permission::ReadScaling:          return "ReadScaling";
    case Permission::ReadPeriodicData:     return "ReadPeriodicData";
    case Permission::WriteDID:             return "WriteDID";
    case Permission::WriteMemory:          return "WriteMemory";
    case Permission::ClearDTC:             return "ClearDTC";
    case Permission::RoutineControl:       return "RoutineControl";
    case Permission::IOControl:            return "IOControl";
    case Permission::CommunicationControl: return "CommunicationControl";
    case Permission::DTCSettingControl:    return "DTCSettingControl";
    case Permission::SessionControl:       return "SessionControl";
    case Permission::SecurityAccess:       return "SecurityAccess";
    case Permission::LinkControl:          return "LinkControl";
    case Permission::RequestDownload:      return "RequestDownload";
    case Permission::RequestUpload:        return "RequestUpload";
    case Permission::TransferData:         return "TransferData";
    case Permission::ECUReset:             return "ECUReset";
    case Permission::Configuration:        return "Configuration";
    case Permission::Development:          return "Development";
    case Permission::OEMSpecific:          return "OEMSpecific";
    default:                               return "Multiple/Unknown";
  }
}

Permission permission_for_service(SID service) {
  switch (service) {
    case SID::DiagnosticSessionControl:
      return Permission::SessionControl;
    case SID::ECUReset:
      return Permission::ECUReset;
    case SID::ClearDiagnosticInformation:
      return Permission::ClearDTC;
    case SID::ReadDTCInformation:
      return Permission::ReadDTC;
    case SID::ReadDataByIdentifier:
      return Permission::ReadDID;
    case SID::ReadMemoryByAddress:
      return Permission::ReadMemory;
    case SID::ReadScalingDataByIdentifier:
      return Permission::ReadScaling;
    case SID::SecurityAccess:
      return Permission::SecurityAccess;
    case SID::CommunicationControl:
      return Permission::CommunicationControl;
    case SID::ReadDataByPeriodicIdentifier:
      return Permission::ReadPeriodicData;
    case SID::WriteDataByIdentifier:
      return Permission::WriteDID;
    case SID::WriteMemoryByAddress:
      return Permission::WriteMemory;
    case SID::RoutineControl:
      return Permission::RoutineControl;
    case SID::RequestDownload:
      return Permission::RequestDownload;
    case SID::RequestUpload:
      return Permission::RequestUpload;
    case SID::TransferData:
      return Permission::TransferData;
    case SID::ControlDTCSetting:
      return Permission::DTCSettingControl;
    case SID::LinkControl:
      return Permission::LinkControl;
    default:
      return Permission{};
  }
}

std::string format_audit_entry(const AuthAuditEntry& entry) {
  std::ostringstream oss;
  
  // Format timestamp
  auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
  oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  
  oss << " [" << entry.user_id << "/" << role_name(entry.role) << "] ";
  
  if (static_cast<uint32_t>(entry.requested_permission) != 0) {
    oss << permission_name(entry.requested_permission) << " ";
  }
  
  if (!entry.operation.empty()) {
    oss << entry.operation << " ";
  }
  
  if (!entry.target.empty()) {
    oss << "-> " << entry.target << " ";
  }
  
  oss << (entry.authorized ? "ALLOWED" : "DENIED");
  
  if (!entry.reason.empty()) {
    oss << " (" << entry.reason << ")";
  }
  
  return oss.str();
}

std::string generate_session_id() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<uint32_t> dis;
  
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  oss << std::setw(8) << dis(gen);
  oss << "-";
  oss << std::setw(4) << (dis(gen) & 0xFFFF);
  oss << "-";
  oss << std::setw(4) << (dis(gen) & 0xFFFF);
  oss << "-";
  oss << std::setw(8) << dis(gen);
  
  return oss.str();
}

} // namespace auth
} // namespace uds
