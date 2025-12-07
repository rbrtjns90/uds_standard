#include "can_slcan.hpp"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

namespace CANProtocol {

// ============================================================================
// SLCAN CommandBuilder Implementation
// ============================================================================

namespace SLCAN {

char CommandBuilder::bitrateToCode(uint32_t bitrate) {
    switch (bitrate) {
        case CAN_BITRATE_10K:  return BITRATE_10K;
        case CAN_BITRATE_20K:  return BITRATE_20K;
        case CAN_BITRATE_50K:  return BITRATE_50K;
        case CAN_BITRATE_100K: return BITRATE_100K;
        case CAN_BITRATE_125K: return BITRATE_125K;
        case CAN_BITRATE_250K: return BITRATE_250K;
        case CAN_BITRATE_500K: return BITRATE_500K;
        case CAN_BITRATE_800K: return BITRATE_800K;
        case CAN_BITRATE_1M:   return BITRATE_1M;
        default:               return BITRATE_500K; // Default to 500kbps
    }
}

std::string CommandBuilder::uint8ToHex(uint8_t value) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
        << static_cast<int>(value);
    return oss.str();
}

std::string CommandBuilder::dataToHex(const uint8_t* data, uint8_t len) {
    std::string result;
    result.reserve(len * 2);
    for (uint8_t i = 0; i < len; ++i) {
        result += uint8ToHex(data[i]);
    }
    return result;
}

std::string CommandBuilder::setupBitrate(uint32_t bitrate) {
    std::string cmd;
    cmd += CMD_SETUP_STD_BITRATE;
    cmd += bitrateToCode(bitrate);
    cmd += RESP_OK;
    return cmd;
}

std::string CommandBuilder::openChannel() {
    std::string cmd;
    cmd += CMD_OPEN;
    cmd += RESP_OK;
    return cmd;
}

std::string CommandBuilder::closeChannel() {
    std::string cmd;
    cmd += CMD_CLOSE;
    cmd += RESP_OK;
    return cmd;
}

std::string CommandBuilder::listenOnlyMode() {
    std::string cmd;
    cmd += CMD_LISTEN_ONLY;
    cmd += RESP_OK;
    return cmd;
}

std::string CommandBuilder::transmitFrame(const CANFrame& frame) {
    if (frame.isRTR()) {
        return transmitRTR(frame.getIdentifier(), frame.dlc, frame.isExtended());
    } else if (frame.isExtended()) {
        return transmitExtendedFrame(frame.getIdentifier(), frame.data.data(), frame.dlc);
    } else {
        return transmitStandardFrame(frame.getIdentifier(), frame.data.data(), frame.dlc);
    }
}

std::string CommandBuilder::transmitStandardFrame(uint32_t id, const uint8_t* data, uint8_t len) {
    std::ostringstream oss;
    
    // Validate inputs
    if (id > CAN_SFF_MASK || len > CAN_MAX_DLEN) {
        return "";
    }
    
    // Format: tiiildata\r
    // t = command, iii = 3-digit hex ID, l = length, data = hex data bytes
    oss << CMD_TRANSMIT_STD
        << std::hex << std::uppercase << std::setw(3) << std::setfill('0') << id
        << std::dec << static_cast<int>(len);
    
    // Add data bytes
    for (uint8_t i = 0; i < len; ++i) {
        oss << uint8ToHex(data[i]);
    }
    
    oss << RESP_OK;
    return oss.str();
}

std::string CommandBuilder::transmitExtendedFrame(uint32_t id, const uint8_t* data, uint8_t len) {
    std::ostringstream oss;
    
    // Validate inputs
    if (id > CAN_EFF_MASK || len > CAN_MAX_DLEN) {
        return "";
    }
    
    // Format: Tiiiiiiiildata\r
    // T = command, iiiiiiii = 8-digit hex ID, l = length, data = hex data bytes
    oss << CMD_TRANSMIT_EXT
        << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << id
        << std::dec << static_cast<int>(len);
    
    // Add data bytes
    for (uint8_t i = 0; i < len; ++i) {
        oss << uint8ToHex(data[i]);
    }
    
    oss << RESP_OK;
    return oss.str();
}

std::string CommandBuilder::transmitRTR(uint32_t id, uint8_t len, bool extended) {
    std::ostringstream oss;
    
    if (extended) {
        // Validate extended ID
        if (id > CAN_EFF_MASK || len > CAN_MAX_DLEN) {
            return "";
        }
        
        // Format: Riiiiiiiil\r
        oss << CMD_TRANSMIT_EXT_RTR
            << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << id
            << std::dec << static_cast<int>(len);
    } else {
        // Validate standard ID
        if (id > CAN_SFF_MASK || len > CAN_MAX_DLEN) {
            return "";
        }
        
        // Format: riiil\r
        oss << CMD_TRANSMIT_STD_RTR
            << std::hex << std::uppercase << std::setw(3) << std::setfill('0') << id
            << std::dec << static_cast<int>(len);
    }
    
    oss << RESP_OK;
    return oss.str();
}

std::string CommandBuilder::getVersion() {
    std::string cmd;
    cmd += CMD_GET_VERSION;
    cmd += RESP_OK;
    return cmd;
}

std::string CommandBuilder::getSerial() {
    std::string cmd;
    cmd += CMD_GET_SERIAL;
    cmd += RESP_OK;
    return cmd;
}

std::string CommandBuilder::enableTimestamp(bool enable) {
    std::string cmd;
    cmd += enable ? CMD_TIMESTAMP_ON : CMD_TIMESTAMP_OFF;
    cmd += RESP_OK;
    return cmd;
}

std::string CommandBuilder::setAcceptanceFilter(uint32_t code, uint32_t mask) {
    std::ostringstream oss;
    
    // Set acceptance code register (ACR)
    oss << CMD_SET_ACR
        << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << code
        << RESP_OK;
    
    std::string cmd = oss.str();
    oss.str("");
    
    // Set acceptance mask register (AMR)
    oss << CMD_SET_AMR
        << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << mask
        << RESP_OK;
    
    cmd += oss.str();
    return cmd;
}

// ============================================================================
// SLCAN FrameParser Implementation
// ============================================================================

uint8_t FrameParser::hexCharToByte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

bool FrameParser::hexStringToBytes(const std::string& hex, uint8_t* bytes, size_t maxLen) {
    if (hex.length() % 2 != 0 || hex.length() / 2 > maxLen) {
        return false;
    }
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        if (!std::isxdigit(hex[i]) || !std::isxdigit(hex[i + 1])) {
            return false;
        }
        bytes[i / 2] = (hexCharToByte(hex[i]) << 4) | hexCharToByte(hex[i + 1]);
    }
    
