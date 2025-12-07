#include "uds_cache.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace uds {
namespace cache {

// ============================================================================
// DIDCache Implementation
// ============================================================================

DIDCache::DIDCache(const CacheConfig& config)
    : config_(config) {
}

std::optional<std::vector<uint8_t>> DIDCache::get(uint16_t did) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = entries_.find(did);
    if (it == entries_.end()) {
        if (config_.enable_statistics) {
            stats_.misses++;
        }
        return std::nullopt;
    }
    
    // Check expiration
    if (it->second.is_expired()) {
        remove_entry(did);
        if (config_.enable_statistics) {
            stats_.misses++;
            stats_.expirations++;
        }
        return std::nullopt;
    }
    
    // Update access
    it->second.touch();
    update_lru(did);
    
    if (config_.enable_statistics) {
        stats_.hits++;
    }
    
    return it->second.data;
}

void DIDCache::put(uint16_t did, const std::vector<uint8_t>& data,
                   std::optional<std::chrono::milliseconds> ttl,
                   std::optional<ExpirationPolicy> policy) {
    // Check if non-cacheable
    if (non_cacheable_.count(did)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Determine TTL and policy
    auto effective_ttl = ttl.value_or(
        did_ttls_.count(did) ? did_ttls_[did] : config_.default_ttl);
    auto effective_policy = policy.value_or(
        did_policies_.count(did) ? did_policies_[did] : config_.default_policy);
    
    // Remove existing entry if present
    if (entries_.count(did)) {
        remove_entry(did);
    }
    
    // Evict if needed
    evict_if_needed();
    
    // Create entry
    CacheEntry entry(data, effective_ttl, effective_policy);
    entries_[did] = entry;
    
    // Add to LRU
    lru_list_.push_front(did);
    lru_map_[did] = lru_list_.begin();
    
    // Update stats
    stats_.current_entries = entries_.size();
    stats_.current_memory += entry.memory_size;
    stats_.peak_entries = std::max(stats_.peak_entries, stats_.current_entries);
    stats_.peak_memory = std::max(stats_.peak_memory, stats_.current_memory);
}

bool DIDCache::contains(uint16_t did) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = entries_.find(did);
    if (it == entries_.end()) {
        return false;
    }
    
    if (it->second.is_expired()) {
        remove_entry(did);
        stats_.expirations++;
        return false;
    }
    
    return true;
}

void DIDCache::invalidate(uint16_t did) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (entries_.count(did)) {
        remove_entry(did);
        stats_.invalidations++;
    }
}

void DIDCache::invalidate_range(uint16_t start_did, uint16_t end_did) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<uint16_t> to_remove;
    for (const auto& [did, entry] : entries_) {
        if (did >= start_did && did <= end_did) {
            to_remove.push_back(did);
        }
    }
    
    for (uint16_t did : to_remove) {
        remove_entry(did);
        stats_.invalidations++;
    }
}

void DIDCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    entries_.clear();
    lru_list_.clear();
    lru_map_.clear();
    stats_.current_entries = 0;
    stats_.current_memory = 0;
}

std::map<uint16_t, std::vector<uint8_t>> DIDCache::get_multiple(const std::vector<uint16_t>& dids) {
    std::map<uint16_t, std::vector<uint8_t>> result;
    
    for (uint16_t did : dids) {
        auto data = get(did);
        if (data) {
            result[did] = *data;
        }
    }
    
    return result;
}

void DIDCache::put_multiple(const std::map<uint16_t, std::vector<uint8_t>>& entries) {
    for (const auto& [did, data] : entries) {
        put(did, data);
    }
}

void DIDCache::set_did_ttl(uint16_t did, std::chrono::milliseconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    did_ttls_[did] = ttl;
}

void DIDCache::set_did_policy(uint16_t did, ExpirationPolicy policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    did_policies_[did] = policy;
}

void DIDCache::set_non_cacheable(uint16_t did) {
    std::lock_guard<std::mutex> lock(mutex_);
    non_cacheable_.insert(did);
    
    // Remove from cache if present
    if (entries_.count(did)) {
        remove_entry(did);
    }
}

bool DIDCache::is_cacheable(uint16_t did) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return non_cacheable_.count(did) == 0;
}

CacheStats DIDCache::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void DIDCache::reset_stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.reset();
}

size_t DIDCache::cleanup_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<uint16_t> expired;
    for (const auto& [did, entry] : entries_) {
        if (entry.is_expired()) {
            expired.push_back(did);
        }
    }
    
    for (uint16_t did : expired) {
        remove_entry(did);
        stats_.expirations++;
    }
    
    return expired.size();
}

size_t DIDCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

size_t DIDCache::memory_usage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_.current_memory;
}

void DIDCache::evict_if_needed() {
    // Evict by count
    while (entries_.size() >= config_.max_entries && !lru_list_.empty()) {
        uint16_t lru_did = lru_list_.back();
        remove_entry(lru_did);
        stats_.evictions++;
    }
    
    // Evict by memory
    while (stats_.current_memory > config_.max_memory_bytes && !lru_list_.empty()) {
        uint16_t lru_did = lru_list_.back();
        remove_entry(lru_did);
        stats_.evictions++;
    }
}

