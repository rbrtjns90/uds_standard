/**
 * @file cache_async_test.cpp
 * @brief Google Test suite for caching and async operations
 */

#include <gtest/gtest.h>
#include "uds_cache.hpp"
#include "uds_async.hpp"

using namespace uds;

// ============================================================================
// Cache Expiration Policy Tests
// ============================================================================

TEST(CacheTest, ExpirationPolicyValues) {
  using namespace cache;
  
  ExpirationPolicy never = ExpirationPolicy::Never;
  ExpirationPolicy ttl = ExpirationPolicy::TimeToLive;
  ExpirationPolicy tti = ExpirationPolicy::TimeToIdle;
  ExpirationPolicy sliding = ExpirationPolicy::Sliding;
  
  EXPECT_NE(static_cast<int>(never), static_cast<int>(ttl));
  EXPECT_NE(static_cast<int>(ttl), static_cast<int>(tti));
  EXPECT_NE(static_cast<int>(tti), static_cast<int>(sliding));
}

// ============================================================================
// Cache Config Tests
// ============================================================================

TEST(CacheTest, ConfigDefaults) {
  using namespace cache;
  
  CacheConfig config;
  
  EXPECT_EQ(config.max_entries, 1000u);
  EXPECT_EQ(config.max_memory_bytes, 1024u * 1024u);
  EXPECT_EQ(config.default_ttl.count(), 60000);
  EXPECT_EQ(config.default_policy, ExpirationPolicy::TimeToLive);
  EXPECT_TRUE(config.enable_statistics);
}

TEST(CacheTest, ConfigVolatileData) {
  using namespace cache;
  
  auto config = CacheConfig::volatile_data();
  
  EXPECT_EQ(config.default_ttl.count(), 1000);  // 1 second
  EXPECT_EQ(config.default_policy, ExpirationPolicy::TimeToLive);
}

TEST(CacheTest, ConfigStaticData) {
  using namespace cache;
  
  auto config = CacheConfig::static_data();
  
  EXPECT_EQ(config.default_ttl.count(), 3600000);  // 1 hour
  EXPECT_EQ(config.default_policy, ExpirationPolicy::TimeToIdle);
}

TEST(CacheTest, ConfigSessionData) {
  using namespace cache;
  
  auto config = CacheConfig::session_data();
  
  EXPECT_EQ(config.default_policy, ExpirationPolicy::Never);
}

// ============================================================================
// Cache Entry Tests
// ============================================================================

TEST(CacheTest, EntryDefaults) {
  using namespace cache;
  
  CacheEntry entry;
  
  EXPECT_TRUE(entry.data.empty());
  EXPECT_EQ(entry.hit_count, 0u);
  EXPECT_EQ(entry.memory_size, 0u);
}

TEST(CacheTest, EntryConstruction) {
  using namespace cache;
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03};
  auto ttl = std::chrono::milliseconds(5000);
  
  CacheEntry entry(data, ttl, ExpirationPolicy::TimeToLive);
  
  EXPECT_EQ(entry.data, data);
  EXPECT_EQ(entry.ttl.count(), 5000);
  EXPECT_EQ(entry.policy, ExpirationPolicy::TimeToLive);
  EXPECT_EQ(entry.hit_count, 0u);
}

TEST(CacheTest, EntryNeverExpires) {
  using namespace cache;
  
  std::vector<uint8_t> data = {0x01};
  CacheEntry entry(data, std::chrono::milliseconds(0), ExpirationPolicy::Never);
  
  EXPECT_FALSE(entry.is_expired());
}

TEST(CacheTest, EntryTouch) {
  using namespace cache;
  
  std::vector<uint8_t> data = {0x01};
  CacheEntry entry(data, std::chrono::milliseconds(1000), ExpirationPolicy::TimeToLive);
  
  EXPECT_EQ(entry.hit_count, 0u);
  
  entry.touch();
  EXPECT_EQ(entry.hit_count, 1u);
  
  entry.touch();
  EXPECT_EQ(entry.hit_count, 2u);
}

