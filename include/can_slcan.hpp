#ifndef CAN_SLCAN_HPP
#define CAN_SLCAN_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace CANProtocol {

// ============================================================================
// CAN Protocol Constants (ISO 11898)
// ============================================================================

constexpr uint32_t CAN_MAX_DLEN = 8;           // Maximum data length for standard CAN
constexpr uint32_t CANFD_MAX_DLEN = 64;        // Maximum data length for CAN FD
constexpr uint32_t CAN_SFF_ID_BITS = 11;       // Standard Frame Format ID bits
constexpr uint32_t CAN_EFF_ID_BITS = 29;       // Extended Frame Format ID bits
constexpr uint32_t CAN_SFF_MASK = 0x000007FFU; // Standard ID mask (11 bits)
constexpr uint32_t CAN_EFF_MASK = 0x1FFFFFFFU; // Extended ID mask (29 bits)
constexpr uint32_t CAN_ERR_MASK = 0x1FFFFFFFU; // Error mask

// CAN ID Flags (for id field)
constexpr uint32_t CAN_EFF_FLAG = 0x80000000U; // Extended Frame Format flag

// CAN Frame Flags (for flags field - 8-bit)
constexpr uint8_t CAN_RTR_FLAG = 0x01;  // Remote Transmission Request flag
constexpr uint8_t CAN_ERR_FLAG = 0x02;  // Error frame flag

// CAN Bit Rates (common)
constexpr uint32_t CAN_BITRATE_1M = 1000000;
constexpr uint32_t CAN_BITRATE_800K = 800000;
constexpr uint32_t CAN_BITRATE_500K = 500000;
constexpr uint32_t CAN_BITRATE_250K = 250000;
constexpr uint32_t CAN_BITRATE_125K = 125000;
constexpr uint32_t CAN_BITRATE_100K = 100000;
constexpr uint32_t CAN_BITRATE_50K = 50000;
constexpr uint32_t CAN_BITRATE_20K = 20000;
constexpr uint32_t CAN_BITRATE_10K = 10000;

// ============================================================================
// CAN Frame Types (as defined in ISO 11898)
// ============================================================================

enum class CANFrameType : uint8_t {
    DATA_FRAME = 0,    // Standard data frame
    REMOTE_FRAME = 1,  // Remote transmission request
    ERROR_FRAME = 2,   // Error frame (6 dominant bits)
    OVERLOAD_FRAME = 3 // Overload frame
};

// ============================================================================
// CAN Error Types
// ============================================================================

enum class CANErrorType : uint8_t {
    NO_ERROR = 0,
    BIT_ERROR = 1,           // Bit monitoring error
    STUFF_ERROR = 2,         // Bit stuffing error
    FORM_ERROR = 3,          // Frame format error
    ACK_ERROR = 4,           // Acknowledgement error
    CRC_ERROR = 5,           // CRC checksum error
    BUS_OFF = 6,             // Node is bus off
    ERROR_PASSIVE = 7,       // Node is error passive
    TX_TIMEOUT = 8,          // Transmission timeout
    RX_OVERFLOW = 9          // Receive buffer overflow
};

// ============================================================================
// CAN Frame Structure (Classical CAN & CAN FD)
// ============================================================================

struct CANFrame {
    uint32_t id;                          // CAN identifier (11 or 29 bit)
    uint8_t dlc;                          // Data Length Code (0-8 for CAN, 0-64 for CAN FD)
    uint8_t flags;                        // Frame flags (RTR, EFF, ERR)
    std::array<uint8_t, CANFD_MAX_DLEN> data; // Data payload
    uint64_t timestamp_us;                // Timestamp in microseconds
    
    CANFrame() : id(0), dlc(0), flags(0), data{}, timestamp_us(0) {}
    
    // Helper methods
    bool isExtended() const { return (id & CAN_EFF_FLAG) != 0; }
    bool isRTR() const { return (flags & CAN_RTR_FLAG) != 0; }
    bool isError() const { return (flags & CAN_ERR_FLAG) != 0; }
    uint32_t getIdentifier() const { return id & (isExtended() ? CAN_EFF_MASK : CAN_SFF_MASK); }
    
