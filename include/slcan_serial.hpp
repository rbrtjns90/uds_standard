#ifndef SLCAN_SERIAL_HPP
#define SLCAN_SERIAL_HPP

#include "isotp.hpp"
#include "can_slcan.hpp"
#include <termios.h>
#include <string>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>

namespace slcan {

// Frame event types for callback mechanism
enum class FrameEvent {
  Received,        // Normal data frame received
  Transmitted,     // Frame transmitted successfully
  Error,           // Error frame detected
  FlowControl,     // Flow Control frame (ISO-TP)
  Timeout,         // Reception timeout
  QueueFull        // TX queue full (back-pressure)
};

// Flow Control frame classification
enum class FlowControlType {
  Unknown,         // Not a flow control frame
  CTS,             // Continue To Send (0x30)
  WT,              // Wait (0x31)
  OVFL             // Overflow/Abort (0x32)
};

// Enhanced CAN frame with metadata
struct CanFrame : public CANProtocol::CANFrame {
  FlowControlType fc_type = FlowControlType::Unknown;
  std::chrono::steady_clock::time_point timestamp;
  
  // Classify if this is a Flow Control frame
  void classify_flow_control() {
    if (dlc >= 3 && (data[0] & 0xF0) == 0x30) {
      uint8_t fs = data[0] & 0x0F;
      if (fs == 0x00) fc_type = FlowControlType::CTS;
      else if (fs == 0x01) fc_type = FlowControlType::WT;
      else if (fs == 0x02) fc_type = FlowControlType::OVFL;
    }
  }
};

/// SLCAN serial driver implementing isotp::ICanDriver
/// Manages serial port, SLCAN protocol initialization, and frame TX/RX
class SerialDriver : public isotp::ICanDriver {
public:
  SerialDriver() = default;
  ~SerialDriver() override;

  // Non-copyable
  SerialDriver(const SerialDriver&) = delete;
  SerialDriver& operator=(const SerialDriver&) = delete;

  /// Open serial port and initialize SLCAN
  /// @param device Path like "/dev/cu.usbserial-XXX"
  /// @param bitrate CAN bitrate (e.g. 500000, 250000)
  /// @param filter_id Optional ID filter (0 = accept all)
  /// @param filter_mask Optional ID mask (0 = accept all)
  bool open(const std::string& device, uint32_t bitrate,
            uint32_t filter_id = 0, uint32_t filter_mask = 0);

  /// Close serial port and channel
  void close();

  /// Check if open
  bool is_open() const { return fd_ >= 0; }

  // ICanDriver interface
  bool send(const CANProtocol::CANFrame& f) override;
  bool recv(CANProtocol::CANFrame& f, std::chrono::milliseconds timeout) override;
  
  // Enhanced frame operations
  bool send_can_frame(const CanFrame& frame);
  bool receive_frame(CanFrame& out);
  
  // Configuration
  void enable_timestamps(bool on) { timestamps_enabled_ = on; }
  bool timestamps_enabled() const { return timestamps_enabled_; }
  
  void set_tx_queue_max_size(size_t max_size) { tx_queue_max_size_ = max_size; }
  size_t tx_queue_size() const { return tx_queue_.size(); }
  size_t tx_queue_max_size() const { return tx_queue_max_size_; }
  
  // Event callback mechanism
  void set_rx_callback(std::function<void(const CanFrame&)> cb) { rx_callback_ = cb; }
  void set_event_callback(std::function<void(FrameEvent, const CanFrame&)> cb) { event_callback_ = cb; }
  
  // Statistics
  struct Statistics {
    uint64_t frames_sent = 0;
    uint64_t frames_received = 0;
    uint64_t error_frames = 0;
    uint64_t fc_cts_count = 0;
    uint64_t fc_wt_count = 0;
    uint64_t fc_ovfl_count = 0;
    uint64_t tx_queue_overflows = 0;
    uint64_t parse_errors = 0;
  };
  
  const Statistics& stats() const { return stats_; }
  void reset_stats() { stats_ = Statistics{}; }

private:
  // Serial port operations
  bool open_serial(const std::string& device);
  void close_serial();
  bool write_command(const std::string& cmd, std::chrono::milliseconds timeout);
  bool read_until_cr(std::string& line, std::chrono::milliseconds timeout);
  ssize_t read_raw(uint8_t* buf, size_t maxlen, std::chrono::milliseconds timeout);

  // SLCAN initialization
  bool init_slcan(uint32_t bitrate, uint32_t filter_id, uint32_t filter_mask);

  // Frame parsing and buffering
  bool parse_slcan_frame(const std::string& line, CANProtocol::CANFrame& f);
  bool read_and_buffer_frames(std::chrono::milliseconds timeout);

  int fd_{-1};
  struct termios orig_termios_{};
  bool termios_saved_{false};
  
  // RX queue and mutex
  std::deque<CANProtocol::CANFrame> rx_queue_;
  std::mutex rx_mutex_;
  
  // TX queue with back-pressure
  std::deque<CanFrame> tx_queue_;
  std::mutex tx_mutex_;
  size_t tx_queue_max_size_{100}; // Default max queue size
  
  // Configuration
  bool timestamps_enabled_{true};
  
  // Callbacks
  std::function<void(const CanFrame&)> rx_callback_;
  std::function<void(FrameEvent, const CanFrame&)> event_callback_;
  
  // Statistics
  Statistics stats_{};
  
  // Helper methods
  void invoke_rx_callback(const CanFrame& frame);
  void invoke_event_callback(FrameEvent event, const CanFrame& frame);
  void update_stats(const CanFrame& frame, FrameEvent event);
};

} // namespace slcan

#endif // SLCAN_SERIAL_HPP
