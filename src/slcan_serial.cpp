#include "slcan_serial.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace slcan {

SerialDriver::~SerialDriver() {
  close();
}

void SerialDriver::close() {
  if (fd_ >= 0) {
    // Try to close SLCAN channel gracefully
    write_command("C\r", std::chrono::milliseconds(100));
    close_serial();
  }
}

bool SerialDriver::open(const std::string& device, uint32_t bitrate,
                        uint32_t filter_id, uint32_t filter_mask) {
  if (!open_serial(device)) return false;
  if (!init_slcan(bitrate, filter_id, filter_mask)) {
    close_serial();
    return false;
  }
  return true;
}

bool SerialDriver::open_serial(const std::string& device) {
  fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    std::cerr << "Failed to open " << device << ": " << strerror(errno) << "\n";
    return false;
  }

  // Save original termios
  if (tcgetattr(fd_, &orig_termios_) < 0) {
    std::cerr << "tcgetattr failed: " << strerror(errno) << "\n";
    ::close(fd_); fd_ = -1;
    return false;
  }
  termios_saved_ = true;

  // Configure raw mode: 8N1, no parity, no flow control
  struct termios tio = orig_termios_;
  cfmakeraw(&tio);
  tio.c_cflag |= (CLOCAL | CREAD);
  tio.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
  tio.c_cflag |= CS8;
  tio.c_iflag &= ~(IXON | IXOFF | IXANY);
  tio.c_oflag &= ~OPOST;
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 0;

  // Most SLCAN adapters use 115200 baud by default
  cfsetispeed(&tio, B115200);
  cfsetospeed(&tio, B115200);

  if (tcsetattr(fd_, TCSANOW, &tio) < 0) {
    std::cerr << "tcsetattr failed: " << strerror(errno) << "\n";
    close_serial();
    return false;
  }

  tcflush(fd_, TCIOFLUSH);
  return true;
}

void SerialDriver::close_serial() {
  if (fd_ >= 0) {
    if (termios_saved_) {
      tcsetattr(fd_, TCSANOW, &orig_termios_);
    }
    ::close(fd_);
    fd_ = -1;
    termios_saved_ = false;
  }
}

ssize_t SerialDriver::read_raw(uint8_t* buf, size_t maxlen, std::chrono::milliseconds timeout) {
  if (fd_ < 0) return -1;

  fd_set rfds;
  struct timeval tv;
  tv.tv_sec = timeout.count() / 1000;
  tv.tv_usec = (timeout.count() % 1000) * 1000;

  FD_ZERO(&rfds);
  FD_SET(fd_, &rfds);

  int ret = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
  if (ret <= 0) return ret; // timeout or error

  return ::read(fd_, buf, maxlen);
}

bool SerialDriver::read_until_cr(std::string& line, std::chrono::milliseconds timeout) {
  line.clear();
  auto deadline = std::chrono::steady_clock::now() + timeout;
  uint8_t ch;

  for (;;) {
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return false;
    auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);

    ssize_t n = read_raw(&ch, 1, remain);
    if (n <= 0) return false;

    if (ch == '\r' || ch == '\n') {
      if (!line.empty()) return true;
      continue; // skip leading CRs/LFs
    }
    if (ch == 0x07) { // SLCAN error bell
      return false;
    }
    line.push_back(static_cast<char>(ch));
    if (line.size() > 128) return false; // sanity limit
  }
}

bool SerialDriver::write_command(const std::string& cmd, std::chrono::milliseconds timeout) {
  if (fd_ < 0) return false;
  ssize_t n = ::write(fd_, cmd.data(), cmd.size());
  if (n != static_cast<ssize_t>(cmd.size())) return false;

  // Read until CR (or error bell)
  std::string resp;
  return read_until_cr(resp, timeout);
}

bool SerialDriver::init_slcan(uint32_t bitrate, uint32_t filter_id, uint32_t filter_mask) {
  // 1) Close channel (in case it was open)
  write_command("C\r", std::chrono::milliseconds(100));

  // 2) Set bitrate
  std::string bitrate_cmd = CANProtocol::SLCAN::CommandBuilder::setupBitrate(bitrate);
  if (!write_command(bitrate_cmd + "\r", std::chrono::milliseconds(500))) {
    std::cerr << "Failed to set bitrate\n";
    return false;
  }

  // 3) Set acceptance filter if specified
  if (filter_mask != 0) {
    std::string filter_cmd = CANProtocol::SLCAN::CommandBuilder::setAcceptanceFilter(filter_id, filter_mask);
    write_command(filter_cmd + "\r", std::chrono::milliseconds(500));
  }

  // 4) Enable timestamps (optional, for debugging)
  write_command("Z1\r", std::chrono::milliseconds(200));

  // 5) Open channel
  if (!write_command("O\r", std::chrono::milliseconds(500))) {
    std::cerr << "Failed to open SLCAN channel\n";
    return false;
  }

  return true;
}

bool SerialDriver::parse_slcan_frame(const std::string& line, CANProtocol::CANFrame& f) {
  bool success = CANProtocol::SLCAN::FrameParser::parseFrame(line, f);
  if (!success) {
    stats_.parse_errors++;
  }
  return success;
}

