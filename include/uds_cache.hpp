#pragma once
/**
 * @file uds_cache.hpp
 * @brief Caching layer for frequently accessed DIDs and data
 * 
 * This module provides intelligent caching to reduce bus traffic and
 * improve response times for frequently accessed diagnostic data.
 * 
 * Features:
 * - Time-based expiration
 * - LRU eviction policy
 * - Cache statistics
 * - Selective invalidation
 * - Thread-safe operations
 */

#include "uds.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <optional>
#include <chrono>
#include <mutex>
#include <list>
#include <functional>

namespace uds {
namespace cache {

// ============================================================================
// Cache Configuration
// ============================================================================

/**
 * @brief Cache entry expiration policy
 */
enum class ExpirationPolicy {
    Never,              ///< Never expire (manual invalidation only)
    TimeToLive,         ///< Expire after fixed time from creation
    TimeToIdle,         ///< Expire after fixed time since last access
    Sliding             ///< Reset TTL on each access
};

/**
 * @brief Cache configuration
 */
struct CacheConfig {
    size_t max_entries = 1000;                          ///< Maximum cache entries
    size_t max_memory_bytes = 1024 * 1024;              ///< Maximum memory usage
    std::chrono::milliseconds default_ttl{60000};       ///< Default TTL (60 seconds)
    ExpirationPolicy default_policy = ExpirationPolicy::TimeToLive;
    bool enable_statistics = true;                      ///< Track hit/miss stats
    
    CacheConfig() = default;
    
    /**
     * @brief Create config for volatile data (short TTL)
     */
    static CacheConfig volatile_data() {
        CacheConfig cfg;
        cfg.default_ttl = std::chrono::milliseconds(1000);  // 1 second
        cfg.default_policy = ExpirationPolicy::TimeToLive;
        return cfg;
    }
    
    /**
     * @brief Create config for static data (long TTL)
     */
    static CacheConfig static_data() {
        CacheConfig cfg;
        cfg.default_ttl = std::chrono::milliseconds(3600000);  // 1 hour
        cfg.default_policy = ExpirationPolicy::TimeToIdle;
        return cfg;
    }
    
    /**
     * @brief Create config for session data (never expires)
     */
    static CacheConfig session_data() {
        CacheConfig cfg;
        cfg.default_policy = ExpirationPolicy::Never;
        return cfg;
    }
};

// ============================================================================
// Cache Entry
// ============================================================================

/**
 * @brief Individual cache entry
 */
struct CacheEntry {
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point last_accessed;
    std::chrono::milliseconds ttl;
    ExpirationPolicy policy;
    uint32_t hit_count = 0;
    size_t memory_size = 0;
    
    CacheEntry() = default;
    CacheEntry(const std::vector<uint8_t>& d, std::chrono::milliseconds t, ExpirationPolicy p)
        : data(d), ttl(t), policy(p), memory_size(d.size() + sizeof(CacheEntry)) {
        created = std::chrono::steady_clock::now();
        last_accessed = created;
    }
    
    /**
     * @brief Check if entry has expired
     */
    bool is_expired() const {
        if (policy == ExpirationPolicy::Never) {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto reference = (policy == ExpirationPolicy::TimeToIdle || 
                         policy == ExpirationPolicy::Sliding) 
                        ? last_accessed : created;
        
        return (now - reference) > ttl;
    }
    
    /**
     * @brief Update access time
     */
    void touch() {
        last_accessed = std::chrono::steady_clock::now();
        hit_count++;
    }
};

// ============================================================================
// Cache Statistics
// ============================================================================

/**
 * @brief Cache performance statistics
 */
struct CacheStats {
    uint64_t hits = 0;              ///< Cache hits
    uint64_t misses = 0;            ///< Cache misses
    uint64_t evictions = 0;         ///< Entries evicted
    uint64_t expirations = 0;       ///< Entries expired
    uint64_t invalidations = 0;     ///< Manual invalidations
    size_t current_entries = 0;     ///< Current entry count
    size_t current_memory = 0;      ///< Current memory usage
    size_t peak_entries = 0;        ///< Peak entry count
    size_t peak_memory = 0;         ///< Peak memory usage
    
    /**
     * @brief Get hit rate (0.0 - 1.0)
     */
    double hit_rate() const {
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }
    
    /**
     * @brief Reset statistics
     */
    void reset() {
        hits = misses = evictions = expirations = invalidations = 0;
    }
};

// ============================================================================
// DID Cache
// ============================================================================

/**
 * @brief Cache for DID (Data Identifier) values
 */
class DIDCache {
public:
    explicit DIDCache(const CacheConfig& config = CacheConfig());
    
    // ========================================================================
    // Basic Operations
    // ========================================================================
    
    /**
     * @brief Get cached value for DID
     * @param did Data identifier
     * @return Cached data or nullopt if not cached/expired
     */
    std::optional<std::vector<uint8_t>> get(uint16_t did);
    
    /**
     * @brief Store value in cache
     * @param did Data identifier
     * @param data Data to cache
     * @param ttl Optional TTL override
     * @param policy Optional policy override
     */
    void put(uint16_t did, const std::vector<uint8_t>& data,
             std::optional<std::chrono::milliseconds> ttl = std::nullopt,
             std::optional<ExpirationPolicy> policy = std::nullopt);
    
    /**
     * @brief Check if DID is cached (and not expired)
     */
    bool contains(uint16_t did);
    
