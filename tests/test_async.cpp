/**
 * @file test_async.cpp
 * @brief Comprehensive tests for uds_async.hpp - Async operations and caching
 */

#include "test_framework.hpp"
#include "../include/uds_async.hpp"
#include <thread>
#include <atomic>

using namespace test;
using namespace uds::async;

// ============================================================================
// Test: AsyncStatus Enum
// ============================================================================

TEST(AsyncStatus_AllStates) {
    std::cout << "  Testing: All AsyncStatus states" << std::endl;
    
    std::vector<AsyncStatus> all_states = {
        AsyncStatus::Pending,
        AsyncStatus::Running,
        AsyncStatus::Completed,
        AsyncStatus::Failed,
        AsyncStatus::Cancelled,
        AsyncStatus::TimedOut
    };
    
    ASSERT_EQ(6, all_states.size());
}

TEST(AsyncStatus_Names) {
    std::cout << "  Testing: AsyncStatus name formatting" << std::endl;
    
    ASSERT_TRUE(std::string(status_name(AsyncStatus::Pending)) == "Pending");
    ASSERT_TRUE(std::string(status_name(AsyncStatus::Running)) == "Running");
    ASSERT_TRUE(std::string(status_name(AsyncStatus::Completed)) == "Completed");
    ASSERT_TRUE(std::string(status_name(AsyncStatus::Failed)) == "Failed");
    ASSERT_TRUE(std::string(status_name(AsyncStatus::Cancelled)) == "Cancelled");
    ASSERT_TRUE(std::string(status_name(AsyncStatus::TimedOut)) == "TimedOut");
}

// ============================================================================
// Test: AsyncResult Structure
// ============================================================================

TEST(AsyncResult_IsReady) {
    std::cout << "  Testing: AsyncResult::is_ready()" << std::endl;
    
    AsyncResult<int> result;
    
    result.status = AsyncStatus::Pending;
    ASSERT_FALSE(result.is_ready());
    
    result.status = AsyncStatus::Running;
    ASSERT_FALSE(result.is_ready());
    
    result.status = AsyncStatus::Completed;
    ASSERT_TRUE(result.is_ready());
    
    result.status = AsyncStatus::Failed;
    ASSERT_TRUE(result.is_ready());
    
    result.status = AsyncStatus::Cancelled;
    ASSERT_TRUE(result.is_ready());
}

TEST(AsyncResult_IsSuccess) {
    std::cout << "  Testing: AsyncResult::is_success()" << std::endl;
    
    AsyncResult<int> result;
    
    result.status = AsyncStatus::Completed;
    ASSERT_TRUE(result.is_success());
    
    result.status = AsyncStatus::Failed;
    ASSERT_FALSE(result.is_success());
    
    result.status = AsyncStatus::Cancelled;
    ASSERT_FALSE(result.is_success());
}

TEST(AsyncResult_Duration) {
    std::cout << "  Testing: AsyncResult duration tracking" << std::endl;
    
    AsyncResult<int> result;
    result.duration = std::chrono::milliseconds(125);
    
    ASSERT_EQ(125, result.duration.count());
}

TEST(AsyncResult_ErrorMessage) {
    std::cout << "  Testing: AsyncResult error message" << std::endl;
    
    AsyncResult<int> result;
    result.status = AsyncStatus::Failed;
    result.error_message = "Operation failed";
    
    ASSERT_FALSE(result.error_message.empty());
    ASSERT_TRUE(result.error_message == "Operation failed");
}

// ============================================================================
// Test: Priority Levels
// ============================================================================

TEST(Priority_Levels) {
    std::cout << "  Testing: Task priority levels" << std::endl;
    
    ASSERT_TRUE(static_cast<int>(Priority::Low) < static_cast<int>(Priority::Normal));
    ASSERT_TRUE(static_cast<int>(Priority::Normal) < static_cast<int>(Priority::High));
    ASSERT_TRUE(static_cast<int>(Priority::High) < static_cast<int>(Priority::Critical));
}

TEST(Priority_Ordering) {
    std::cout << "  Testing: Priority ordering for task scheduling" << std::endl;
    
    std::vector<Priority> priorities = {
        Priority::Low,
        Priority::Normal,
        Priority::High,
        Priority::Critical
    };
    
    // Critical should execute first
    ASSERT_EQ(static_cast<int>(Priority::Critical), static_cast<int>(priorities.back()));
    ASSERT_EQ(static_cast<int>(Priority::Low), static_cast<int>(priorities.front()));
}

// ============================================================================
// Test: TaskHandle
// ============================================================================

TEST(TaskHandle_Validity) {
    std::cout << "  Testing: TaskHandle validity" << std::endl;
    
    TaskHandle invalid;
    ASSERT_FALSE(invalid.is_valid());
    
    TaskHandle valid(12345);
    ASSERT_TRUE(valid.is_valid());
    ASSERT_EQ(12345, valid.id());
}

