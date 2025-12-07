#ifndef ISOTP_HPP
#define ISOTP_HPP

/**
 * @file isotp.hpp
 * @brief ISO-TP (ISO 15765-2) Transport Protocol Implementation
 * 
 * ISO 15765-2:2016 - Diagnostic communication over Controller Area Network (DoCAN)
 * 
 * This module implements the ISO-TP transport layer for UDS communication over CAN.
 * ISO-TP provides segmentation and reassembly of messages larger than 8 bytes.
 * 
 * Protocol Overview (ISO 15765-2 Section 7):
 * - Single Frame (SF): Messages ≤ 7 bytes in one CAN frame
 * - First Frame (FF): First segment of multi-frame message
 * - Consecutive Frame (CF): Subsequent segments of multi-frame message
 * - Flow Control (FC): Receiver controls sender flow
 * 
 * Frame Types (ISO 15765-2 Table 11, Section 8.2):
 * - Single Frame (0x0):       [0x0N] [data...] where N = length
 * - First Frame (0x1):         [0x1L LL] [data...] where LLL = length (12 bits)
 * - Consecutive Frame (0x2):   [0x2N] [data...] where N = sequence number
 * - Flow Control (0x3):        [0x30 | 0x31 | 0x32] [BS] [STmin]
 * 
 * Flow Control Types (ISO 15765-2 Section 8.5):
 * - ContinueToSend (0x30): Receiver ready, continue sending
 * - Wait (0x31): Receiver busy, wait for next FC
 * - Overflow (0x32): Receiver buffer full, abort transfer
 * 
 * Timing Parameters (ISO 15765-2 Section 9):
 * - N_As: Timeout for sender to send next CF (1000ms default)
 * - N_Bs: Timeout for receiver to send FC after FF (1000ms default)
 * - N_Cr: Timeout for receiver to receive next CF (1000ms default)
 * - STmin: Minimum separation time between CFs (0-127ms or 100-900µs)
 * - BS (Block Size): Number of CFs before next FC (0 = no limit)
 * 
 * Addressing (ISO 15765-2 Section 10):
 * - Normal (11-bit CAN): Standard CAN IDs (e.g., 0x7E0 request, 0x7E8 response)
 * - Extended (29-bit CAN): Extended CAN IDs
 * - Normal fixed: Target/source addressing in CAN ID
 * - Mixed addressing: First data byte contains address
 */

#include <cstdint>
#include <vector>
#include <chrono>
#include "can_slcan.hpp" // for CANProtocol::CANFrame
#include "uds.hpp"       // for uds::Transport, uds::Address

namespace isotp {

// ISO 15765-2 Timing Parameters (ISO-TP spec)
struct ISOTPTimings {
  // N_As: Sender timeout for transmission of CAN frame (not used in this impl - assumes instant send)
  std::chrono::milliseconds N_As{std::chrono::milliseconds(50)};
  
  // N_Ar: Receiver timeout for reception of CAN frame (network response time)
  std::chrono::milliseconds N_Ar{std::chrono::milliseconds(1000)};
  
  // N_Bs: Sender timeout for receiving Flow Control
  std::chrono::milliseconds N_Bs{std::chrono::milliseconds(1000)};
  
  // N_Br: Receiver timeout before sending Flow Control (not used - we send FC immediately)
  std::chrono::milliseconds N_Br{std::chrono::milliseconds(50)};
  
  // N_Cs: Sender timeout for receiving Consecutive Frame (for receiver role)
  std::chrono::milliseconds N_Cs{std::chrono::milliseconds(1000)};
  
  // N_Cr: Receiver timeout between Consecutive Frames
  std::chrono::milliseconds N_Cr{std::chrono::milliseconds(1000)};
  