    return true;
}

bool FrameParser::isValidFrame(const std::string& slcanStr) {
    if (slcanStr.empty()) return false;
    
    char type = slcanStr[0];
    return (type == CMD_TRANSMIT_STD || type == CMD_TRANSMIT_EXT ||
            type == CMD_TRANSMIT_STD_RTR || type == CMD_TRANSMIT_EXT_RTR);
}

bool FrameParser::parseStandardFrame(const std::string& slcanStr, CANFrame& frame) {
    // Format: tiiildata or riiil
    // Minimum length: t + 3 hex ID + 1 length = 5 characters
    if (slcanStr.length() < 5) return false;
    
    bool isRTR = (slcanStr[0] == CMD_TRANSMIT_STD_RTR);
    
    // Parse ID (3 hex digits)
    std::string idStr = slcanStr.substr(1, 3);
    uint32_t id = 0;
    for (char c : idStr) {
        if (!std::isxdigit(c)) return false;
        id = (id << 4) | hexCharToByte(c);
    }
    
    // Validate standard ID range
    if (id > CAN_SFF_MASK) return false;
    
    // Parse length
    if (!std::isdigit(slcanStr[4])) return false;
    uint8_t len = slcanStr[4] - '0';
    if (len > CAN_MAX_DLEN) return false;
    
    // Setup frame
    frame.id = id;
    frame.dlc = len;
    frame.setExtended(false);
    frame.setRTR(isRTR);
    
    if (!isRTR && len > 0) {
        // Parse data bytes (2 hex chars per byte)
        if (slcanStr.length() < 5 + (len * 2)) return false;
        
        std::string dataStr = slcanStr.substr(5, len * 2);
        return hexStringToBytes(dataStr, frame.data.data(), len);
    }
    
    return true;
}

bool FrameParser::parseExtendedFrame(const std::string& slcanStr, CANFrame& frame) {
    // Format: Tiiiiiiiildata or Riiiiiiiil
    // Minimum length: T + 8 hex ID + 1 length = 10 characters
    if (slcanStr.length() < 10) return false;
    
    bool isRTR = (slcanStr[0] == CMD_TRANSMIT_EXT_RTR);
    
    // Parse ID (8 hex digits)
    std::string idStr = slcanStr.substr(1, 8);
    uint32_t id = 0;
    for (char c : idStr) {
        if (!std::isxdigit(c)) return false;
        id = (id << 4) | hexCharToByte(c);
    }
    
    // Validate extended ID range
    if (id > CAN_EFF_MASK) return false;
    
    // Parse length
    if (!std::isdigit(slcanStr[9])) return false;
    uint8_t len = slcanStr[9] - '0';
    if (len > CAN_MAX_DLEN) return false;
    
    // Setup frame
    frame.id = id;
    frame.dlc = len;
    frame.setExtended(true);
    frame.setRTR(isRTR);
    
    if (!isRTR && len > 0) {
        // Parse data bytes (2 hex chars per byte)
        if (slcanStr.length() < 10 + (len * 2)) return false;
        
        std::string dataStr = slcanStr.substr(10, len * 2);
        return hexStringToBytes(dataStr, frame.data.data(), len);
    }
    
    return true;
}