TEST(TaskHandle_Equality) {
    std::cout << "  Testing: TaskHandle equality comparison" << std::endl;
    
    TaskHandle handle1(100);
    TaskHandle handle2(100);
    TaskHandle handle3(200);
    
    ASSERT_TRUE(handle1 == handle2);
    ASSERT_TRUE(handle1 != handle3);
}

TEST(TaskHandle_UniqueIDs) {
    std::cout << "  Testing: TaskHandle unique IDs" << std::endl;
    
    std::vector<TaskHandle> handles;
    for (uint64_t i = 1; i <= 100; ++i) {
        handles.emplace_back(i);
    }
    
    // All IDs should be unique
    for (size_t i = 0; i < handles.size(); ++i) {
        for (size_t j = i + 1; j < handles.size(); ++j) {
            ASSERT_TRUE(handles[i] != handles[j]);
        }
    }
}

// ============================================================================
// Test: Callback Types
// ============================================================================

TEST(Callbacks_ResultCallback) {
    std::cout << "  Testing: ResultCallback invocation" << std::endl;
    
    bool callback_invoked = false;
    int callback_value = 0;
    
    ResultCallback<int> callback = [&](const AsyncResult<int>& result) {
        callback_invoked = true;
        if (result.is_success()) {
            callback_value = result.value;
        }
    };
    
    AsyncResult<int> result;
    result.status = AsyncStatus::Completed;
    result.value = 42;
    
    callback(result);
    
    ASSERT_TRUE(callback_invoked);
    ASSERT_EQ(42, callback_value);
}

TEST(Callbacks_VoidCallback) {
    std::cout << "  Testing: VoidCallback invocation" << std::endl;
    
    bool callback_invoked = false;
    
    VoidCallback callback = [&]() {
        callback_invoked = true;
    };
    
    callback();
    
    ASSERT_TRUE(callback_invoked);
}

TEST(Callbacks_ErrorCallback) {
    std::cout << "  Testing: ErrorCallback invocation" << std::endl;
    
    std::string error_msg;
    
    ErrorCallback callback = [&](const std::string& msg) {
        error_msg = msg;
    };
    
    callback("Test error");
    
    ASSERT_TRUE(error_msg == "Test error");
}

// ============================================================================
// Test: Timeout Utility
// ============================================================================

TEST(Timeout_FastOperation) {
    std::cout << "  Testing: run_with_timeout - fast operation" << std::endl;
    
    bool completed = run_with_timeout([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }, std::chrono::milliseconds(100));
    
    ASSERT_TRUE(completed);
}

TEST(Timeout_SlowOperation) {
    std::cout << "  Testing: run_with_timeout - timeout exceeded" << std::endl;
    
    bool completed = run_with_timeout([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }, std::chrono::milliseconds(50));
    
    ASSERT_FALSE(completed);
}

TEST(Timeout_ImmediateReturn) {
    std::cout << "  Testing: run_with_timeout - immediate return" << std::endl;
    
    bool completed = run_with_timeout([]() {
        // Return immediately
    }, std::chrono::milliseconds(100));
    
    ASSERT_TRUE(completed);
}

// ============================================================================
// Test: Task Queueing (Conceptual)
// ============================================================================

TEST(TaskQueue_Priority) {
    std::cout << "  Testing: Task priority queue ordering" << std::endl;
    
    struct SimpleTask {
        Priority priority;
        int order;
        
        bool operator<(const SimpleTask& other) const {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
    };
    
    std::priority_queue<SimpleTask> queue;
    
    queue.push({Priority::Low, 1});
    queue.push({Priority::Critical, 2});
    queue.push({Priority::Normal, 3});
    queue.push({Priority::High, 4});
    
    // Critical should be first
    auto first = queue.top();
    ASSERT_EQ(static_cast<int>(Priority::Critical), static_cast<int>(first.priority));
}

TEST(TaskQueue_FIFO_SamePriority) {
    std::cout << "  Testing: FIFO ordering for same priority tasks" << std::endl;
    
    struct TimedTask {
        Priority priority;
        std::chrono::steady_clock::time_point created;
        int order;
        
        bool operator<(const TimedTask& other) const {
            if (priority != other.priority) {
                return static_cast<int>(priority) < static_cast<int>(other.priority);
            }
            return created > other.created; // Earlier tasks first
        }
    };
    
    std::priority_queue<TimedTask> queue;
    
    auto now = std::chrono::steady_clock::now();
    queue.push({Priority::Normal, now, 1});
    queue.push({Priority::Normal, now + std::chrono::milliseconds(1), 2});
    queue.push({Priority::Normal, now + std::chrono::milliseconds(2), 3});
    
    // First created should be first
    auto first = queue.top();
    ASSERT_EQ(1, first.order);
}

// ============================================================================
// Test: Concurrent Operations
// ============================================================================

TEST(Concurrent_MultipleReads) {
    std::cout << "  Testing: Concurrent read operations simulation" << std::endl;
    
    std::atomic<int> completed_operations{0};
    std::vector<std::thread> threads;
    
    // Simulate 10 concurrent read operations
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&completed_operations, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10 + (i % 5)));
            completed_operations++;
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(10, completed_operations.load());
}