  // Maximum number of Wait (WT) Flow Control frames to accept
  uint8_t max_wft{10};
};

// Simplified ISO-TP configuration structure
struct IsoTpConfig {
  uint8_t blockSize = 8;                           // Flow Control block size (0 = unlimited)
  uint8_t stMin = 0;                               // Minimum separation time (ms)
  std::chrono::milliseconds n_ar = std::chrono::milliseconds(100);  // N_Ar timeout
  std::chrono::milliseconds n_bs = std::chrono::milliseconds(100);  // N_Bs timeout
  std::chrono::milliseconds n_cr = std::chrono::milliseconds(100);  // N_Cr timeout
  bool functional = false;                         // Functional addressing
};

// Abstract CAN driver (user must provide an implementation, e.g. SLCAN over serial)
class ICanDriver {
public:
  virtual ~ICanDriver() = default;
  virtual bool send(const CANProtocol::CANFrame& f) = 0;
  virtual bool recv(CANProtocol::CANFrame& f, std::chrono::milliseconds timeout) = 0;
};

// Professional ISO‑TP transport implementing ISO 15765-2 with full compliance
class Transport : public uds::Transport {
public:
  explicit Transport(ICanDriver& drv) : drv_(drv) {}
  
  // Communication control state
  void enable_rx(bool enable) { rx_enabled_ = enable; }
  void enable_tx(bool enable) { tx_enabled_ = enable; }
  bool is_rx_enabled() const { return rx_enabled_; }
  bool is_tx_enabled() const { return tx_enabled_; }

  void set_address(const uds::Address& a) override { addr_ = a; }
  const uds::Address& address() const override { return addr_; }

  bool request_response(const std::vector<uint8_t>& tx,
                        std::vector<uint8_t>& rx,
                        std::chrono::milliseconds timeout) override;

  // Receive-only (for RCR-RP continuation after ResponsePending)
  bool recv_only(std::vector<uint8_t>& rx, std::chrono::milliseconds timeout);
  
  // Receive unsolicited messages (for periodic data, etc.)
  bool recv_unsolicited(std::vector<uint8_t>& rx,
                       std::chrono::milliseconds timeout) override {
    return recv_sdu(rx, timeout);
  }

  // ISO-TP Configuration (legacy detailed API)
  void set_timings(const ISOTPTimings& timings) { timings_ = timings; }
  const ISOTPTimings& timings() const { return timings_; }
  
  void set_block_size(uint8_t bs) { block_size_ = bs; }      // 0 = unlimited
  void set_stmin(uint8_t st) { stmin_ = st; }                // inter‑frame separation (ms for 0x00..0x7F)
  
  // Enable/disable functional addressing support (broadcast)
  void set_functional_addressing(bool enabled) { functional_addressing_ = enabled; }
  
  // Simplified configuration API
  void set_config(const IsoTpConfig& cfg) {
    block_size_ = cfg.blockSize;
    stmin_ = cfg.stMin;
    timings_.N_Ar = cfg.n_ar;
    timings_.N_Bs = cfg.n_bs;
    timings_.N_Cr = cfg.n_cr;
    functional_addressing_ = cfg.functional;
  }
  
  IsoTpConfig config() const {
    IsoTpConfig cfg;
    cfg.blockSize = block_size_;
    cfg.stMin = stmin_;
    cfg.n_ar = timings_.N_Ar;
    cfg.n_bs = timings_.N_Bs;
    cfg.n_cr = timings_.N_Cr;
    cfg.functional = functional_addressing_;
    return cfg;
  }

private:
  bool send_sdu(const std::vector<uint8_t>& sdu, std::chrono::milliseconds timeout);
  bool recv_sdu(std::vector<uint8_t>& sdu, std::chrono::milliseconds timeout);
  
  // Wait for Flow Control with WT (Wait) handling
  bool wait_for_flow_control(CANProtocol::CANFrame& fc, 
                            std::chrono::steady_clock::time_point deadline,
                            uint8_t& flow_status);
  
  // Calculate STmin delay in milliseconds
  uint32_t calculate_stmin_delay(uint8_t stmin_value) const;

  ICanDriver& drv_;
  uds::Address addr_{};
  ISOTPTimings timings_{};
  uint8_t block_size_{0};
  uint8_t stmin_{0};
  bool rx_enabled_{true};
  bool tx_enabled_{true};
  bool functional_addressing_{false};
};

} // namespace isotp

#endif // ISOTP_HPP
