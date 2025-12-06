#include "isotp.hpp"
#include <thread>
#include <cstring>

namespace isotp {

// PCI types
static constexpr uint8_t PCI_SF = 0x0 << 4; // Single Frame
static constexpr uint8_t PCI_FF = 0x1 << 4; // First Frame
static constexpr uint8_t PCI_CF = 0x2 << 4; // Consecutive Frame
static constexpr uint8_t PCI_FC = 0x3 << 4; // Flow Control

// Flow Status values
static constexpr uint8_t FC_CTS  = 0x00; // Continue To Send
static constexpr uint8_t FC_WT   = 0x01; // Wait
static constexpr uint8_t FC_OVFL = 0x02; // Overflow/abort

static inline void sleep_ms(uint32_t ms) { if (ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// Calculate STmin delay based on ISO-TP specification
// 0x00-0x7F: 0-127 ms
// 0xF1-0xF9: 100-900 microseconds (we round up to 1ms for simplicity)
// Other values: reserved/invalid, treat as 0
uint32_t Transport::calculate_stmin_delay(uint8_t stmin_value) const {
  if (stmin_value <= 0x7F) {
    return stmin_value; // 0-127 ms
  } else if (stmin_value >= 0xF1 && stmin_value <= 0xF9) {
    // 100-900 microseconds - round up to 1ms for simplicity
    return 1;
  }
  return 0; // Invalid/reserved values
}

bool Transport::request_response(const std::vector<uint8_t>& tx,
                                 std::vector<uint8_t>& rx,
                                 std::chrono::milliseconds timeout) {
  if (!send_sdu(tx, timeout)) return false;
  return recv_sdu(rx, timeout);
}

bool Transport::recv_only(std::vector<uint8_t>& rx, std::chrono::milliseconds timeout) {
  return recv_sdu(rx, timeout);
}

bool Transport::send_sdu(const std::vector<uint8_t>& sdu, [[maybe_unused]] std::chrono::milliseconds timeout) {
  // Check if transmission is allowed
  if (!tx_enabled_) {
    return false; // Tx disabled by CommunicationControl
  }
  
  using CANFrame = CANProtocol::CANFrame;
  const size_t len = sdu.size();

  if (len <= 7) {
    CANFrame f{}; f.id = addr_.tx_can_id; f.dlc = 8;
    f.data[0] = uint8_t(PCI_SF | (len & 0x0F));
    std::memcpy(&f.data[1], sdu.data(), len);
    return drv_.send(f);
  }

  // First Frame
  CANFrame f{}; f.id = addr_.tx_can_id; f.dlc = 8;
  const uint16_t total = static_cast<uint16_t>(len); // limit 4095 for simplicity
  f.data[0] = uint8_t(PCI_FF | ((total >> 8) & 0x0F));
  f.data[1] = uint8_t(total & 0xFF);
  size_t idx = 0;
  const size_t first_copy = 6; // bytes available in FF
  std::memcpy(&f.data[2], &sdu[idx], first_copy); idx += first_copy;
  if (!drv_.send(f)) return false;

  // Wait for FC from receiver with N_Bs timeout and WT handling
  auto fc_deadline = std::chrono::steady_clock::now() + timings_.N_Bs;
  CANFrame fc{};
  uint8_t flow_status = 0;
  if (!wait_for_flow_control(fc, fc_deadline, flow_status)) {
    return false;
  }
  
  if (flow_status == FC_OVFL) return false;
  
  uint8_t bs = fc.data[1]; // 0 = unlimited
  uint8_t stmin_value = fc.data[2];
  uint32_t stmin_delay = calculate_stmin_delay(stmin_value);
  
  // Override with our configured STmin if needed
  if (stmin_ > 0 && stmin_delay < stmin_) {
    stmin_delay = stmin_;
  }

  // Consecutive frames
  uint8_t sn = 1;
  size_t sent_in_block = 0;
  while (idx < len) {
    CANFrame cf{}; cf.id = addr_.tx_can_id; cf.dlc = 8;
    cf.data[0] = uint8_t(PCI_CF | (sn & 0x0F));
    const size_t chunk = std::min(static_cast<size_t>(7), len - idx);
    std::memcpy(&cf.data[1], &sdu[idx], chunk);
    idx += chunk;
    if (!drv_.send(cf)) return false;
    sn = (uint8_t)((sn + 1) & 0x0F);

    ++sent_in_block;
    sleep_ms(stmin_delay);

    if (bs != 0 && sent_in_block >= bs && idx < len) {
      // Expect next FC with N_Bs timeout
      sent_in_block = 0;
      fc_deadline = std::chrono::steady_clock::now() + timings_.N_Bs;
      
      if (!wait_for_flow_control(fc, fc_deadline, flow_status)) {
        return false;
      }
      
      if (flow_status == FC_OVFL) return false;
      
      bs = fc.data[1];
      stmin_value = fc.data[2];
      stmin_delay = calculate_stmin_delay(stmin_value);
      
      if (stmin_ > 0 && stmin_delay < stmin_) {
        stmin_delay = stmin_;
      }
    }
  }

  return true;
}

// Wait for Flow Control frame with FC_WT (Wait) handling
bool Transport::wait_for_flow_control(CANProtocol::CANFrame& fc,
                                     std::chrono::steady_clock::time_point deadline,
                                     uint8_t& flow_status) {
  uint8_t wft_count = 0;
  
  while (true) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return false;
    
    const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    if (!drv_.recv(fc, remain)) return false;
    
    // Filter by CAN ID
    if (fc.id != addr_.rx_can_id) continue;
    
    // Check if this is a Flow Control frame
    if ((fc.data[0] & 0xF0) != PCI_FC) continue;
    
    flow_status = fc.data[0] & 0x0F;
    
    // Handle FC_WT (Wait) - ECU is busy, wait and retry
    if (flow_status == FC_WT) {
      wft_count++;
      if (wft_count > timings_.max_wft) {
        // Exceeded maximum wait frames
        return false;
      }
      // Reset deadline for next FC with N_Bs timeout
      deadline = std::chrono::steady_clock::now() + timings_.N_Bs;
      continue; // Wait for next FC
    }
    
    // Got CTS or OVFL
    return true;
  }
}

bool Transport::recv_sdu(std::vector<uint8_t>& sdu, std::chrono::milliseconds timeout) {
  // Check if reception is allowed
  if (!rx_enabled_) {
    return false; // Rx disabled by CommunicationControl
  }
  
  using CANFrame = CANProtocol::CANFrame;
  auto deadline = std::chrono::steady_clock::now() + timeout;

  CANFrame f{};
  for (;;) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return false;
    const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    if (!drv_.recv(f, remain)) return false;
    if (f.id != addr_.rx_can_id) continue; // filter others
    break;
  }

  const uint8_t pci = f.data[0] & 0xF0;
  if (pci == PCI_SF) {
    const uint8_t len = f.data[0] & 0x0F;
    sdu.assign(&f.data[1], &f.data[1] + len);
    return true;
  }

  if (pci != PCI_FF) return false;

  const uint16_t total = ((f.data[0] & 0x0F) << 8) | f.data[1];
  sdu.clear(); sdu.reserve(total);
  sdu.insert(sdu.end(), &f.data[2], &f.data[8]); // first 6 bytes

  // Send FC CTS
  CANFrame fc{}; fc.id = addr_.tx_can_id; fc.dlc = 8;
  fc.data[0] = uint8_t(PCI_FC | FC_CTS);
  fc.data[1] = block_size_;
  fc.data[2] = stmin_;
  if (!drv_.send(fc)) return false;

  uint8_t expect_sn = 1;
  uint8_t frames_in_block = 0;
  
  while (sdu.size() < total) {
    // Use N_Cr timeout between consecutive frames
    const auto cf_deadline = std::chrono::steady_clock::now() + timings_.N_Cr;
    const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(
      cf_deadline - std::chrono::steady_clock::now());
    
    CANFrame cf{};
    if (!drv_.recv(cf, remain)) return false;
    if (cf.id != addr_.rx_can_id) continue;
    if ((cf.data[0] & 0xF0) != PCI_CF) continue;
    
    const uint8_t sn = cf.data[0] & 0x0F;
    if (sn != expect_sn) return false; // sequence error
    expect_sn = (uint8_t)((expect_sn + 1) & 0x0F);

    const size_t remaining = total - sdu.size();
    const size_t take = remaining < 7 ? remaining : 7;
    sdu.insert(sdu.end(), &cf.data[1], &cf.data[1] + take);
    
    frames_in_block++;
    
    // Send another FC if we've reached block size and there's more data
    if (block_size_ > 0 && frames_in_block >= block_size_ && sdu.size() < total) {
      frames_in_block = 0;
      CANFrame fc{}; fc.id = addr_.tx_can_id; fc.dlc = 8;
      fc.data[0] = uint8_t(PCI_FC | FC_CTS);
      fc.data[1] = block_size_;
      fc.data[2] = stmin_;
      if (!drv_.send(fc)) return false;
    }
  }

  return true;
}

} // namespace isotp
