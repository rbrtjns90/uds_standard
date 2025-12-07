/**
 * @file thread_safety_test.cpp
 * @brief Thread safety and concurrency tests
 * 
 * Tests cover:
 * - Concurrent data structure access
 * - Thread-safe counter operations
 * - Parallel encoding/decoding
 * - Cancellation token thread safety
 * - Concurrent CRC calculations
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <future>
#include <chrono>
#include <numeric>
#include "uds.hpp"
#include "uds_block.hpp"
#include "uds_security.hpp"
#include "uds_dtc.hpp"
#include "uds_scaling.hpp"

using namespace uds;

// ============================================================================
// Atomic Counter Tests
// ============================================================================

TEST(ThreadSafetyTest, AtomicCounterIncrement) {
  std::atomic<int> counter{0};
  constexpr int num_threads = 10;
  constexpr int increments_per_thread = 10000;
  
  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&counter]() {
      for (int i = 0; i < increments_per_thread; ++i) {
        counter.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  EXPECT_EQ(counter.load(), num_threads * increments_per_thread);
}

// ============================================================================
// Cancellation Token Thread Safety Tests
// ============================================================================

TEST(ThreadSafetyTest, CancellationTokenConcurrentAccess) {
  using namespace block;
  
  CancellationToken token;
  std::atomic<int> checks_before_cancel{0};
  std::atomic<int> checks_after_cancel{0};
  std::atomic<bool> cancel_done{false};
  
  constexpr int num_readers = 5;
  std::vector<std::thread> readers;
  readers.reserve(num_readers);
  
  // Start reader threads
  for (int i = 0; i < num_readers; ++i) {
    readers.emplace_back([&]() {
      while (!cancel_done.load()) {
        bool cancelled = token.is_cancelled();
        if (cancelled) {
          checks_after_cancel.fetch_add(1);
        } else {
          checks_before_cancel.fetch_add(1);
        }
        std::this_thread::yield();
      }
    });
  }
  
  // Let readers run for a bit
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  
  // Cancel the token
  token.cancel();
  
  // Let readers see the cancellation
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  
  cancel_done.store(true);
  
  for (auto& t : readers) {
    t.join();
  }
  
  EXPECT_GT(checks_before_cancel.load(), 0);
  EXPECT_GT(checks_after_cancel.load(), 0);
  EXPECT_TRUE(token.is_cancelled());
}

TEST(ThreadSafetyTest, CancellationTokenResetWhileReading) {
  using namespace block;
  
  CancellationToken token;
  std::atomic<bool> done{false};
  std::atomic<int> read_count{0};
  
  // Reader thread
  std::thread reader([&]() {
    while (!done.load()) {
      token.is_cancelled();
      read_count.fetch_add(1);
      std::this_thread::yield();
    }
  });
  
  // Writer thread - toggle cancel/reset
  std::thread writer([&]() {
    for (int i = 0; i < 100; ++i) {
      token.cancel();
      std::this_thread::yield();
      token.reset();
      std::this_thread::yield();
    }
    done.store(true);
  });
  
  reader.join();
  writer.join();
  
  EXPECT_GT(read_count.load(), 0);
}

// ============================================================================
// Concurrent Encoding Tests
// ============================================================================

TEST(ThreadSafetyTest, ConcurrentBigEndianEncoding) {
  constexpr int num_threads = 8;
  constexpr int operations_per_thread = 10000;
  
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([t, &success_count]() {
      for (int i = 0; i < operations_per_thread; ++i) {
        std::vector<uint8_t> v;
        v.reserve(6);
        
        uint16_t val16 = static_cast<uint16_t>((t * 1000 + i) & 0xFFFF);
        uint32_t val32 = static_cast<uint32_t>(t * 1000000 + i);
        
        codec::be16(v, val16);
        codec::be32(v, val32);
        
        // Verify encoding
        uint16_t decoded16 = (static_cast<uint16_t>(v[0]) << 8) | v[1];
        uint32_t decoded32 = (static_cast<uint32_t>(v[2]) << 24) |
                             (static_cast<uint32_t>(v[3]) << 16) |
                             (static_cast<uint32_t>(v[4]) << 8) |
                             static_cast<uint32_t>(v[5]);
        
        if (decoded16 == val16 && decoded32 == val32) {
          success_count.fetch_add(1);
        }
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  EXPECT_EQ(success_count.load(), num_threads * operations_per_thread);
}

// ============================================================================
// Concurrent CRC Calculation Tests
// ============================================================================

TEST(ThreadSafetyTest, ConcurrentCRCCalculation) {
  using namespace block;
  
  constexpr int num_threads = 4;
  std::vector<std::thread> threads;
  std::vector<uint32_t> results(num_threads);
  
  // Same data for all threads
  std::vector<uint8_t> data(1000);
  std::iota(data.begin(), data.end(), 0);
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&data, &results, t]() {
      // Each thread calculates CRC multiple times
      for (int i = 0; i < 100; ++i) {
        results[t] = calculate_crc32(data);
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  // All threads should get the same result
  for (int t = 1; t < num_threads; ++t) {
    EXPECT_EQ(results[t], results[0]) << "CRC mismatch between threads";
  }
}

TEST(ThreadSafetyTest, ConcurrentCRCDifferentData) {
  using namespace block;
  
  constexpr int num_threads = 4;
  std::vector<std::thread> threads;
  std::vector<uint32_t> results(num_threads);
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&results, t]() {
      // Each thread has different data
      std::vector<uint8_t> data(1000, static_cast<uint8_t>(t));
      results[t] = calculate_crc32(data);
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  // Different data should produce different CRCs
  for (int t = 1; t < num_threads; ++t) {
    EXPECT_NE(results[t], results[0]) << "Different data should have different CRC";
  }
}

// ============================================================================
// Concurrent Security Algorithm Tests
// ============================================================================

TEST(ThreadSafetyTest, ConcurrentKeyCalculation) {
  using namespace security;
  
  constexpr int num_threads = 4;
  std::vector<std::thread> threads;
  std::vector<std::vector<uint8_t>> results(num_threads);
  
  std::vector<uint8_t> seed = {0x12, 0x34, 0x56, 0x78};
  uint8_t level = Level::Basic;
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&seed, level, &results, t]() {
      XORAlgorithm algo;
      for (int i = 0; i < 100; ++i) {
        results[t] = algo.calculate_key(seed, level, {});
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  // All threads should get the same result
  for (int t = 1; t < num_threads; ++t) {
    EXPECT_EQ(results[t], results[0]) << "Key calculation mismatch between threads";
  }
}

// ============================================================================
// Concurrent DTC Parsing Tests
// ============================================================================

TEST(ThreadSafetyTest, ConcurrentDTCParsing) {
  using namespace dtc;
  
  constexpr int num_threads = 4;
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([t, &success_count]() {
      for (int i = 0; i < 1000; ++i) {
        uint8_t bytes[3] = {
          static_cast<uint8_t>(t),
          static_cast<uint8_t>(i >> 8),
          static_cast<uint8_t>(i & 0xFF)
        };
        
        uint32_t dtc = parse_dtc_code(bytes);
        
        // Verify parsing
        uint32_t expected = (static_cast<uint32_t>(t) << 16) |
                           (static_cast<uint32_t>(i >> 8) << 8) |
                           static_cast<uint32_t>(i & 0xFF);
        
        if (dtc == expected) {
          success_count.fetch_add(1);
        }
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  EXPECT_EQ(success_count.load(), num_threads * 1000);
}

// ============================================================================
// Concurrent Scaling Tests
// ============================================================================

TEST(ThreadSafetyTest, ConcurrentByteConversion) {
  using namespace scaling;
  
  constexpr int num_threads = 4;
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([t, &success_count]() {
      for (int i = 0; i < 10000; ++i) {
        uint32_t value = static_cast<uint32_t>(t * 1000000 + i);
        
        std::vector<uint8_t> bytes = {
          static_cast<uint8_t>((value >> 24) & 0xFF),
          static_cast<uint8_t>((value >> 16) & 0xFF),
          static_cast<uint8_t>((value >> 8) & 0xFF),
          static_cast<uint8_t>(value & 0xFF)
        };
        
        uint64_t decoded = bytes_to_uint(bytes);
        
        if (decoded == value) {
          success_count.fetch_add(1);
        }
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  EXPECT_EQ(success_count.load(), num_threads * 10000);
}

// ============================================================================
// Parallel Task Tests
// ============================================================================

TEST(ThreadSafetyTest, ParallelFutures) {
  constexpr int num_tasks = 10;
  std::vector<std::future<uint32_t>> futures;
  futures.reserve(num_tasks);
  
  for (int i = 0; i < num_tasks; ++i) {
    futures.push_back(std::async(std::launch::async, [i]() {
      // Simulate some work
      std::vector<uint8_t> data(1000, static_cast<uint8_t>(i));
      return block::calculate_crc32(data);
    }));
  }
  
  std::vector<uint32_t> results;
  results.reserve(num_tasks);
  
  for (auto& f : futures) {
    results.push_back(f.get());
  }
  
  EXPECT_EQ(results.size(), static_cast<size_t>(num_tasks));
  
  // Each task had different data, so CRCs should differ
  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_NE(results[i], results[0]);
  }
}

// ============================================================================
// Mutex-Protected Shared State Tests
// ============================================================================

TEST(ThreadSafetyTest, MutexProtectedVector) {
  std::vector<uint8_t> shared_data;
  std::mutex mtx;
  
  constexpr int num_threads = 4;
  constexpr int items_per_thread = 1000;
  
  std::vector<std::thread> threads;
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&shared_data, &mtx, t]() {
      for (int i = 0; i < items_per_thread; ++i) {
        std::lock_guard<std::mutex> lock(mtx);
        shared_data.push_back(static_cast<uint8_t>(t));
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  EXPECT_EQ(shared_data.size(), static_cast<size_t>(num_threads * items_per_thread));
}

// ============================================================================
// Race Condition Detection Tests
// ============================================================================

TEST(ThreadSafetyTest, NoRaceInVectorReserve) {
  // Each thread works on its own vector - no race
  constexpr int num_threads = 4;
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&success_count]() {
      for (int i = 0; i < 1000; ++i) {
        std::vector<uint8_t> local_vec;
        local_vec.reserve(100);
        
        for (int j = 0; j < 100; ++j) {
          local_vec.push_back(static_cast<uint8_t>(j));
        }
        
        if (local_vec.size() == 100) {
          success_count.fetch_add(1);
        }
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  EXPECT_EQ(success_count.load(), num_threads * 1000);
}

// ============================================================================
// Simulated Concurrent Client Operations
// ============================================================================

TEST(ThreadSafetyTest, SimulatedConcurrentPayloadBuilding) {
  // Simulate multiple threads building UDS payloads concurrently
  constexpr int num_threads = 4;
  std::vector<std::thread> threads;
  std::atomic<int> payloads_built{0};
  
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([t, &payloads_built]() {
      for (int i = 0; i < 1000; ++i) {
        // Build a ReadDataByIdentifier payload
        std::vector<uint8_t> payload;
        payload.reserve(3);
        payload.push_back(0x22);  // RDBI SID
        
        DID did = static_cast<DID>(0xF190 + (t * 100) + (i % 100));
        codec::be16(payload, did);
        
        if (payload.size() == 3 && payload[0] == 0x22) {
          payloads_built.fetch_add(1);
        }
      }
    });
  }
  
  for (auto& t : threads) {
    t.join();
  }
  
  EXPECT_EQ(payloads_built.load(), num_threads * 1000);
}

// ============================================================================
// Thread Pool Simulation
// ============================================================================

TEST(ThreadSafetyTest, SimulatedThreadPool) {
  constexpr int num_workers = 4;
  constexpr int num_tasks = 100;
  
  std::atomic<int> tasks_completed{0};
  std::atomic<int> next_task{0};
  std::atomic<bool> done{false};
  
  std::vector<std::thread> workers;
  
  for (int w = 0; w < num_workers; ++w) {
    workers.emplace_back([&]() {
      while (!done.load()) {
        int task_id = next_task.fetch_add(1);
        
        if (task_id >= num_tasks) {
          break;
        }
        
        // Simulate task work
        std::vector<uint8_t> data(100, static_cast<uint8_t>(task_id));
        uint32_t crc = block::calculate_crc32(data);
        (void)crc;
        
        tasks_completed.fetch_add(1);
      }
    });
  }
  
  // Wait for all tasks
  while (tasks_completed.load() < num_tasks) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  
  done.store(true);
  
  for (auto& w : workers) {
    w.join();
  }
  
  EXPECT_EQ(tasks_completed.load(), num_tasks);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