// ============================================================================
// Cache Statistics Tests
// ============================================================================

TEST(CacheTest, StatsDefaults) {
  using namespace cache;
  
  CacheStats stats;
  
  EXPECT_EQ(stats.hits, 0u);
  EXPECT_EQ(stats.misses, 0u);
  EXPECT_EQ(stats.evictions, 0u);
  EXPECT_EQ(stats.expirations, 0u);
  EXPECT_EQ(stats.invalidations, 0u);
  EXPECT_EQ(stats.current_entries, 0u);
  EXPECT_EQ(stats.current_memory, 0u);
}

TEST(CacheTest, StatsHitRate) {
  using namespace cache;
  
  CacheStats stats;
  stats.hits = 80;
  stats.misses = 20;
  
  EXPECT_DOUBLE_EQ(stats.hit_rate(), 0.8);
}

TEST(CacheTest, StatsHitRateZero) {
  using namespace cache;
  
  CacheStats stats;
  // No hits or misses
  
  EXPECT_DOUBLE_EQ(stats.hit_rate(), 0.0);
}

TEST(CacheTest, StatsReset) {
  using namespace cache;
  
  CacheStats stats;
  stats.hits = 100;
  stats.misses = 50;
  stats.evictions = 10;
  
  stats.reset();
  
  EXPECT_EQ(stats.hits, 0u);
  EXPECT_EQ(stats.misses, 0u);
  EXPECT_EQ(stats.evictions, 0u);
}

// ============================================================================
// DID Category Tests
// ============================================================================

TEST(CacheTest, VolatileDIDs) {
  using namespace cache::did_categories;
  
  auto dids = volatile_dids();
  
  EXPECT_FALSE(dids.empty());
  // Should include real-time data like RPM, speed
  EXPECT_NE(std::find(dids.begin(), dids.end(), 0xF40C), dids.end());  // Engine RPM
  EXPECT_NE(std::find(dids.begin(), dids.end(), 0xF40D), dids.end());  // Vehicle Speed
}

TEST(CacheTest, StaticDIDs) {
  using namespace cache::did_categories;
  
  auto dids = static_dids();
  
  EXPECT_FALSE(dids.empty());
  // Should include VIN, serial numbers
  EXPECT_NE(std::find(dids.begin(), dids.end(), 0xF190), dids.end());  // VIN
  EXPECT_NE(std::find(dids.begin(), dids.end(), 0xF18C), dids.end());  // ECU Serial
}

TEST(CacheTest, SessionDIDs) {
  using namespace cache::did_categories;
  
  auto dids = session_dids();
  
  EXPECT_FALSE(dids.empty());
  // Should include session-specific data
  EXPECT_NE(std::find(dids.begin(), dids.end(), 0xF186), dids.end());  // Active Session
}

// ============================================================================
// Async Status Tests
// ============================================================================

TEST(AsyncTest, StatusValues) {
  using namespace async;
  
  EXPECT_NE(static_cast<int>(AsyncStatus::Pending), static_cast<int>(AsyncStatus::Running));
  EXPECT_NE(static_cast<int>(AsyncStatus::Running), static_cast<int>(AsyncStatus::Completed));
  EXPECT_NE(static_cast<int>(AsyncStatus::Completed), static_cast<int>(AsyncStatus::Failed));
  EXPECT_NE(static_cast<int>(AsyncStatus::Failed), static_cast<int>(AsyncStatus::Cancelled));
  EXPECT_NE(static_cast<int>(AsyncStatus::Cancelled), static_cast<int>(AsyncStatus::TimedOut));
}