    void setExtended(bool extended) {
        if (extended) {
            id |= CAN_EFF_FLAG;
        } else {
            id &= ~CAN_EFF_FLAG;
        }
    }
    
    void setRTR(bool rtr) {
        if (rtr) {
            flags |= CAN_RTR_FLAG;
        } else {
            flags &= ~CAN_RTR_FLAG;
        }
    }
};

// ============================================================================
// SLCAN Protocol (Serial Line CAN - Lawicel Protocol)
// ============================================================================

namespace SLCAN {

// SLCAN Commands
constexpr char CMD_SETUP_STD_BITRATE = 'S';    // Setup standard bit rate
constexpr char CMD_SETUP_BTR = 's';             // Setup bit timing register
constexpr char CMD_OPEN = 'O';                  // Open CAN channel
constexpr char CMD_LISTEN_ONLY = 'L';           // Listen only mode
constexpr char CMD_CLOSE = 'C';                 // Close CAN channel
constexpr char CMD_TRANSMIT_STD = 't';          // Transmit standard frame
constexpr char CMD_TRANSMIT_EXT = 'T';          // Transmit extended frame
constexpr char CMD_TRANSMIT_STD_RTR = 'r';      // Transmit standard RTR frame
constexpr char CMD_TRANSMIT_EXT_RTR = 'R';      // Transmit extended RTR frame
constexpr char CMD_READ_STATUS = 'F';           // Read status flags
constexpr char CMD_SET_ACR = 'M';               // Set acceptance code register
constexpr char CMD_SET_AMR = 'm';               // Set acceptance mask register
constexpr char CMD_GET_VERSION = 'V';           // Get hardware/software version
constexpr char CMD_GET_SERIAL = 'N';            // Get serial number
constexpr char CMD_TIMESTAMP_ON = 'Z';          // Timestamp on
constexpr char CMD_TIMESTAMP_OFF = 'z';         // Timestamp off
constexpr char CMD_AUTO_POLL_ON = 'X';          // Auto poll/send on
constexpr char CMD_AUTO_POLL_OFF = 'x';         // Auto poll/send off

// Response characters
constexpr char RESP_OK = '\r';                  // Command success
constexpr char RESP_ERROR = '\x07';             // Command error (bell)

// Frame type prefixes (for RX)
constexpr char FRAME_STD = 't';                 // Standard data frame
constexpr char FRAME_EXT = 'T';                 // Extended data frame
constexpr char FRAME_STD_RTR = 'r';             // Standard RTR frame
constexpr char FRAME_EXT_RTR = 'R';             // Extended RTR frame
constexpr char FRAME_ERROR = 'F';               // Error frame (Fxxxxxxxx format)

// Standard bit rate codes (for 'S' command)
constexpr char BITRATE_10K = '0';
constexpr char BITRATE_20K = '1';
constexpr char BITRATE_50K = '2';
constexpr char BITRATE_100K = '3';
constexpr char BITRATE_125K = '4';
constexpr char BITRATE_250K = '5';
constexpr char BITRATE_500K = '6';
constexpr char BITRATE_800K = '7';
constexpr char BITRATE_1M = '8';

// ============================================================================
// SLCAN Command Builder
// ============================================================================

class CommandBuilder {
public:
    // Setup commands
    static std::string setupBitrate(uint32_t bitrate);
    static std::string openChannel();
    static std::string closeChannel();
    static std::string listenOnlyMode();
    
    // Transmission commands
    static std::string transmitFrame(const CANFrame& frame);
    static std::string transmitStandardFrame(uint32_t id, const uint8_t* data, uint8_t len);
    static std::string transmitExtendedFrame(uint32_t id, const uint8_t* data, uint8_t len);
    static std::string transmitRTR(uint32_t id, uint8_t len, bool extended = false);
    