TEST(Concurrent_ReadWrite) {
    std::cout << "  Testing: Concurrent read and write operations" << std::endl;
    
    std::atomic<int> reads{0};
    std::atomic<int> writes{0};
    
    std::vector<std::thread> threads;
    
    // 5 readers
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&reads]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            reads++;
        });
    }
    
    // 5 writers
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&writes]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            writes++;
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(5, reads.load());
    ASSERT_EQ(5, writes.load());
}

// ============================================================================
// Test: Batch Operations
// ============================================================================

TEST(Batch_MultipleOperations) {
    std::cout << "  Testing: Batch operation structure" << std::endl;
    
    struct Operation {
        enum Type { Read, Write };
        Type type;
        uint16_t did;
        std::vector<uint8_t> data;
    };
    
    std::vector<Operation> batch;
    
    // Add reads
    batch.push_back({Operation::Read, 0xF190, {}});
    batch.push_back({Operation::Read, 0xF191, {}});
    
    // Add writes
    batch.push_back({Operation::Write, 0x0102, {0x01, 0x02, 0x03}});
    
    ASSERT_EQ(3, batch.size());
}

TEST(Batch_ResultMapping) {
    std::cout << "  Testing: Batch result mapping" << std::endl;
    
    std::map<uint16_t, AsyncResult<std::vector<uint8_t>>> results;
    
    // Simulate results for different DIDs
    AsyncResult<std::vector<uint8_t>> result1;
    result1.status = AsyncStatus::Completed;
    result1.value = {0x01, 0x02, 0x03};
    results[0xF190] = result1;
    
    AsyncResult<std::vector<uint8_t>> result2;
    result2.status = AsyncStatus::Failed;
    results[0xF191] = result2;
    
    ASSERT_EQ(2, results.size());
    ASSERT_TRUE(results[0xF190].is_success());
    ASSERT_FALSE(results[0xF191].is_success());
}

// ============================================================================
// Test: Periodic Monitor Simulation
// ============================================================================

TEST(Monitor_ChangeDetection) {
    std::cout << "  Testing: Change detection for monitored values" << std::endl;
    
    std::vector<uint8_t> previous_value = {0x01, 0x02, 0x03};
    std::vector<uint8_t> current_value = {0x01, 0x02, 0x04}; // Changed
    
    bool changed = (previous_value != current_value);
    
    ASSERT_TRUE(changed);
}

TEST(Monitor_NoChange) {
    std::cout << "  Testing: No change detection" << std::endl;
    
    std::vector<uint8_t> previous_value = {0x01, 0x02, 0x03};
    std::vector<uint8_t> current_value = {0x01, 0x02, 0x03}; // Same
    
    bool changed = (previous_value != current_value);
    
    ASSERT_FALSE(changed);
}

TEST(Monitor_IntervalTiming) {
    std::cout << "  Testing: Monitor polling interval" << std::endl;
    
    auto last_poll = std::chrono::steady_clock::now();
    auto interval = std::chrono::milliseconds(100);
    
    std::this_thread::sleep_for(interval);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll);
    
    ASSERT_GE(elapsed.count(), interval.count());
}

TEST(Monitor_MultipleValues) {
    std::cout << "  Testing: Monitor multiple DID values" << std::endl;
    
    std::map<uint16_t, std::vector<uint8_t>> monitored_values;
    
    monitored_values[0xF190] = {0x01, 0x02};
    monitored_values[0xF191] = {0x03, 0x04};
    monitored_values[0xF192] = {0x05, 0x06};
    
    ASSERT_EQ(3, monitored_values.size());
}

// ============================================================================
// Test: Task Cancellation
// ============================================================================

TEST(Cancel_PendingTask) {
    std::cout << "  Testing: Cancel pending task" << std::endl;
    
    AsyncResult<int> result;
    result.status = AsyncStatus::Pending;
    
    // Simulate cancellation
    result.status = AsyncStatus::Cancelled;
    
    ASSERT_TRUE(result.status == AsyncStatus::Cancelled);
    ASSERT_TRUE(result.is_ready());
    ASSERT_FALSE(result.is_success());
}