bool SerialDriver::read_and_buffer_frames(std::chrono::milliseconds timeout) {
  std::string line;
  if (!read_until_cr(line, timeout)) return false;
  if (line.empty()) return false;

  CANProtocol::CANFrame f;
  if (parse_slcan_frame(line, f)) {
    std::lock_guard<std::mutex> lock(rx_mutex_);
    rx_queue_.push_back(f);
    return true;
  }
  return false;
}

bool SerialDriver::send(const CANProtocol::CANFrame& f) {
  if (fd_ < 0) return false;

  std::string slcan_cmd = CANProtocol::SLCAN::CommandBuilder::transmitFrame(f);
  slcan_cmd += "\r";

  ssize_t n = ::write(fd_, slcan_cmd.data(), slcan_cmd.size());
  if (n != static_cast<ssize_t>(slcan_cmd.size())) return false;

  // Read acknowledgement (CR or bell)
  std::string ack;
  return read_until_cr(ack, std::chrono::milliseconds(100));
}

bool SerialDriver::recv(CANProtocol::CANFrame& f, std::chrono::milliseconds timeout) {
  // Check buffered frames first
  {
    std::lock_guard<std::mutex> lock(rx_mutex_);
    if (!rx_queue_.empty()) {
      f = rx_queue_.front();
      rx_queue_.pop_front();
      return true;
    }
  }

  // Read new frame(s) with timeout
  auto deadline = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline) return false;
    auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);

    if (read_and_buffer_frames(remain)) {
      std::lock_guard<std::mutex> lock(rx_mutex_);
      if (!rx_queue_.empty()) {
        f = rx_queue_.front();
        rx_queue_.pop_front();
        return true;
      }
    }
  }
}

// Enhanced frame operations
bool SerialDriver::send_can_frame(const CanFrame& frame) {
  // Check TX queue capacity (back-pressure)
  {
    std::lock_guard<std::mutex> lock(tx_mutex_);
    if (tx_queue_.size() >= tx_queue_max_size_) {
      stats_.tx_queue_overflows++;
      CanFrame evt_frame = frame;
      invoke_event_callback(FrameEvent::QueueFull, evt_frame);
      return false; // Queue full - apply back-pressure
    }
    tx_queue_.push_back(frame);
  }
  
  // Send frame immediately if queue was empty
  CanFrame to_send;
  {
    std::lock_guard<std::mutex> lock(tx_mutex_);
    if (tx_queue_.empty()) return true;
    to_send = tx_queue_.front();
    tx_queue_.pop_front();
  }
  
  bool success = send(to_send);
  if (success) {
    stats_.frames_sent++;
    invoke_event_callback(FrameEvent::Transmitted, to_send);
  }
  
  return success;
}

bool SerialDriver::receive_frame(CanFrame& out) {
  CANProtocol::CANFrame base_frame;
  bool success = recv(base_frame, std::chrono::milliseconds(0));
  
  if (!success) {
    return false;
  }
  
  // Convert to enhanced CanFrame
  out.id = base_frame.id;
  out.dlc = base_frame.dlc;
  out.flags = base_frame.flags;
  std::copy(base_frame.data.begin(), base_frame.data.begin() + 8, out.data.begin());
  out.timestamp_us = base_frame.timestamp_us;
  
  // Capture timestamp
  out.timestamp = std::chrono::steady_clock::now();
  
  // Classify flow control
  out.classify_flow_control();
  
  // Update statistics
  update_stats(out, FrameEvent::Received);
  
  // Invoke callbacks
  invoke_rx_callback(out);
  
  if (out.flags & CANProtocol::CAN_ERR_FLAG) {
    invoke_event_callback(FrameEvent::Error, out);
  } else if (out.fc_type != FlowControlType::Unknown) {
    invoke_event_callback(FrameEvent::FlowControl, out);
  } else {
    invoke_event_callback(FrameEvent::Received, out);
  }
  
  return true;
}

// Helper methods
void SerialDriver::invoke_rx_callback(const CanFrame& frame) {
  if (rx_callback_) {
    rx_callback_(frame);
  }
}

void SerialDriver::invoke_event_callback(FrameEvent event, const CanFrame& frame) {
  if (event_callback_) {
    event_callback_(event, frame);
  }
}

void SerialDriver::update_stats(const CanFrame& frame, FrameEvent event) {
  switch (event) {
    case FrameEvent::Received:
      stats_.frames_received++;
      break;
    case FrameEvent::Transmitted:
      stats_.frames_sent++;
      break;
    case FrameEvent::Error:
      stats_.error_frames++;
      break;
    default:
      break;
  }
  
  // Update FC statistics
  switch (frame.fc_type) {
    case FlowControlType::CTS:
      stats_.fc_cts_count++;
      break;
    case FlowControlType::WT:
      stats_.fc_wt_count++;
      break;
    case FlowControlType::OVFL:
      stats_.fc_ovfl_count++;
      break;
    default:
      break;
  }
}

} // namespace slcan