bool FrameParser::parseFrame(const std::string& slcanStr, CANFrame& frame) {
    if (slcanStr.empty()) return false;
    
    char type = slcanStr[0];
    
    // Check for error frame first
    if (type == FRAME_ERROR) {
        CANErrorType errorType;
        if (parseErrorFrame(slcanStr, frame, errorType)) {
            // Mark as error frame
            frame.flags |= CAN_ERR_FLAG;
            return true;
        }
        return false;
    }
    
    if (!isValidFrame(slcanStr)) return false;
    
    if (type == CMD_TRANSMIT_STD || type == CMD_TRANSMIT_STD_RTR) {
        if (!parseStandardFrame(slcanStr, frame)) return false;
    } else if (type == CMD_TRANSMIT_EXT || type == CMD_TRANSMIT_EXT_RTR) {
        if (!parseExtendedFrame(slcanStr, frame)) return false;
    } else {
        return false;
    }
    
    // Try to parse timestamp if present
    uint32_t timestamp_ms = 0;
    if (parseTimestamp(slcanStr, timestamp_ms)) {
        frame.timestamp_us = static_cast<uint64_t>(timestamp_ms) * 1000;
    }
    
    return true;
}

bool FrameParser::parseErrorFrame(const std::string& slcanStr, CANFrame& frame, CANErrorType& errorType) {
    // Error frame format: Fxxxxxxxx (F + 8 hex digits)
    if (slcanStr.length() < 9) return false;
    if (slcanStr[0] != FRAME_ERROR) return false;
    
    // Parse error code (8 hex digits)
    std::string errorCodeStr = slcanStr.substr(1, 8);
    uint32_t errorCode = 0;
    for (char c : errorCodeStr) {
        if (!std::isxdigit(c)) return false;
        errorCode = (errorCode << 4) | hexCharToByte(c);
    }
    
    // Setup error frame
    frame.id = errorCode;
    frame.dlc = 0;
    frame.flags = CAN_ERR_FLAG;
    
    // Decode error type from error code
    // Note: Error code format is device-specific, this is a generic interpretation
    if (errorCode & 0x0001) errorType = CANErrorType::BIT_ERROR;
    else if (errorCode & 0x0002) errorType = CANErrorType::STUFF_ERROR;
    else if (errorCode & 0x0004) errorType = CANErrorType::FORM_ERROR;
    else if (errorCode & 0x0008) errorType = CANErrorType::ACK_ERROR;
    else if (errorCode & 0x0010) errorType = CANErrorType::CRC_ERROR;
    else if (errorCode & 0x0020) errorType = CANErrorType::BUS_OFF;
    else if (errorCode & 0x0040) errorType = CANErrorType::ERROR_PASSIVE;
    else errorType = CANErrorType::NO_ERROR;
    
    return true;
}

bool FrameParser::parseTimestamp(const std::string& slcanStr, uint32_t& timestamp_ms) {
    // Timestamp format: frame data followed by 4 hex digits [tttt]
    // Find timestamp at end of string
    if (slcanStr.length() < 4) return false;
    
    // Look for timestamp pattern at the end (before \r if present)
    size_t tsStart = slcanStr.length() - 4;
    if (slcanStr.back() == '\r') {
        if (slcanStr.length() < 5) return false;
        tsStart = slcanStr.length() - 5;
    }
    
    // Check if last 4 characters are hex digits
    for (size_t i = 0; i < 4; ++i) {
        if (!std::isxdigit(slcanStr[tsStart + i])) {
            return false;
        }
    }
    
    // Parse timestamp (4 hex digits = 0-65535 ms)
    timestamp_ms = 0;
    for (size_t i = 0; i < 4; ++i) {
        timestamp_ms = (timestamp_ms << 4) | hexCharToByte(slcanStr[tsStart + i]);
    }
    
    return true;
}

} // namespace SLCAN

// ============================================================================
// CANErrorCounter Implementation
// ============================================================================

void CANErrorCounter::incrementTxError(uint8_t amount) {
    // Transmit errors increment by 8 (as per CAN specification)
    if (tx_error_count + amount <= 255) {
        tx_error_count += amount;
    } else {
        tx_error_count = 255;
    }
}

void CANErrorCounter::incrementRxError(uint8_t amount) {
    // Receive errors increment by 1 (as per CAN specification)
    if (rx_error_count + amount <= 255) {
        rx_error_count += amount;
    } else {
        rx_error_count = 255;
    }
}

void CANErrorCounter::decrementTxError(uint8_t amount) {
    if (tx_error_count >= amount) {
        tx_error_count -= amount;
    } else {
        tx_error_count = 0;
    }
}

void CANErrorCounter::decrementRxError(uint8_t amount) {
    if (rx_error_count >= amount) {
        rx_error_count -= amount;
    } else {
        rx_error_count = 0;
    }
}

void CANErrorCounter::reset() {
    tx_error_count = 0;
    rx_error_count = 0;
}

} // namespace CANProtocol
