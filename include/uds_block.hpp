#pragma once
/**
 * @file uds_block.hpp
 * @brief Block Transfer Services - ISO 14229-1:2013 Section 14
 * 
 * This module provides optimized block transfer capabilities for firmware
 * download and upload operations.
 * 
 * ============================================================================
 * ISO 14229-1:2013 UPLOAD/DOWNLOAD REFERENCE
 * ============================================================================
 * 
 * RequestDownload (0x34) - Section 14.2 (p. 270):
 *   Purpose: Initiate data transfer from client to server (flash programming)
 * 
 *   Request Format:
 *     [0x34] [dataFormatIdentifier] [addressAndLengthFormatIdentifier]
 *            [memoryAddress...] [memorySize...]
 * 
 *   dataFormatIdentifier:
 *     High nibble: compressionMethod (0=none, 1-F=OEM specific)
 *     Low nibble:  encryptingMethod (0=none, 1-F=OEM specific)
 * 
 *   Response Format:
 *     [0x74] [lengthFormatIdentifier] [maxNumberOfBlockLength...]
 * 
 * RequestUpload (0x35) - Section 14.3 (p. 275):
 *   Purpose: Initiate data transfer from server to client (memory read)
 *   Same format as RequestDownload
 * 
 * TransferData (0x36) - Section 14.4 (p. 280):
 *   Purpose: Transfer data blocks during download/upload
 * 
 *   Request Format:
 *     [0x36] [blockSequenceCounter] [transferRequestParameterRecord...]
 * 
 *   blockSequenceCounter:
 *     - Starts at 0x01 for first block
 *     - Increments for each block
 *     - Wraps from 0xFF to 0x00 (not 0x01!)
 * 
 *   Response Format:
 *     [0x76] [blockSequenceCounter] [transferResponseParameterRecord...]
 * 
 * RequestTransferExit (0x37) - Section 14.5 (p. 285):
 *   Purpose: Terminate data transfer
 * 
 *   Request Format:
 *     [0x37] [transferRequestParameterRecord...]
 * 
 *   Response Format:
 *     [0x77] [transferResponseParameterRecord...]
 * 
 * Critical NRCs for Transfer (Annex A, Table A.1):
 *   0x70: uploadDownloadNotAccepted - Transfer rejected
 *   0x71: transferDataSuspended - Transfer paused
 *   0x72: generalProgrammingFailure - Flash write failed
 *   0x73: wrongBlockSequenceCounter - Block counter mismatch (RETRY SAME BLOCK!)
 * 
 * Block Counter Recovery:
 *   On NRC 0x73, client should retry with the expected block counter
 *   returned in the negative response.
 */

#include "uds.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <chrono>
#include <atomic>

namespace uds {
namespace block {

// ============================================================================
// Transfer Configuration
// ============================================================================

/**
 * @brief Block transfer configuration
 */
struct TransferConfig {
    uint32_t block_size = 256;              ///< Size of each transfer block
    uint32_t max_retries = 3;               ///< Max retries per block on failure
    uint32_t retry_delay_ms = 100;          ///< Delay between retries
    bool verify_blocks = true;              ///< Verify each block after write
    bool use_crc = true;                    ///< Include CRC in transfers
    uint32_t timeout_ms = 5000;             ///< Timeout per block operation
    
    TransferConfig() = default;
    
    /**
     * @brief Create config optimized for speed
     */
    static TransferConfig fast() {
        TransferConfig cfg;
        cfg.block_size = 4096;
        cfg.verify_blocks = false;
        cfg.max_retries = 1;
        return cfg;
    }
    
    /**
     * @brief Create config optimized for reliability
     */
    static TransferConfig reliable() {
        TransferConfig cfg;
        cfg.block_size = 256;
        cfg.verify_blocks = true;
        cfg.max_retries = 5;
        cfg.retry_delay_ms = 200;
        return cfg;
    }
    
