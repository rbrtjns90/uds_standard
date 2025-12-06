#include "uds_block.hpp"
#include <sstream>
#include <iomanip>
#include <thread>
#include <cstring>

namespace uds {
namespace block {

// ============================================================================
// CRC32 Implementation
// ============================================================================

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t calculate_crc32(const std::vector<uint8_t>& data) {
    return calculate_crc32(data, 0xFFFFFFFF) ^ 0xFFFFFFFF;
}

uint32_t calculate_crc32(const std::vector<uint8_t>& data, uint32_t initial) {
    uint32_t crc = initial;
    for (uint8_t byte : data) {
        crc = crc32_table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

// ============================================================================
// BlockTransferManager Implementation
// ============================================================================

BlockTransferManager::BlockTransferManager(Client& client)
    : client_(client) {
}

std::vector<uint8_t> BlockTransferManager::encode_address_and_length(uint32_t address, uint32_t length) {
    std::vector<uint8_t> result;
    
    // Address and length format identifier
    uint8_t format = ((length_bytes_ & 0x0F) << 4) | (address_bytes_ & 0x0F);
    result.push_back(format);
    
    // Memory address (big-endian)
    for (int i = address_bytes_ - 1; i >= 0; --i) {
        result.push_back(static_cast<uint8_t>((address >> (i * 8)) & 0xFF));
    }
    
    // Memory size (big-endian)
    for (int i = length_bytes_ - 1; i >= 0; --i) {
        result.push_back(static_cast<uint8_t>((length >> (i * 8)) & 0xFF));
    }
    
    return result;
}

void BlockTransferManager::update_progress(TransferState state, const std::string& msg) {
    progress_.state = state;
    progress_.last_update = std::chrono::steady_clock::now();
    if (!msg.empty()) {
        progress_.status_message = msg;
    }
}

bool BlockTransferManager::request_download(uint32_t address, uint32_t size, uint32_t& max_block) {
    // Build RequestDownload payload
    std::vector<uint8_t> payload;
    payload.push_back(data_format_);  // dataFormatIdentifier
    
    auto addr_len = encode_address_and_length(address, size);
    payload.insert(payload.end(), addr_len.begin(), addr_len.end());
    
    auto response = client_.exchange(SID::RequestDownload, payload);
    if (!response.ok) {
        return false;
    }
    
    // Parse response: lengthFormatIdentifier + maxNumberOfBlockLength
    if (response.payload.empty()) {
        return false;
    }
    
    uint8_t len_format = response.payload[0] >> 4;
    max_block = 0;
    for (uint8_t i = 0; i < len_format && i + 1 < response.payload.size(); ++i) {
        max_block = (max_block << 8) | response.payload[i + 1];
    }
    
    if (max_block == 0) {
        max_block = 256;  // Default if not specified
    }
    
    block_sequence_ = 1;  // Reset sequence counter
    return true;
}

bool BlockTransferManager::request_upload(uint32_t address, uint32_t size, uint32_t& max_block) {
    // Build RequestUpload payload
    std::vector<uint8_t> payload;
    payload.push_back(data_format_);  // dataFormatIdentifier
    
    auto addr_len = encode_address_and_length(address, size);
    payload.insert(payload.end(), addr_len.begin(), addr_len.end());
    
    auto response = client_.exchange(SID::RequestUpload, payload);
    if (!response.ok) {
        return false;
    }
    
    // Parse response same as download
    if (response.payload.empty()) {
        return false;
    }
    
    uint8_t len_format = response.payload[0] >> 4;
    max_block = 0;
    for (uint8_t i = 0; i < len_format && i + 1 < response.payload.size(); ++i) {
        max_block = (max_block << 8) | response.payload[i + 1];
    }
    
    if (max_block == 0) {
        max_block = 256;
    }
    
    block_sequence_ = 1;
    return true;
}

bool BlockTransferManager::transfer_block(const std::vector<uint8_t>& data, bool is_upload) {
    std::vector<uint8_t> payload;
    payload.push_back(block_sequence_);
    
    if (is_upload) {
        payload.insert(payload.end(), data.begin(), data.end());
    }
    
    auto response = client_.exchange(SID::TransferData, payload);
    if (!response.ok) {
        return false;
    }
    
    // For download, extract received data
    if (!is_upload && response.payload.size() > 1) {
        // First byte is block sequence counter echo
        download_buffer_.insert(download_buffer_.end(),
                               response.payload.begin() + 1,
                               response.payload.end());
    }
    
    block_sequence_++;
    if (block_sequence_ == 0) {
        block_sequence_ = 1;  // Wrap around, skip 0
    }
    
    return true;
}

bool BlockTransferManager::request_transfer_exit() {
    std::vector<uint8_t> payload;  // Empty payload for basic exit
    auto response = client_.exchange(SID::RequestTransferExit, payload);
    return response.ok;
}

TransferResult BlockTransferManager::download(uint32_t address, uint32_t size,
                                              const TransferConfig& config,
                                              ProgressCallback progress_cb,
                                              CancellationToken* cancel) {
    TransferResult result;
    download_buffer_.clear();
    download_buffer_.reserve(size);
    
    // Initialize progress
    progress_ = TransferProgress();
    progress_.total_bytes = size;
    progress_.start_time = std::chrono::steady_clock::now();
    update_progress(TransferState::Preparing, "Requesting download...");
    
    if (progress_cb) progress_cb(progress_);
    
    // Request download
    uint32_t max_block_size = 0;
    if (!request_download(address, size, max_block_size)) {
        result.error_message = "RequestDownload failed";
        result.final_state = TransferState::Failed;
        update_progress(TransferState::Failed, result.error_message);
        return result;
    }
    
    // Use smaller of ECU max and config block size
    uint32_t block_size = std::min(max_block_size, config.block_size);
    progress_.total_blocks = (size + block_size - 1) / block_size;
    
    update_progress(TransferState::Transferring, "Downloading...");
    if (progress_cb) progress_cb(progress_);
    
    // Save resume state
    resume_state_.valid = true;
    resume_state_.is_upload = false;
    resume_state_.address = address;
    resume_state_.total_size = size;
    resume_state_.transferred = 0;
    resume_state_.next_block = 0;
    
    // Transfer blocks
    uint32_t remaining = size;
    while (remaining > 0) {
        // Check cancellation
        if (cancel && cancel->is_cancelled()) {
            result.final_state = TransferState::Cancelled;
            result.error_message = "Transfer cancelled";
            update_progress(TransferState::Cancelled, result.error_message);
            return result;
        }
        
        uint32_t chunk = std::min(block_size, remaining);
        bool block_ok = false;
        
        for (uint32_t retry = 0; retry <= config.max_retries && !block_ok; ++retry) {
            if (retry > 0) {
                progress_.retry_count = retry;
                progress_.total_retries++;
                std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
            }
            
            block_ok = transfer_block({}, false);  // Empty data for download
        }
        
        if (!block_ok) {
            result.final_state = TransferState::Failed;
            result.error_message = "Block transfer failed after retries";
            result.bytes_transferred = progress_.transferred_bytes;
            update_progress(TransferState::Failed, result.error_message);
            return result;
        }
        
        remaining -= chunk;
        progress_.transferred_bytes += chunk;
        progress_.current_block++;
        progress_.retry_count = 0;
        resume_state_.transferred = progress_.transferred_bytes;
        resume_state_.next_block = progress_.current_block;
        
        if (progress_cb) progress_cb(progress_);
    }
    
    // Complete transfer
    update_progress(TransferState::Completing, "Completing transfer...");
    if (progress_cb) progress_cb(progress_);
    
    if (!request_transfer_exit()) {
        result.final_state = TransferState::Failed;
        result.error_message = "RequestTransferExit failed";
        update_progress(TransferState::Failed, result.error_message);
        return result;
    }
    
    // Success
    result.ok = true;
    result.final_state = TransferState::Completed;
    result.bytes_transferred = size;
    result.blocks_transferred = progress_.total_blocks;
    result.total_retries = progress_.total_retries;
    result.duration = progress_.elapsed();
    
    if (config.use_crc) {
        result.crc32 = calculate_crc32(download_buffer_);
    }
    
    resume_state_.valid = false;
    update_progress(TransferState::Completed, "Download complete");
    if (progress_cb) progress_cb(progress_);
    
    return result;
}

TransferResult BlockTransferManager::upload(uint32_t address, const std::vector<uint8_t>& data,
                                            const TransferConfig& config,
                                            ProgressCallback progress_cb,
                                            CancellationToken* cancel) {
    TransferResult result;
    upload_data_ = data;
    
    // Initialize progress
    progress_ = TransferProgress();
    progress_.total_bytes = data.size();
    progress_.start_time = std::chrono::steady_clock::now();
    update_progress(TransferState::Preparing, "Requesting upload...");
    
    if (progress_cb) progress_cb(progress_);
    
    // Request upload
    uint32_t max_block_size = 0;
    if (!request_upload(address, static_cast<uint32_t>(data.size()), max_block_size)) {
        result.error_message = "RequestUpload failed";
        result.final_state = TransferState::Failed;
        update_progress(TransferState::Failed, result.error_message);
        return result;
    }
    
    // Account for block sequence counter byte
    uint32_t effective_block = max_block_size > 2 ? max_block_size - 2 : max_block_size;
    uint32_t block_size = std::min(effective_block, config.block_size);
    progress_.total_blocks = (data.size() + block_size - 1) / block_size;
    
    update_progress(TransferState::Transferring, "Uploading...");
    if (progress_cb) progress_cb(progress_);
    
    // Save resume state
    resume_state_.valid = true;
    resume_state_.is_upload = true;
    resume_state_.address = address;
    resume_state_.total_size = data.size();
    resume_state_.transferred = 0;
    resume_state_.next_block = 0;
    
    // Transfer blocks
    size_t offset = 0;
    while (offset < data.size()) {
        // Check cancellation
        if (cancel && cancel->is_cancelled()) {
            result.final_state = TransferState::Cancelled;
            result.error_message = "Transfer cancelled";
            update_progress(TransferState::Cancelled, result.error_message);
            return result;
        }
        
        size_t chunk = std::min(static_cast<size_t>(block_size), data.size() - offset);
        std::vector<uint8_t> block_data(data.begin() + offset, data.begin() + offset + chunk);
        
        bool block_ok = false;
        for (uint32_t retry = 0; retry <= config.max_retries && !block_ok; ++retry) {
            if (retry > 0) {
                progress_.retry_count = retry;
                progress_.total_retries++;
                std::this_thread::sleep_for(std::chrono::milliseconds(config.retry_delay_ms));
            }
            
            block_ok = transfer_block(block_data, true);
        }
        
        if (!block_ok) {
            result.final_state = TransferState::Failed;
            result.error_message = "Block transfer failed after retries";
            result.bytes_transferred = progress_.transferred_bytes;
            update_progress(TransferState::Failed, result.error_message);
            return result;
        }
        
        offset += chunk;
        progress_.transferred_bytes = offset;
        progress_.current_block++;
        progress_.retry_count = 0;
        resume_state_.transferred = offset;
        resume_state_.next_block = progress_.current_block;
        
        if (progress_cb) progress_cb(progress_);
    }
    
    // Complete transfer
    update_progress(TransferState::Completing, "Completing transfer...");
    if (progress_cb) progress_cb(progress_);
    
    if (!request_transfer_exit()) {
        result.final_state = TransferState::Failed;
        result.error_message = "RequestTransferExit failed";
        update_progress(TransferState::Failed, result.error_message);
        return result;
    }
    
    // Verify if requested
    if (config.verify_blocks) {
        update_progress(TransferState::Verifying, "Verifying upload...");
        if (progress_cb) progress_cb(progress_);
        
        if (!verify_upload(address, data, config)) {
            result.final_state = TransferState::Failed;
            result.error_message = "Verification failed";
            update_progress(TransferState::Failed, result.error_message);
            return result;
        }
    }
    
    // Success
    result.ok = true;
    result.final_state = TransferState::Completed;
    result.bytes_transferred = data.size();
    result.blocks_transferred = progress_.total_blocks;
    result.total_retries = progress_.total_retries;
    result.duration = progress_.elapsed();
    
    if (config.use_crc) {
        result.crc32 = calculate_crc32(data);
    }
    
    resume_state_.valid = false;
    update_progress(TransferState::Completed, "Upload complete");
    if (progress_cb) progress_cb(progress_);
    
    return result;
}

TransferResult BlockTransferManager::resume(const TransferConfig& config,
                                            ProgressCallback progress_cb,
                                            CancellationToken* cancel) {
    TransferResult result;
    
    if (!resume_state_.valid) {
        result.error_message = "No transfer to resume";
        result.final_state = TransferState::Failed;
        return result;
    }
    
    // For now, restart from beginning
    // A full implementation would need ECU support for partial transfers
    if (resume_state_.is_upload) {
        return upload(resume_state_.address, upload_data_, config, progress_cb, cancel);
    } else {
        return download(resume_state_.address, 
                       static_cast<uint32_t>(resume_state_.total_size),
                       config, progress_cb, cancel);
    }
}

bool BlockTransferManager::verify_upload(uint32_t address, const std::vector<uint8_t>& expected,
                                         const TransferConfig& config) {
    auto result = download(address, static_cast<uint32_t>(expected.size()), config);
    if (!result.ok) {
        return false;
    }
    return download_buffer_ == expected;
}

std::optional<uint32_t> BlockTransferManager::calculate_remote_crc(uint32_t address, uint32_t size) {
    auto result = download(address, size, TransferConfig::fast());
    if (!result.ok) {
        return std::nullopt;
    }
    return calculate_crc32(download_buffer_);
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string format_transfer_result(const TransferResult& result) {
    std::ostringstream ss;
    
    if (result.ok) {
        ss << "Transfer completed successfully\n";
        ss << "  Bytes: " << result.bytes_transferred << "\n";
        ss << "  Blocks: " << result.blocks_transferred << "\n";
        ss << "  Duration: " << format_duration(result.duration) << "\n";
        ss << "  Rate: " << format_transfer_rate(result.bytes_per_second()) << "\n";
        if (result.crc32) {
            ss << "  CRC32: 0x" << std::hex << std::uppercase 
               << std::setw(8) << std::setfill('0') << *result.crc32 << "\n";
        }
        if (result.total_retries > 0) {
            ss << "  Retries: " << std::dec << result.total_retries << "\n";
        }
    } else {
        ss << "Transfer failed: " << result.error_message << "\n";
        ss << "  Bytes transferred: " << result.bytes_transferred << "\n";
    }
    
    return ss.str();
}

std::string format_progress(const TransferProgress& progress) {
    std::ostringstream ss;
    
    ss << std::fixed << std::setprecision(1) << progress.percentage() << "% ";
    ss << "(" << progress.transferred_bytes << "/" << progress.total_bytes << " bytes) ";
    ss << "Block " << progress.current_block << "/" << progress.total_blocks << " ";
    ss << format_transfer_rate(progress.bytes_per_second());
    
    auto remaining = progress.estimated_remaining();
    if (remaining.count() > 0) {
        ss << " ETA: " << format_duration(remaining);
    }
    
    return ss.str();
}

std::string format_transfer_rate(double bytes_per_second) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    
    if (bytes_per_second >= 1024 * 1024) {
        ss << (bytes_per_second / (1024 * 1024)) << " MB/s";
    } else if (bytes_per_second >= 1024) {
        ss << (bytes_per_second / 1024) << " KB/s";
    } else {
        ss << bytes_per_second << " B/s";
    }
    
    return ss.str();
}

std::string format_duration(std::chrono::milliseconds duration) {
    std::ostringstream ss;
    
    auto ms = duration.count();
    if (ms >= 60000) {
        ss << (ms / 60000) << "m " << ((ms % 60000) / 1000) << "s";
    } else if (ms >= 1000) {
        ss << std::fixed << std::setprecision(1) << (ms / 1000.0) << "s";
    } else {
        ss << ms << "ms";
    }
    
    return ss.str();
}

} // namespace block
} // namespace uds
