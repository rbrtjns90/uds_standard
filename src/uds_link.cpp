#include "uds_link.hpp"

namespace uds {
namespace link {

// ============================================================================
// Baudrate Encoding/Decoding
// ============================================================================

std::vector<uint8_t> encode_baudrate(uint32_t baudrate_bps) {
  // Encode baudrate as 3 bytes (big-endian): high, mid, low
  return {
    static_cast<uint8_t>((baudrate_bps >> 16) & 0xFF),
    static_cast<uint8_t>((baudrate_bps >> 8) & 0xFF),
    static_cast<uint8_t>(baudrate_bps & 0xFF)
  };
}

uint32_t decode_baudrate(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < 3) {
    return 0;
  }
  return (static_cast<uint32_t>(bytes[0]) << 16) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         static_cast<uint32_t>(bytes[2]);
}

// ============================================================================
// Helper: Map fixed baudrate ID to bps
// ============================================================================

static uint32_t fixed_baudrate_to_bps(uint8_t id) {
  switch (static_cast<FixedBaudrate>(id)) {
    case FixedBaudrate::CAN_125kbps:  return 125000;
    case FixedBaudrate::CAN_250kbps:  return 250000;
    case FixedBaudrate::CAN_500kbps:  return 500000;
    case FixedBaudrate::CAN_1Mbps:    return 1000000;
    case FixedBaudrate::Rate_9600:    return 9600;
    case FixedBaudrate::Rate_19200:   return 19200;
    case FixedBaudrate::Rate_38400:   return 38400;
    case FixedBaudrate::Rate_57600:   return 57600;
    case FixedBaudrate::Rate_115200:  return 115200;
    default:                          return 0;
  }
}

// ============================================================================
// Core API Implementation
// ============================================================================

Result<LinkResponse> verify_fixed_baudrate(Client& client, uint8_t baudrate_id) {
  // Build request: [SubFunction=0x01][linkBaudrateRecord=baudrate_id]
  std::vector<uint8_t> payload;
  payload.push_back(static_cast<uint8_t>(LinkControlType::VerifyBaudrateTransitionWithFixedBaudrate));
  payload.push_back(baudrate_id);
  
  auto result = client.exchange(SID::LinkControl, payload);
  
  if (!result.ok) {
    return Result<LinkResponse>::error(result.nrc);
  }
  
  LinkResponse response;
  if (!result.payload.empty()) {
    response.control_type = static_cast<LinkControlType>(result.payload[0] & 0x7F);
    if (result.payload.size() > 1) {
      response.link_baudrate_record.assign(result.payload.begin() + 1, result.payload.end());
    }
  }
  
  return Result<LinkResponse>::success(response);
}

Result<LinkResponse> verify_fixed_baudrate(Client& client, FixedBaudrate baudrate) {
  return verify_fixed_baudrate(client, static_cast<uint8_t>(baudrate));
}

Result<LinkResponse> verify_specific_baudrate(Client& client, uint32_t baudrate_bps) {
  // Build request: [SubFunction=0x02][linkBaudrateRecord=3-byte baudrate]
  std::vector<uint8_t> payload;
  payload.push_back(static_cast<uint8_t>(LinkControlType::VerifyBaudrateTransitionWithSpecificBaudrate));
  
  auto baudrate_bytes = encode_baudrate(baudrate_bps);
  payload.insert(payload.end(), baudrate_bytes.begin(), baudrate_bytes.end());
  
  auto result = client.exchange(SID::LinkControl, payload);
  
  if (!result.ok) {
    return Result<LinkResponse>::error(result.nrc);
  }
  
  LinkResponse response;
  if (!result.payload.empty()) {
    response.control_type = static_cast<LinkControlType>(result.payload[0] & 0x7F);
    if (result.payload.size() > 1) {
      response.link_baudrate_record.assign(result.payload.begin() + 1, result.payload.end());
    }
  }
  
  return Result<LinkResponse>::success(response);
}

Result<void> transition_baudrate(Client& client) {
  // Build request: [SubFunction=0x03 | 0x80 (suppressPosRsp)]
  // The ECU will switch baudrate immediately, so we suppress the positive response
  // since it would arrive at the new baudrate before we've switched.
  std::vector<uint8_t> payload;
  payload.push_back(static_cast<uint8_t>(LinkControlType::TransitionBaudrate) | 0x80);
  
  // Send the request - we don't expect a response
  auto result = client.exchange(SID::LinkControl, payload);
  
  // For transition with suppressPosRsp, a timeout is expected and acceptable
  // The ECU switches baudrate immediately after receiving the request
  // We consider the request successful if it was sent (even if no response)
  
  // Note: In practice, you'd want to verify the transition worked by
  // sending a TesterPresent at the new baudrate
  
  return Result<void>::success();
}

Result<LinkResponse> link_control(Client& client, const LinkRequest& request) {
  switch (request.control_type) {
    case LinkControlType::VerifyBaudrateTransitionWithFixedBaudrate:
      if (request.baudrate_id.has_value()) {
        return verify_fixed_baudrate(client, request.baudrate_id.value());
      }
      return Result<LinkResponse>::error();
      
    case LinkControlType::VerifyBaudrateTransitionWithSpecificBaudrate:
      if (request.specific_baudrate_bps.has_value()) {
        return verify_specific_baudrate(client, request.specific_baudrate_bps.value());
      }
      return Result<LinkResponse>::error();
      
    case LinkControlType::TransitionBaudrate: {
      auto result = transition_baudrate(client);
      if (result.ok) {
        LinkResponse response;
        response.control_type = LinkControlType::TransitionBaudrate;
        return Result<LinkResponse>::success(response);
      }
      return Result<LinkResponse>::error(result.nrc);
    }
    
    default:
      return Result<LinkResponse>::error();
  }
}

// ============================================================================
// Convenience Functions
// ============================================================================

Result<void> prepare_programming_baudrate(Client& client, uint32_t target_baudrate_bps) {
  // If no specific baudrate requested, try common programming baudrates
  if (target_baudrate_bps == 0) {
    // Try 500kbps first (common programming baudrate)
    auto result = verify_fixed_baudrate(client, FixedBaudrate::CAN_500kbps);
    if (result.ok) {
      return Result<void>::success();
    }
    
    // Try 1Mbps
    result = verify_fixed_baudrate(client, FixedBaudrate::CAN_1Mbps);
    if (result.ok) {
      return Result<void>::success();
    }
    
    // Fall back to 250kbps
    result = verify_fixed_baudrate(client, FixedBaudrate::CAN_250kbps);
    if (result.ok) {
      return Result<void>::success();
    }
    
    return Result<void>::error();
  }
  
  // Try specific baudrate
  auto result = verify_specific_baudrate(client, target_baudrate_bps);
  if (result.ok) {
    return Result<void>::success();
  }
  
  return Result<void>::error(result.nrc);
}

const char* link_control_type_name(LinkControlType type) {
  switch (type) {
    case LinkControlType::VerifyBaudrateTransitionWithFixedBaudrate:
      return "VerifyBaudrateTransitionWithFixedBaudrate";
    case LinkControlType::VerifyBaudrateTransitionWithSpecificBaudrate:
      return "VerifyBaudrateTransitionWithSpecificBaudrate";
    case LinkControlType::TransitionBaudrate:
      return "TransitionBaudrate";
    default:
      return "Unknown";
  }
}

const char* fixed_baudrate_name(FixedBaudrate baudrate) {
  switch (baudrate) {
    case FixedBaudrate::CAN_125kbps:      return "CAN 125 kbps";
    case FixedBaudrate::CAN_250kbps:      return "CAN 250 kbps";
    case FixedBaudrate::CAN_500kbps:      return "CAN 500 kbps";
    case FixedBaudrate::CAN_1Mbps:        return "CAN 1 Mbps";
    case FixedBaudrate::Rate_9600:        return "9600 bps";
    case FixedBaudrate::Rate_19200:       return "19200 bps";
    case FixedBaudrate::Rate_38400:       return "38400 bps";
    case FixedBaudrate::Rate_57600:       return "57600 bps";
    case FixedBaudrate::Rate_115200:      return "115200 bps";
    case FixedBaudrate::Programming_High: return "Programming High";
    case FixedBaudrate::Programming_Max:  return "Programming Max";
    default:                              return "Unknown";
  }
}

// ============================================================================
// RAII Guard Implementation
// ============================================================================

TemporaryBaudrateGuard::TemporaryBaudrateGuard(Client& client,
                                               FixedBaudrate target_baudrate,
                                               FixedBaudrate original_baudrate)
    : client_(client),
      target_baudrate_bps_(fixed_baudrate_to_bps(static_cast<uint8_t>(target_baudrate))),
      original_baudrate_bps_(fixed_baudrate_to_bps(static_cast<uint8_t>(original_baudrate))),
      use_fixed_baudrate_(true),
      original_fixed_id_(static_cast<uint8_t>(original_baudrate)) {
  
  // Verify the target baudrate
  auto verify_result = verify_fixed_baudrate(client_, target_baudrate);
  if (!verify_result.ok) {
    return; // active_ remains false
  }
  
  // Transition to the new baudrate
  auto transition_result = transition_baudrate(client_);
  if (!transition_result.ok) {
    return; // active_ remains false
  }
  
  active_ = true;
}

TemporaryBaudrateGuard::TemporaryBaudrateGuard(Client& client,
                                               uint32_t target_baudrate_bps,
                                               uint32_t original_baudrate_bps)
    : client_(client),
      target_baudrate_bps_(target_baudrate_bps),
      original_baudrate_bps_(original_baudrate_bps),
      use_fixed_baudrate_(false) {
  
  // Verify the target baudrate
  auto verify_result = verify_specific_baudrate(client_, target_baudrate_bps);
  if (!verify_result.ok) {
    return; // active_ remains false
  }
  
  // Transition to the new baudrate
  auto transition_result = transition_baudrate(client_);
  if (!transition_result.ok) {
    return; // active_ remains false
  }
  
  active_ = true;
}

TemporaryBaudrateGuard::~TemporaryBaudrateGuard() {
  if (!active_ || original_baudrate_bps_ == 0) {
    return;
  }
  
  // Note: At this point, the local CAN interface should already be
  // reconfigured to the original baudrate by the caller.
  // We just need to tell the ECU to switch back.
  
  if (use_fixed_baudrate_) {
    auto verify_result = verify_fixed_baudrate(client_, original_fixed_id_);
    if (verify_result.ok) {
      transition_baudrate(client_);
    }
  } else {
    auto verify_result = verify_specific_baudrate(client_, original_baudrate_bps_);
    if (verify_result.ok) {
      transition_baudrate(client_);
    }
  }
}

} // namespace link
} // namespace uds