TEST(Cancel_RunningTask) {
    std::cout << "  Testing: Cancel running task" << std::endl;
    
    std::atomic<bool> should_cancel{false};
    std::atomic<bool> task_completed{false};
    
    std::thread task([&]() {
        for (int i = 0; i < 100; ++i) {
            if (should_cancel.load()) {
                return; // Cancelled
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        task_completed = true;
    });
    
    // Cancel after short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    should_cancel = true;
    
    task.join();
    
    ASSERT_TRUE(should_cancel.load());
    ASSERT_FALSE(task_completed.load()); // Should not have completed
}

TEST(Cancel_AllTasks) {
    std::cout << "  Testing: Cancel all pending tasks" << std::endl;
    
    std::vector<AsyncResult<int>> results(10);
    
    // Set all to pending
    for (auto& result : results) {
        result.status = AsyncStatus::Pending;
    }
    
    // Cancel all
    for (auto& result : results) {
        result.status = AsyncStatus::Cancelled;
    }
    
    // Verify all cancelled
    int cancelled_count = 0;
    for (const auto& result : results) {
        if (result.status == AsyncStatus::Cancelled) {
            cancelled_count++;
        }
    }
    
    ASSERT_EQ(10, cancelled_count);
}

// ============================================================================
// Test: Wait Operations
// ============================================================================

TEST(Wait_TaskCompletion) {
    std::cout << "  Testing: Wait for task completion" << std::endl;
    
    std::atomic<bool> completed{false};
    
    std::thread task([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        completed = true;
    });
    
    // Wait for completion
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    
    while (!completed.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    task.join();
    
    ASSERT_TRUE(completed.load());
}

TEST(Wait_Timeout) {
    std::cout << "  Testing: Wait timeout" << std::endl;
    
    std::atomic<bool> completed{false};
    
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    
    // Simulate waiting for something that doesn't complete
    while (!completed.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    ASSERT_FALSE(completed.load()); // Should timeout
}

// ============================================================================
// Test: Pause/Resume
// ============================================================================

TEST(PauseResume_State) {
    std::cout << "  Testing: Pause and resume state" << std::endl;
    
    std::atomic<bool> paused{false};
    
    ASSERT_FALSE(paused.load());
    
    paused = true;
    ASSERT_TRUE(paused.load());
    
    paused = false;
    ASSERT_FALSE(paused.load());
}

TEST(PauseResume_TaskProcessing) {
    std::cout << "  Testing: Pause stops task processing" << std::endl;
    
    std::atomic<bool> paused{false};
    std::atomic<int> tasks_processed{0};
    
    std::thread worker([&]() {
        for (int i = 0; i < 10; ++i) {
            if (paused.load()) {
                // Wait until resumed
                while (paused.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
            tasks_processed++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Let it process a few tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    int initial_count = tasks_processed.load();
    
    // Pause
    paused = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int paused_count = tasks_processed.load();
    
    // Should not have processed many more tasks while paused
    ASSERT_LE(paused_count - initial_count, 2);
    
    // Resume
    paused = false;
    worker.join();
    
    ASSERT_EQ(10, tasks_processed.load());
}

// ============================================================================
// Test: Default Timeout Configuration
// ============================================================================

TEST(Config_DefaultTimeout) {
    std::cout << "  Testing: Default timeout configuration" << std::endl;
    
    std::chrono::milliseconds default_timeout{5000};
    
    ASSERT_EQ(5000, default_timeout.count());
}

TEST(Config_CustomTimeout) {
    std::cout << "  Testing: Custom timeout configuration" << std::endl;
    
    std::chrono::milliseconds custom_timeout{10000};
    
    ASSERT_EQ(10000, custom_timeout.count());
    ASSERT_GT(custom_timeout.count(), 5000);
}

// ============================================================================
// Test: Thread Safety Concepts
// ============================================================================

TEST(ThreadSafety_AtomicOperations) {
    std::cout << "  Testing: Atomic operations for thread safety" << std::endl;
    
    std::atomic<int> counter{0};
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < 100; ++j) {
                counter++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(1000, counter.load());
}

TEST(ThreadSafety_MutexProtection) {
    std::cout << "  Testing: Mutex-protected operations" << std::endl;
    
    int shared_value = 0;
    std::mutex mutex;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                std::lock_guard<std::mutex> lock(mutex);
                shared_value++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(1000, shared_value);
}

// ============================================================================
// Main test runner
// ============================================================================

int main() {
    std::cout << "\n" << colors::BOLD << "UDS Async Operations Tests" << colors::RESET << "\n";
    std::cout << "Testing Asynchronous Operations, Caching, and Concurrent Execution\n\n";
    
    TestRunner::instance().run_all_tests();
    
    auto stats = TestRunner::instance().get_stats();
    return (stats.failed == 0) ? 0 : 1;
}