    /**
     * @brief Invalidate specific DID
     */
    void invalidate(uint16_t did);
    
    /**
     * @brief Invalidate all DIDs in a range
     */
    void invalidate_range(uint16_t start_did, uint16_t end_did);
    
    /**
     * @brief Clear entire cache
     */
    void clear();
    
    // ========================================================================
    // Batch Operations
    // ========================================================================
    
    /**
     * @brief Get multiple DIDs at once
     * @param dids List of DIDs to get
     * @return Map of DID -> data for cached entries
     */
    std::map<uint16_t, std::vector<uint8_t>> get_multiple(const std::vector<uint16_t>& dids);
    
    /**
     * @brief Store multiple DIDs at once
     */
    void put_multiple(const std::map<uint16_t, std::vector<uint8_t>>& entries);
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * @brief Set TTL for specific DID
     */
    void set_did_ttl(uint16_t did, std::chrono::milliseconds ttl);
    
    /**
     * @brief Set policy for specific DID
     */
    void set_did_policy(uint16_t did, ExpirationPolicy policy);
    
    /**
     * @brief Mark DID as non-cacheable
     */
    void set_non_cacheable(uint16_t did);
    
    /**
     * @brief Check if DID is cacheable
     */
    bool is_cacheable(uint16_t did) const;
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /**
     * @brief Get cache statistics
     */
    CacheStats stats() const;
    
    /**
     * @brief Reset statistics
     */
    void reset_stats();
    
    // ========================================================================
    // Maintenance
    // ========================================================================
    
    /**
     * @brief Remove expired entries
     * @return Number of entries removed
     */
    size_t cleanup_expired();
    
    /**
     * @brief Get current entry count
     */
    size_t size() const;
    
    /**
     * @brief Get current memory usage
     */
    size_t memory_usage() const;

private:
    CacheConfig config_;
    mutable std::mutex mutex_;
    
    // LRU list (front = most recently used)
    std::list<uint16_t> lru_list_;
    std::unordered_map<uint16_t, std::list<uint16_t>::iterator> lru_map_;
    
    // Cache storage
    std::unordered_map<uint16_t, CacheEntry> entries_;
    
    // Per-DID configuration
    std::unordered_map<uint16_t, std::chrono::milliseconds> did_ttls_;
    std::unordered_map<uint16_t, ExpirationPolicy> did_policies_;
    std::set<uint16_t> non_cacheable_;
    
    // Statistics
    mutable CacheStats stats_;
    
    // Internal helpers
    void evict_if_needed();
    void update_lru(uint16_t did);
    void remove_entry(uint16_t did);
};

// ============================================================================
// Cached UDS Client
// ============================================================================

/**
 * @brief UDS client wrapper with automatic caching
 */
class CachedClient {
public:
    CachedClient(Client& client, const CacheConfig& config = CacheConfig());
    
    /**
     * @brief Read DID with caching
     * @param did Data identifier
     * @param force_refresh Bypass cache and read from ECU
     * @return Read result
     */
    PositiveOrNegative read_did(uint16_t did, bool force_refresh = false);
    
    /**
     * @brief Read multiple DIDs with caching
     * @param dids List of DIDs
     * @param force_refresh Bypass cache
     * @return Map of DID -> data
     */
    std::map<uint16_t, std::vector<uint8_t>> read_dids(
        const std::vector<uint16_t>& dids, bool force_refresh = false);
    
    /**
     * @brief Write DID (invalidates cache)
     */
    PositiveOrNegative write_did(uint16_t did, const std::vector<uint8_t>& data);
    
    /**
     * @brief Get underlying client
     */
    Client& client() { return client_; }
    
    /**
     * @brief Get cache
     */
    DIDCache& cache() { return cache_; }
    
    /**
     * @brief Prefetch DIDs into cache
     */
    void prefetch(const std::vector<uint16_t>& dids);
    
    /**
     * @brief Invalidate cache on session change
     */
    void on_session_change();

private:
    Client& client_;
    DIDCache cache_;
};

// ============================================================================
// Common DID Categories
// ============================================================================

namespace did_categories {

/**
 * @brief DIDs that should never be cached (real-time data)
 */
inline std::vector<uint16_t> volatile_dids() {
    return {
        0xF40C,  // Engine RPM
        0xF40D,  // Vehicle Speed
        0xF405,  // Coolant Temperature
        0xF410,  // MAF Sensor
        0xF411,  // Throttle Position
    };
}

/**
 * @brief DIDs that can be cached for a long time (static data)
 */
inline std::vector<uint16_t> static_dids() {
    return {
        0xF190,  // VIN
        0xF18A,  // System Supplier Identifier
        0xF18B,  // ECU Manufacturing Date
        0xF18C,  // ECU Serial Number
        0xF191,  // Vehicle Manufacturer ECU Hardware Number
        0xF193,  // System Supplier ECU Hardware Version Number
        0xF195,  // System Supplier ECU Software Version Number
    };
}

/**
 * @brief DIDs that should be cached per-session
 */
inline std::vector<uint16_t> session_dids() {
    return {
        0xF186,  // Active Diagnostic Session
        0xF187,  // Vehicle Manufacturer Spare Part Number
    };
}

} // namespace did_categories

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Format cache statistics for display
 */
std::string format_cache_stats(const CacheStats& stats);

} // namespace cache
} // namespace uds