    /**
     * @brief Create config for slow/unreliable connections
     */
    static TransferConfig conservative() {
        TransferConfig cfg;
        cfg.block_size = 64;
        cfg.verify_blocks = true;
        cfg.max_retries = 10;
        cfg.retry_delay_ms = 500;
        cfg.timeout_ms = 10000;
        return cfg;
    }
};

// ============================================================================
// Transfer State and Progress
// ============================================================================

/**
 * @brief Transfer state
 */
enum class TransferState {
    Idle,               ///< No transfer in progress
    Preparing,          ///< Preparing transfer (requesting download/upload)
    Transferring,       ///< Actively transferring data
    Verifying,          ///< Verifying transferred data
    Completing,         ///< Completing transfer (RequestTransferExit)
    Completed,          ///< Transfer completed successfully
    Failed,             ///< Transfer failed
    Cancelled           ///< Transfer was cancelled
};

/**
 * @brief Transfer progress information
 */
struct TransferProgress {
    TransferState state = TransferState::Idle;
    uint64_t total_bytes = 0;               ///< Total bytes to transfer
    uint64_t transferred_bytes = 0;         ///< Bytes transferred so far
    uint32_t current_block = 0;             ///< Current block number
    uint32_t total_blocks = 0;              ///< Total number of blocks
    uint32_t retry_count = 0;               ///< Retries for current block
    uint32_t total_retries = 0;             ///< Total retries across all blocks
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_update;
    std::string status_message;
    
    /**
     * @brief Get progress percentage (0-100)
     */
    float percentage() const {
        if (total_bytes == 0) return 0.0f;
        return (static_cast<float>(transferred_bytes) / total_bytes) * 100.0f;
    }
    
    /**
     * @brief Get elapsed time
     */
    std::chrono::milliseconds elapsed() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
    }
    
    /**
     * @brief Get estimated time remaining
     */
    std::chrono::milliseconds estimated_remaining() const {
        if (transferred_bytes == 0) return std::chrono::milliseconds(0);
        auto elapsed_ms = elapsed().count();
        auto rate = static_cast<double>(transferred_bytes) / elapsed_ms;
        if (rate <= 0) return std::chrono::milliseconds(0);
        auto remaining_bytes = total_bytes - transferred_bytes;
        return std::chrono::milliseconds(static_cast<int64_t>(remaining_bytes / rate));
    }
    
    /**
     * @brief Get transfer rate in bytes per second
     */
    double bytes_per_second() const {
        auto ms = elapsed().count();
        if (ms == 0) return 0.0;
        return (static_cast<double>(transferred_bytes) / ms) * 1000.0;
    }
};

/**
 * @brief Progress callback type
 */
using ProgressCallback = std::function<void(const TransferProgress&)>;

/**
 * @brief Cancellation token for aborting transfers
 */
class CancellationToken {
public:
    CancellationToken() : cancelled_(false) {}
    
    void cancel() { cancelled_ = true; }
    bool is_cancelled() const { return cancelled_; }
    void reset() { cancelled_ = false; }
    
private:
    std::atomic<bool> cancelled_;
};

// ============================================================================
// Transfer Results
// ============================================================================

/**
 * @brief Result of a block transfer operation
 */
struct TransferResult {
    bool ok = false;
    TransferState final_state = TransferState::Idle;
    uint64_t bytes_transferred = 0;
    uint32_t blocks_transferred = 0;
    uint32_t total_retries = 0;
    std::chrono::milliseconds duration{0};
    std::optional<uint32_t> crc32;          ///< CRC of transferred data
    NegativeResponseCode nrc = static_cast<NegativeResponseCode>(0x00);
    std::string error_message;
    
    /**
     * @brief Get effective transfer rate
     */
    double bytes_per_second() const {
        if (duration.count() == 0) return 0.0;
        return (static_cast<double>(bytes_transferred) / duration.count()) * 1000.0;
    }
};

// ============================================================================
// Block Transfer Manager
// ============================================================================

/**
 * @brief Manages block-oriented transfers with resume capability
 */
class BlockTransferManager {
public:
    explicit BlockTransferManager(Client& client);
    
    // ========================================================================
    // Download (ECU -> Tester)
    // ========================================================================
    
    /**
     * @brief Download data from ECU memory
     * @param address Start address
     * @param size Number of bytes to download
     * @param config Transfer configuration
     * @param progress Progress callback (optional)
     * @param cancel Cancellation token (optional)
     * @return Transfer result with downloaded data
     */
    TransferResult download(uint32_t address, uint32_t size,
                           const TransferConfig& config = TransferConfig(),
                           ProgressCallback progress = nullptr,
                           CancellationToken* cancel = nullptr);
    
    /**
     * @brief Get downloaded data after successful transfer
     */
    const std::vector<uint8_t>& downloaded_data() const { return download_buffer_; }
    