    // Status/configuration commands
    static std::string getVersion();
    static std::string getSerial();
    static std::string enableTimestamp(bool enable);
    static std::string setAcceptanceFilter(uint32_t code, uint32_t mask);
    
private:
    static char bitrateToCode(uint32_t bitrate);
    static std::string uint8ToHex(uint8_t value);
    static std::string dataToHex(const uint8_t* data, uint8_t len);
};

// ============================================================================
// SLCAN Frame Parser
// ============================================================================

class FrameParser {
public:
    // Parse received SLCAN frame string into CANFrame
    static bool parseFrame(const std::string& slcanStr, CANFrame& frame);
    
    // Check if string is a valid SLCAN frame
    static bool isValidFrame(const std::string& slcanStr);
    
    // Parse error frame (Fxxxxxxxx format)
    static bool parseErrorFrame(const std::string& slcanStr, CANFrame& frame, CANErrorType& errorType);
    
    // Parse timestamp if present (format: tiiildata[tttt])
    static bool parseTimestamp(const std::string& slcanStr, uint32_t& timestamp_ms);
    
private:
    static bool parseStandardFrame(const std::string& slcanStr, CANFrame& frame);
    static bool parseExtendedFrame(const std::string& slcanStr, CANFrame& frame);
    static uint8_t hexCharToByte(char c);
    static bool hexStringToBytes(const std::string& hex, uint8_t* bytes, size_t maxLen);
};

} // namespace SLCAN

// ============================================================================
// CAN Bus Error Counter (for error confinement)
// ============================================================================

class CANErrorCounter {
public:
    CANErrorCounter() : tx_error_count(0), rx_error_count(0) {}
    
    void incrementTxError(uint8_t amount = 8);
    void incrementRxError(uint8_t amount = 1);
    void decrementTxError(uint8_t amount = 1);
    void decrementRxError(uint8_t amount = 1);
    void reset();
    
    bool isErrorActive() const { return tx_error_count <= 127 && rx_error_count <= 127; }
    bool isErrorPassive() const { return tx_error_count > 127 || rx_error_count > 127; }
    bool isBusOff() const { return tx_error_count >= 255; }  // Bus off at 256, but uint8_t saturates at 255
    
    uint8_t getTxErrorCount() const { return tx_error_count; }
    uint8_t getRxErrorCount() const { return rx_error_count; }
    
private:
    uint8_t tx_error_count;
    uint8_t rx_error_count;
};

// ============================================================================
// CAN Bit Timing Configuration
// ============================================================================

struct CANBitTiming {
    uint16_t prescaler;         // Bit Rate Prescaler (BRP)
    uint8_t sync_seg;           // Synchronization segment (always 1)
    uint8_t prop_seg;           // Propagation segment
    uint8_t phase_seg1;         // Phase segment 1
    uint8_t phase_seg2;         // Phase segment 2
    uint8_t sjw;                // Synchronization Jump Width
    uint32_t bitrate;           // Resulting bit rate
    
    CANBitTiming() 
        : prescaler(0), sync_seg(1), prop_seg(0), 
          phase_seg1(0), phase_seg2(0), sjw(0), bitrate(0) {}
    
    // Calculate total time quanta per bit
    uint16_t getTotalTQ() const {
        return sync_seg + prop_seg + phase_seg1 + phase_seg2;
    }
    
    // Calculate sampling point percentage
    float getSamplingPoint() const {
        if (getTotalTQ() == 0) return 0.0f;
        return 100.0f * (sync_seg + prop_seg + phase_seg1) / getTotalTQ();
    }
};

// ============================================================================
// CAN Statistics
// ============================================================================

struct CANStatistics {
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t bus_off_count;
    uint64_t error_warning_count;
    
    CANStatistics() : rx_frames(0), tx_frames(0), rx_errors(0), 
                      tx_errors(0), bus_off_count(0), error_warning_count(0) {}
    
    void reset() {
        rx_frames = 0;
        tx_frames = 0;
        rx_errors = 0;
        tx_errors = 0;
        bus_off_count = 0;
        error_warning_count = 0;
    }
};

} // namespace CANProtocol

#endif // CAN_SLCAN_HPP
