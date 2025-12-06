#include "uds.hpp"
#include "isotp.hpp"  // For dynamic_cast to isotp::Transport
#include "nrc.hpp"    // For NRC action-based handling
#include <thread>

namespace uds {

static inline void sleep_for_min_gap(const Timings& t){ if (t.req_gap.count()>0) std::this_thread::sleep_for(t.req_gap); }

// Core exchange: build [SID | payload], perform transport request/response,
// parse positive or negative response and return structured result.
// Automatically handles NRC 0x78 (ResponsePending) and 0x21 (BusyRepeatRequest).
PositiveOrNegative Client::exchange(SID sid,
                                    const std::vector<uint8_t>& req_payload,
                                    std::chrono::milliseconds timeout) {
  PositiveOrNegative out{};
  std::vector<uint8_t> tx; tx.reserve(1 + req_payload.size());
  tx.push_back(static_cast<uint8_t>(sid));
  tx.insert(tx.end(), req_payload.begin(), req_payload.end());

  if (timeout.count() == 0) timeout = timings_.p2; // default

  sleep_for_min_gap(timings_);
  std::vector<uint8_t> rx;
  if (!t_.request_response(tx, rx, timeout)) {
    return out; // ok=false
  }
  if (rx.empty()) return out;

  // Handle NRCs (0x7F) including 0x78 (ResponsePending) and 0x21 (BusyRepeatRequest)
  for (;;) {
    const uint8_t sid_rx = rx[0];

    if (sid_rx == 0x7F) { // Negative Response
      if (rx.size() >= 3) {
        out.nrc.original_sid = static_cast<SID>(rx[1]);
        out.nrc.code = static_cast<NegativeResponseCode>(rx[2]);

        // 0x78 = RequestCorrectlyReceived_ResponsePending → wait P2* and listen
        if (out.nrc.code == NegativeResponseCode::RequestCorrectlyReceived_ResponsePending) {
          rx.clear();
          auto* tp = dynamic_cast<isotp::Transport*>(&t_);
          if (tp && tp->recv_only(rx, timings_.p2_star)) {
            if (!rx.empty()) continue; // got another frame, re-evaluate
          }
          return out; // ok=false, timeout or empty response
        }

        // 0x21 = BusyRepeatRequest → wait P2 and listen again once
        if (out.nrc.code == NegativeResponseCode::BusyRepeatRequest) {
          rx.clear();
          auto* tp = dynamic_cast<isotp::Transport*>(&t_);
          if (tp && tp->recv_only(rx, timings_.p2)) {
            if (!rx.empty()) continue; // got another frame, re-evaluate
          }
          return out; // ok=false if nothing else shows up
        }
      }
      // Any other NRC → just return as failure
      return out;
    }

    // Not a negative response: must be a positive one
    if (!is_positive_response(sid_rx, static_cast<uint8_t>(sid))) {
      return out; // unexpected frame
    }

    out.ok = true;
    out.payload.assign(rx.begin() + 1, rx.end());
    return out;
  }
}

PositiveOrNegative Client::diagnostic_session_control(Session s) {
  // Send request
  PositiveOrNegative res =
      exchange(SID::DiagnosticSessionControl, { static_cast<uint8_t>(s) });

  if (!res.ok) {
    return res;
  }

  // Typical layout (CAN, no suppressPosRsp):
  // [0] = session echo
  // [1..2] = P2Server_max (ms, big-endian)    (optional)
  // [3..4] = P2*Server_max (ms, big-endian)   (optional)
  //
  // Not all ECUs fill these in, so we only update if present.
  if (res.payload.size() >= 5) {
    // uint8_t session_echo = res.payload[0]; // could sanity check vs s

    uint16_t p2_ms =
        (static_cast<uint16_t>(res.payload[1]) << 8) |
         static_cast<uint16_t>(res.payload[2]);

    uint16_t p2_star_ms =
        (static_cast<uint16_t>(res.payload[3]) << 8) |
         static_cast<uint16_t>(res.payload[4]);

    if (p2_ms == 0)       p2_ms = 50;
    if (p2_star_ms < 500) p2_star_ms = 500;

    timings_.p2      = std::chrono::milliseconds(p2_ms);
    timings_.p2_star = std::chrono::milliseconds(p2_star_ms);
  }

  return res;
}

PositiveOrNegative Client::ecu_reset(EcuResetType type) {
  return exchange(SID::ECUReset, { static_cast<uint8_t>(type) });
}

PositiveOrNegative Client::tester_present(bool suppress_response) {
  uint8_t sub = suppress_response ? 0x80 : 0x00; // bit7=1 suppress
  return exchange(SID::TesterPresent, { sub });
}

PositiveOrNegative Client::security_access_request_seed(uint8_t level) {
  return exchange(SID::SecurityAccess, { static_cast<uint8_t>((level<<1) | 0x01) });
}

PositiveOrNegative Client::security_access_send_key(uint8_t level, const std::vector<uint8_t>& key) {
  std::vector<uint8_t> p{ static_cast<uint8_t>((level<<1) | 0x00) };
  p.insert(p.end(), key.begin(), key.end());
  return exchange(SID::SecurityAccess, p);
}

PositiveOrNegative Client::read_data_by_identifier(DID did) {
  std::vector<uint8_t> p; p.reserve(2);
  codec::be16(p, did);
  return exchange(SID::ReadDataByIdentifier, p);
}

PositiveOrNegative Client::read_scaling_data_by_identifier(DID did) {
  // ReadScalingDataByIdentifier (0x24) - same format as ReadDataByIdentifier
  // Returns scaling information for the specified DID
  std::vector<uint8_t> p; p.reserve(2);
  codec::be16(p, did);
  return exchange(SID::ReadScalingDataByIdentifier, p);
}

PositiveOrNegative Client::write_data_by_identifier(DID did, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> p; p.reserve(2 + data.size());
  codec::be16(p, did);
  p.insert(p.end(), data.begin(), data.end());
  return exchange(SID::WriteDataByIdentifier, p, timings_.p2_star);
}

PositiveOrNegative Client::dynamically_define_data_identifier_by_did(
    DID dynamic_did,
    const std::vector<DDDI_SourceByDID>& sources) {
  // Build: [SubFunction=0x01][DynamicDID][SourceDID][Position][MemSize]...
  std::vector<uint8_t> p;
  p.reserve(3 + sources.size() * 4); // sub + did + (source_did + pos + size)*N
  
  p.push_back(static_cast<uint8_t>(DDDISubFunction::DefineByIdentifier));
  codec::be16(p, dynamic_did);
  
  for (const auto& src : sources) {
    codec::be16(p, src.source_did);
    p.push_back(src.position);
    p.push_back(src.mem_size);
  }
  
  return exchange(SID::DynamicallyDefineDataIdentifier, p);
}

PositiveOrNegative Client::dynamically_define_data_identifier_by_memory(
    DID dynamic_did,
    const std::vector<DDDI_SourceByMemory>& sources) {
  // Build: [SubFunction=0x02][DynamicDID][ALFID][Address...][Size...]...
  std::vector<uint8_t> p;
  p.push_back(static_cast<uint8_t>(DDDISubFunction::DefineByMemoryAddress));
  codec::be16(p, dynamic_did);
  
  for (const auto& src : sources) {
    p.push_back(src.address_and_length_format_id);
    p.insert(p.end(), src.memory_address.begin(), src.memory_address.end());
    p.insert(p.end(), src.memory_size.begin(), src.memory_size.end());
  }
  
  return exchange(SID::DynamicallyDefineDataIdentifier, p);
}

PositiveOrNegative Client::clear_dynamically_defined_data_identifier(DID dynamic_did) {
  // Build: [SubFunction=0x03][DynamicDID]
  std::vector<uint8_t> p;
  p.reserve(3);
  p.push_back(static_cast<uint8_t>(DDDISubFunction::ClearDynamicallyDefinedDataIdentifier));
  codec::be16(p, dynamic_did);
  
  return exchange(SID::DynamicallyDefineDataIdentifier, p);
}

PositiveOrNegative Client::read_memory_by_address(uint32_t address, uint32_t size) {
  // Build: [addressAndLengthFormatIdentifier][address bytes...][size bytes...]
  // Using 4-byte address and 4-byte size (ALFI = 0x44)
  std::vector<uint8_t> p;
  p.reserve(9); // 1 (ALFI) + 4 (address) + 4 (size)
  
  // AddressAndLengthFormatIdentifier: high nibble = address length, low nibble = size length
  p.push_back(0x44); // 4 bytes for address, 4 bytes for size
  
  // Append address (big-endian)
  codec::be32(p, address);
  
  // Append size (big-endian)
  codec::be32(p, size);
  
  return exchange(SID::ReadMemoryByAddress, p, timings_.p2_star);
}

PositiveOrNegative Client::read_memory_by_address(const std::vector<uint8_t>& addr,
                                                  const std::vector<uint8_t>& size) {
  const uint8_t al = static_cast<uint8_t>(addr.size() & 0x0F);
  const uint8_t sl = static_cast<uint8_t>(size.size() & 0x0F);
  const uint8_t alfid = static_cast<uint8_t>((al << 4) | sl);

  std::vector<uint8_t> p;
  p.reserve(1 + addr.size() + size.size());
  p.push_back(alfid);
  p.insert(p.end(), addr.begin(), addr.end());
  p.insert(p.end(), size.begin(), size.end());

  return exchange(SID::ReadMemoryByAddress, p, timings_.p2_star);
}

PositiveOrNegative Client::write_memory_by_address(uint32_t address, const std::vector<uint8_t>& data) {
  // Build: [addressAndLengthFormatIdentifier][address bytes...][size bytes...][data bytes...]
  // Using 4-byte address and 4-byte size (ALFI = 0x44)
  std::vector<uint8_t> p;
  const uint32_t size = static_cast<uint32_t>(data.size());
  p.reserve(9 + data.size()); // 1 (ALFI) + 4 (address) + 4 (size) + data
  
  // AddressAndLengthFormatIdentifier: high nibble = address length, low nibble = size length
  p.push_back(0x44); // 4 bytes for address, 4 bytes for size
  
  // Append address (big-endian)
  codec::be32(p, address);
  
  // Append size (big-endian)
  codec::be32(p, size);
  
  // Append data
  p.insert(p.end(), data.begin(), data.end());
  
  return exchange(SID::WriteMemoryByAddress, p, timings_.p2_star);
}

PositiveOrNegative Client::write_memory_by_address(const std::vector<uint8_t>& addr,
                                                   const std::vector<uint8_t>& size,
                                                   const std::vector<uint8_t>& data) {
  const uint8_t al = static_cast<uint8_t>(addr.size() & 0x0F);
  const uint8_t sl = static_cast<uint8_t>(size.size() & 0x0F);
  const uint8_t alfid = static_cast<uint8_t>((al << 4) | sl);

  std::vector<uint8_t> p;
  p.reserve(1 + addr.size() + size.size() + data.size());
  p.push_back(alfid);
  p.insert(p.end(), addr.begin(), addr.end());
  p.insert(p.end(), size.begin(), size.end());
  p.insert(p.end(), data.begin(), data.end());

  return exchange(SID::WriteMemoryByAddress, p, timings_.p2_star);
}

PositiveOrNegative Client::routine_control(RoutineAction action, RoutineId id, const std::vector<uint8_t>& record) {
  std::vector<uint8_t> p; p.reserve(3 + record.size());
  p.push_back(static_cast<uint8_t>(action));
  codec::be16(p, id);
  p.insert(p.end(), record.begin(), record.end());
  return exchange(SID::RoutineControl, p, timings_.p2_star);
}

PositiveOrNegative Client::clear_diagnostic_information(const std::vector<uint8_t>& group_of_dtc) {
  // groupOfDTC length is typically 3 bytes (mask), but leave generic
  return exchange(SID::ClearDiagnosticInformation, group_of_dtc, timings_.p2_star);
}

PositiveOrNegative Client::read_dtc_information(uint8_t subFunction, const std::vector<uint8_t>& record) {
  std::vector<uint8_t> p{ subFunction };
  p.insert(p.end(), record.begin(), record.end());
  return exchange(SID::ReadDTCInformation, p, timings_.p2_star);
}

// Unused helper - kept for reference
// static std::vector<uint8_t> build_len_prefixed(const std::vector<uint8_t>& raw){
//   // LengthFormatIdentifier uses upper nibble for address length and lower for size length in RequestDownload.
//   // Here we return just [len, raw...] where len = raw.size(). Caller assembles LFI separately.
//   std::vector<uint8_t> v; v.reserve(1 + raw.size()); v.push_back(static_cast<uint8_t>(raw.size())); v.insert(v.end(), raw.begin(), raw.end()); return v; }

PositiveOrNegative Client::request_download(uint8_t dfi,
                                            const std::vector<uint8_t>& addr,
                                            const std::vector<uint8_t>& size) {
  // Build: [DFI][ALFI|SLFI][ALEN|ADDR...][SLEN|SIZE...]
  std::vector<uint8_t> p; p.reserve(2 + addr.size()+1 + size.size()+1);
  p.push_back(dfi);
  const uint8_t al = static_cast<uint8_t>(addr.size());
  const uint8_t sl = static_cast<uint8_t>(size.size());
  p.push_back(static_cast<uint8_t>(((al & 0x0F) << 4) | (sl & 0x0F)));
  p.push_back(al); p.insert(p.end(), addr.begin(), addr.end());
  p.push_back(sl); p.insert(p.end(), size.begin(), size.end());
  return exchange(SID::RequestDownload, p, timings_.p2_star);
}

PositiveOrNegative Client::request_upload(uint8_t dfi,
                                          const std::vector<uint8_t>& addr,
                                          const std::vector<uint8_t>& size) {
  // Build: [DFI][ALFI|SLFI][ALEN|ADDR...][SLEN|SIZE...]
  // Identical format to RequestDownload, different SID
  std::vector<uint8_t> p; p.reserve(2 + addr.size()+1 + size.size()+1);
  p.push_back(dfi);
  const uint8_t al = static_cast<uint8_t>(addr.size());
  const uint8_t sl = static_cast<uint8_t>(size.size());
  p.push_back(static_cast<uint8_t>(((al & 0x0F) << 4) | (sl & 0x0F)));
  p.push_back(al); p.insert(p.end(), addr.begin(), addr.end());
  p.push_back(sl); p.insert(p.end(), size.begin(), size.end());
  return exchange(SID::RequestUpload, p, timings_.p2_star);
}

PositiveOrNegative Client::transfer_data(BlockCounter block, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> p; p.reserve(1 + data.size());
  p.push_back(block);
  p.insert(p.end(), data.begin(), data.end());
  return exchange(SID::TransferData, p, timings_.p2_star);
}

PositiveOrNegative Client::request_transfer_exit(const std::vector<uint8_t>& opt) {
  return exchange(SID::RequestTransferExit, opt, timings_.p2_star);
}

PositiveOrNegative Client::communication_control(uint8_t subFunction, uint8_t communicationType) {
  std::vector<uint8_t> p{ subFunction, communicationType };
  auto result = exchange(SID::CommunicationControl, p);
  
  // Update internal communication state on success
  if (result.ok) {
    // Decode the subfunction to update state
    const uint8_t sf = subFunction & 0x7F; // strip suppress positive response bit
    
    switch (sf) {
      case 0x00: // EnableRxAndTx
      case 0x04: // EnableRxAndTxWithEnhancedAddrInfo
        comm_state_.rx_enabled = true;
        comm_state_.tx_enabled = true;
        break;
      case 0x01: // EnableRxDisableTx
      case 0x05: // EnableRxDisableTxWithEnhancedAddrInfo
        comm_state_.rx_enabled = true;
        comm_state_.tx_enabled = false;
        break;
      case 0x02: // DisableRxEnableTx
      case 0x06: // DisableRxEnableTxWithEnhancedAddrInfo
        comm_state_.rx_enabled = false;
        comm_state_.tx_enabled = true;
        break;
      case 0x03: // DisableRxAndTx
      case 0x07: // DisableRxAndTxWithEnhancedAddrInfo
        comm_state_.rx_enabled = false;
        comm_state_.tx_enabled = false;
        break;
      default:
        // OEM-specific or reserved subfunctions - store as-is
        break;
    }
    
    comm_state_.active_comm_type = communicationType;
  }
  
  return result;
}

PositiveOrNegative Client::control_dtc_setting(uint8_t settingType) {
  std::vector<uint8_t> p{ settingType };
  auto result = exchange(SID::ControlDTCSetting, p);
  
  // Update internal DTC setting state on success
  if (result.ok) {
    const uint8_t st = settingType & 0x7F; // strip suppress positive response bit
    
    switch (st) {
      case 0x01: // On - enable DTC logging
        dtc_setting_enabled_ = true;
        break;
      case 0x02: // Off - disable DTC logging
        dtc_setting_enabled_ = false;
        break;
      default:
        // OEM-specific subfunctions - assume they follow on/off semantics
        // or leave state unchanged if behavior is unknown
        break;
    }
  }
  
  return result;
}

PositiveOrNegative Client::access_timing_parameters(AccessTimingParametersType type,
                                                    const std::vector<uint8_t>& record) {
  std::vector<uint8_t> p;
  p.reserve(1 + record.size());
  p.push_back(static_cast<uint8_t>(type));
  p.insert(p.end(), record.begin(), record.end());
  
  auto result = exchange(SID::AccessTimingParameters, p);
  
  // If reading current timing parameters and successful, parse and update
  if (result.ok && 
      (type == AccessTimingParametersType::ReadCurrentlyActiveTimingParameters ||
       type == AccessTimingParametersType::ReadExtendedTimingParameterSet)) {
    // Response format: [SubFunction][P2_Server_Max(2)][P2*_Server_Max(2)][...]
    if (result.payload.size() >= 5) {
      // Bytes 1-2: P2 in 1ms units (big-endian)
      uint16_t p2_ms = (static_cast<uint16_t>(result.payload[1]) << 8) |
                        static_cast<uint16_t>(result.payload[2]);
      
      // Bytes 3-4: P2* in 10ms units (big-endian)
      uint16_t p2_star_10ms = (static_cast<uint16_t>(result.payload[3]) << 8) |
                               static_cast<uint16_t>(result.payload[4]);
      
      // Update client timings
      timings_.p2 = std::chrono::milliseconds(p2_ms);
      timings_.p2_star = std::chrono::milliseconds(p2_star_10ms * 10);
    }
  }
  
  return result;
}

PositiveOrNegative Client::read_data_by_periodic_identifier(
    PeriodicTransmissionMode mode,
    const std::vector<PeriodicDID>& identifiers) {
  // Build: [TransmissionMode][PeriodicDID1][PeriodicDID2]...
  std::vector<uint8_t> p;
  p.reserve(1 + identifiers.size());
  
  p.push_back(static_cast<uint8_t>(mode));
  
  // Append all periodic identifiers
  for (PeriodicDID id : identifiers) {
    p.push_back(id);
  }
  
  return exchange(SID::ReadDataByPeriodicIdentifier, p);
}

bool Client::receive_periodic_data(PeriodicDataMessage& msg,
                                   std::chrono::milliseconds timeout) {
  std::vector<uint8_t> rx;
  
  // Try to receive unsolicited message from transport
  if (!t_.recv_unsolicited(rx, timeout)) {
    return false;
  }
  
  if (rx.empty()) {
    return false;
  }
  
  // Periodic data response format: [0x6A][PeriodicDID][data...]
  // where 0x6A = 0x2A + 0x40 (positive response)
  const uint8_t sid = rx[0];
  if (sid != 0x6A) { // Not a periodic data message
    return false;
  }
  
  if (rx.size() < 2) { // Need at least SID + PeriodicDID
    return false;
  }
  
  msg.identifier = rx[1];
  
  // Extract data (everything after the periodic DID)
  if (rx.size() > 2) {
    msg.data.assign(rx.begin() + 2, rx.end());
  } else {
    msg.data.clear();
  }
  
  return true;
}

} // namespace uds