    // ========================================================================
    // Upload (Tester -> ECU)
    // ========================================================================
    
    /**
     * @brief Upload data to ECU memory
     * @param address Start address
     * @param data Data to upload
     * @param config Transfer configuration
     * @param progress Progress callback (optional)
     * @param cancel Cancellation token (optional)
     * @return Transfer result
     */
    TransferResult upload(uint32_t address, const std::vector<uint8_t>& data,
                         const TransferConfig& config = TransferConfig(),
                         ProgressCallback progress = nullptr,
                         CancellationToken* cancel = nullptr);
    
    // ========================================================================
    // Resume Support
    // ========================================================================
    
    /**
     * @brief Check if a transfer can be resumed
     */
    bool can_resume() const { return resume_state_.valid; }
    
    /**
     * @brief Get resume information
     */
    struct ResumeInfo {
        bool valid = false;
        bool is_upload = false;
        uint32_t address = 0;
        uint64_t total_size = 0;
        uint64_t transferred = 0;
        uint32_t next_block = 0;
    };
    ResumeInfo get_resume_info() const { return resume_state_; }
    
    /**
     * @brief Resume an interrupted transfer
     * @param config Transfer configuration
     * @param progress Progress callback
     * @param cancel Cancellation token
     * @return Transfer result
     */
    TransferResult resume(const TransferConfig& config = TransferConfig(),
                         ProgressCallback progress = nullptr,
                         CancellationToken* cancel = nullptr);
    
    /**
     * @brief Clear resume state
     */
    void clear_resume_state() { resume_state_ = ResumeInfo(); }
    
    // ========================================================================
    // Verification
    // ========================================================================
    
    /**
     * @brief Verify uploaded data by reading back and comparing
     * @param address Start address
     * @param expected Expected data
     * @param config Transfer configuration
     * @return True if verification passed
     */
    bool verify_upload(uint32_t address, const std::vector<uint8_t>& expected,
                      const TransferConfig& config = TransferConfig());
    
    /**
     * @brief Calculate CRC32 of ECU memory region
     * @param address Start address
     * @param size Size of region
     * @return CRC32 value or nullopt on error
     */
    std::optional<uint32_t> calculate_remote_crc(uint32_t address, uint32_t size);
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * @brief Set data format identifier for transfers
     */
    void set_data_format(uint8_t format) { data_format_ = format; }
    
    /**
     * @brief Set address and length format
     */
    void set_address_format(uint8_t addr_bytes, uint8_t len_bytes) {
        address_bytes_ = addr_bytes;
        length_bytes_ = len_bytes;
    }
    
    /**
     * @brief Get current transfer progress
     */
    TransferProgress current_progress() const { return progress_; }

private:
    Client& client_;
    std::vector<uint8_t> download_buffer_;
    std::vector<uint8_t> upload_data_;
    TransferProgress progress_;
    ResumeInfo resume_state_;
    
    uint8_t data_format_ = 0x00;
    uint8_t address_bytes_ = 4;
    uint8_t length_bytes_ = 4;
    uint8_t block_sequence_ = 0;
    
    // Internal helpers
    bool request_download(uint32_t address, uint32_t size, uint32_t& max_block);
    bool request_upload(uint32_t address, uint32_t size, uint32_t& max_block);
    bool transfer_block(const std::vector<uint8_t>& data, bool is_upload);
    bool request_transfer_exit();
    void update_progress(TransferState state, const std::string& msg = "");
    std::vector<uint8_t> encode_address_and_length(uint32_t address, uint32_t length);
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Calculate CRC32 of data
 */
uint32_t calculate_crc32(const std::vector<uint8_t>& data);

/**
 * @brief Calculate CRC32 with initial value (for incremental calculation)
 */
uint32_t calculate_crc32(const std::vector<uint8_t>& data, uint32_t initial);

/**
 * @brief Format transfer result for display
 */
std::string format_transfer_result(const TransferResult& result);

/**
 * @brief Format progress for display
 */
std::string format_progress(const TransferProgress& progress);

/**
 * @brief Format bytes per second with appropriate units
 */
std::string format_transfer_rate(double bytes_per_second);

/**
 * @brief Format duration for display
 */
std::string format_duration(std::chrono::milliseconds duration);

} // namespace block
} // namespace uds