TEST(AsyncTest, StatusName) {
  using namespace async;
  
  EXPECT_STREQ(status_name(AsyncStatus::Pending), "Pending");
  EXPECT_STREQ(status_name(AsyncStatus::Running), "Running");
  EXPECT_STREQ(status_name(AsyncStatus::Completed), "Completed");
  EXPECT_STREQ(status_name(AsyncStatus::Failed), "Failed");
  EXPECT_STREQ(status_name(AsyncStatus::Cancelled), "Cancelled");
  EXPECT_STREQ(status_name(AsyncStatus::TimedOut), "TimedOut");
}

// ============================================================================
// Async Result Tests
// ============================================================================

TEST(AsyncTest, ResultDefaults) {
  using namespace async;
  
  AsyncResult<int> result;
  
  EXPECT_EQ(result.status, AsyncStatus::Pending);
  EXPECT_FALSE(result.is_ready());
  EXPECT_FALSE(result.is_success());
}

TEST(AsyncTest, ResultIsReady) {
  using namespace async;
  
  AsyncResult<int> pending;
  pending.status = AsyncStatus::Pending;
  EXPECT_FALSE(pending.is_ready());
  
  AsyncResult<int> running;
  running.status = AsyncStatus::Running;
  EXPECT_FALSE(running.is_ready());
  
  AsyncResult<int> completed;
  completed.status = AsyncStatus::Completed;
  EXPECT_TRUE(completed.is_ready());
  
  AsyncResult<int> failed;
  failed.status = AsyncStatus::Failed;
  EXPECT_TRUE(failed.is_ready());
}

TEST(AsyncTest, ResultIsSuccess) {
  using namespace async;
  
  AsyncResult<int> completed;
  completed.status = AsyncStatus::Completed;
  EXPECT_TRUE(completed.is_success());
  
  AsyncResult<int> failed;
  failed.status = AsyncStatus::Failed;
  EXPECT_FALSE(failed.is_success());
}

// ============================================================================
// Priority Tests
// ============================================================================

TEST(AsyncTest, PriorityValues) {
  using namespace async;
  
  EXPECT_LT(static_cast<int>(Priority::Low), static_cast<int>(Priority::Normal));
  EXPECT_LT(static_cast<int>(Priority::Normal), static_cast<int>(Priority::High));
  EXPECT_LT(static_cast<int>(Priority::High), static_cast<int>(Priority::Critical));
}

TEST(AsyncTest, PriorityOrdering) {
  using namespace async;
  
  EXPECT_EQ(static_cast<int>(Priority::Low), 0);
  EXPECT_EQ(static_cast<int>(Priority::Normal), 1);
  EXPECT_EQ(static_cast<int>(Priority::High), 2);
  EXPECT_EQ(static_cast<int>(Priority::Critical), 3);
}

// ============================================================================
// Task Handle Tests
// ============================================================================

TEST(AsyncTest, TaskHandleDefault) {
  using namespace async;
  
  TaskHandle handle;
  
  EXPECT_FALSE(handle.is_valid());
  EXPECT_EQ(handle.id(), 0u);
}

TEST(AsyncTest, TaskHandleValid) {
  using namespace async;
  
  TaskHandle handle(42);
  
  EXPECT_TRUE(handle.is_valid());
  EXPECT_EQ(handle.id(), 42u);
}

TEST(AsyncTest, TaskHandleEquality) {
  using namespace async;
  
  TaskHandle h1(100);
  TaskHandle h2(100);
  TaskHandle h3(200);
  
  EXPECT_EQ(h1, h2);
  EXPECT_NE(h1, h3);
}

// ============================================================================
// Timeout Utility Tests
// ============================================================================

TEST(AsyncTest, TimeoutCalculation) {
  auto timeout = std::chrono::milliseconds(5000);
  auto start = std::chrono::steady_clock::now();
  
  // Simulate some work
  auto elapsed = std::chrono::steady_clock::now() - start;
  auto remaining = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
  
  EXPECT_GT(remaining.count(), 0);
  EXPECT_LE(remaining.count(), 5000);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