void DIDCache::update_lru(uint16_t did) {
    auto it = lru_map_.find(did);
    if (it != lru_map_.end()) {
        lru_list_.erase(it->second);
        lru_list_.push_front(did);
        lru_map_[did] = lru_list_.begin();
    }
}

void DIDCache::remove_entry(uint16_t did) {
    auto entry_it = entries_.find(did);
    if (entry_it != entries_.end()) {
        stats_.current_memory -= entry_it->second.memory_size;
        entries_.erase(entry_it);
    }
    
    auto lru_it = lru_map_.find(did);
    if (lru_it != lru_map_.end()) {
        lru_list_.erase(lru_it->second);
        lru_map_.erase(lru_it);
    }
    
    stats_.current_entries = entries_.size();
}

// ============================================================================
// CachedClient Implementation
// ============================================================================

CachedClient::CachedClient(Client& client, const CacheConfig& config)
    : client_(client), cache_(config) {
    
    // Configure volatile DIDs as non-cacheable
    for (uint16_t did : did_categories::volatile_dids()) {
        cache_.set_non_cacheable(did);
    }
    
    // Configure static DIDs with long TTL
    for (uint16_t did : did_categories::static_dids()) {
        cache_.set_did_ttl(did, std::chrono::hours(24));
        cache_.set_did_policy(did, ExpirationPolicy::TimeToIdle);
    }
    
    // Configure session DIDs
    for (uint16_t did : did_categories::session_dids()) {
        cache_.set_did_policy(did, ExpirationPolicy::Never);
    }
}

PositiveOrNegative CachedClient::read_did(uint16_t did, bool force_refresh) {
    // Check cache first (unless force refresh)
    if (!force_refresh && cache_.is_cacheable(did)) {
        auto cached = cache_.get(did);
        if (cached) {
            PositiveOrNegative result;
            result.ok = true;
            result.payload = *cached;
            return result;
        }
    }
    
    // Read from ECU
    auto result = client_.read_data_by_identifier(did);
    
    // Cache successful reads
    if (result.ok && cache_.is_cacheable(did)) {
        cache_.put(did, result.payload);
    }
    
    return result;
}

std::map<uint16_t, std::vector<uint8_t>> CachedClient::read_dids(
    const std::vector<uint16_t>& dids, bool force_refresh) {
    
    std::map<uint16_t, std::vector<uint8_t>> result;
    std::vector<uint16_t> to_fetch;
    
    // Check cache for each DID
    if (!force_refresh) {
        for (uint16_t did : dids) {
            if (cache_.is_cacheable(did)) {
                auto cached = cache_.get(did);
                if (cached) {
                    result[did] = *cached;
                    continue;
                }
            }
            to_fetch.push_back(did);
        }
    } else {
        to_fetch = dids;
    }
    
    // Fetch remaining from ECU
    for (uint16_t did : to_fetch) {
        auto response = client_.read_data_by_identifier(did);
        if (response.ok) {
            result[did] = response.payload;
            if (cache_.is_cacheable(did)) {
                cache_.put(did, response.payload);
            }
        }
    }
    
    return result;
}

PositiveOrNegative CachedClient::write_did(uint16_t did, const std::vector<uint8_t>& data) {
    // Invalidate cache before write
    cache_.invalidate(did);
    
    // Perform write
    auto result = client_.write_data_by_identifier(did, data);
    
    // If successful, update cache with new value
    if (result.ok && cache_.is_cacheable(did)) {
        cache_.put(did, data);
    }
    
    return result;
}

void CachedClient::prefetch(const std::vector<uint16_t>& dids) {
    for (uint16_t did : dids) {
        if (!cache_.contains(did)) {
            read_did(did);
        }
    }
}

void CachedClient::on_session_change() {
    // Invalidate session-specific DIDs
    for (uint16_t did : did_categories::session_dids()) {
        cache_.invalidate(did);
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string format_cache_stats(const CacheStats& stats) {
    std::ostringstream ss;
    
    ss << "Cache Statistics:\n";
    ss << "  Entries: " << stats.current_entries 
       << " (peak: " << stats.peak_entries << ")\n";
    ss << "  Memory: " << (stats.current_memory / 1024) << " KB"
       << " (peak: " << (stats.peak_memory / 1024) << " KB)\n";
    ss << "  Hit Rate: " << std::fixed << std::setprecision(1) 
       << (stats.hit_rate() * 100) << "%\n";
    ss << "  Hits: " << stats.hits << "\n";
    ss << "  Misses: " << stats.misses << "\n";
    ss << "  Evictions: " << stats.evictions << "\n";
    ss << "  Expirations: " << stats.expirations << "\n";
    ss << "  Invalidations: " << stats.invalidations << "\n";
    
    return ss.str();
}

} // namespace cache
} // namespace uds
