#pragma once
/**
 * @file uds_async.hpp
 * @brief Asynchronous UDS operations with futures and callbacks
 * 
 * This module provides non-blocking UDS operations for:
 * - Concurrent requests to multiple ECUs
 * - Background polling and monitoring
 * - Event-driven programming patterns
 * - Task queuing and prioritization
 */

#include "uds.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <memory>
#include <optional>
#include <chrono>

namespace uds {
namespace async {

// ============================================================================
// Async Result Types
// ============================================================================

/**
 * @brief Async operation status
 */
enum class AsyncStatus {
    Pending,        ///< Operation not yet started
    Running,        ///< Operation in progress
    Completed,      ///< Operation completed successfully
    Failed,         ///< Operation failed
    Cancelled,      ///< Operation was cancelled
    TimedOut        ///< Operation timed out
};

/**
 * @brief Result of an async operation
 */
template<typename T>
struct AsyncResult {
    AsyncStatus status = AsyncStatus::Pending;
    T value;
    NegativeResponseCode nrc = static_cast<NegativeResponseCode>(0x00);
    std::string error_message;
    std::chrono::milliseconds duration{0};
    
    bool is_ready() const { 
        return status != AsyncStatus::Pending && status != AsyncStatus::Running; 
    }
    bool is_success() const { return status == AsyncStatus::Completed; }
};

/**
 * @brief Callback types
 */
template<typename T>
using ResultCallback = std::function<void(const AsyncResult<T>&)>;

using VoidCallback = std::function<void()>;
using ErrorCallback = std::function<void(const std::string&)>;

// ============================================================================
// Task Priority
// ============================================================================

/**
 * @brief Task priority levels
 */
enum class Priority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// ============================================================================
// Async Task
// ============================================================================

/**
 * @brief Handle to an async operation
 */
class TaskHandle {
public:
    TaskHandle() : id_(0), valid_(false) {}
    explicit TaskHandle(uint64_t id) : id_(id), valid_(true) {}
    
    uint64_t id() const { return id_; }
    bool is_valid() const { return valid_; }
    
    bool operator==(const TaskHandle& other) const { return id_ == other.id_; }
    bool operator!=(const TaskHandle& other) const { return id_ != other.id_; }
    
private:
    uint64_t id_;
    bool valid_;
};

// ============================================================================
// Async Client
// ============================================================================

/**
 * @brief Asynchronous UDS client with task queue
 */
class AsyncClient {
public:
    /**
     * @brief Construct async client
     * @param client Underlying UDS client
     * @param num_workers Number of worker threads (default: 1)
     */
    explicit AsyncClient(Client& client, size_t num_workers = 1);
    
    /**
     * @brief Destructor - stops all workers
     */
    ~AsyncClient();
    
    // Non-copyable
    AsyncClient(const AsyncClient&) = delete;
    AsyncClient& operator=(const AsyncClient&) = delete;
    
    // ========================================================================
    // Async Read Operations
    // ========================================================================
    
    /**
     * @brief Async read DID
     * @param did Data identifier
     * @param callback Result callback
     * @param priority Task priority
     * @return Task handle
     */
    TaskHandle read_did_async(uint16_t did,
                              ResultCallback<std::vector<uint8_t>> callback,
                              Priority priority = Priority::Normal);
    
    /**
     * @brief Async read DID with future
     * @param did Data identifier
     * @return Future with result
     */
    std::future<AsyncResult<std::vector<uint8_t>>> read_did_future(uint16_t did);
    
    /**
     * @brief Async read multiple DIDs
     * @param dids List of DIDs
     * @param callback Result callback (called once with all results)
     * @return Task handle
     */
    TaskHandle read_dids_async(const std::vector<uint16_t>& dids,
                               ResultCallback<std::map<uint16_t, std::vector<uint8_t>>> callback,
                               Priority priority = Priority::Normal);
    
    // ========================================================================
    // Async Write Operations
    // ========================================================================
    
    /**
     * @brief Async write DID
     * @param did Data identifier
     * @param data Data to write
     * @param callback Result callback
     * @return Task handle
     */
    TaskHandle write_did_async(uint16_t did, const std::vector<uint8_t>& data,
                               ResultCallback<bool> callback,
                               Priority priority = Priority::Normal);
    
    // ========================================================================
    // Async Service Operations
    // ========================================================================
    
    /**
     * @brief Async diagnostic session control
     */
    TaskHandle session_control_async(Session session,
                                     ResultCallback<bool> callback,
                                     Priority priority = Priority::High);
    
    /**
     * @brief Async security access (seed-key)
     * @param level Security level
     * @param key_calculator Function to calculate key from seed
     * @param callback Result callback
     */
    TaskHandle security_access_async(
        uint8_t level,
        std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)> key_calculator,
        ResultCallback<bool> callback,
        Priority priority = Priority::High);
    
    /**
     * @brief Async routine control
     */
    TaskHandle routine_control_async(uint8_t control_type, uint16_t routine_id,
                                     const std::vector<uint8_t>& params,
                                     ResultCallback<std::vector<uint8_t>> callback,
                                     Priority priority = Priority::Normal);
    
    // ========================================================================
    // Task Management
    // ========================================================================
    
    /**
     * @brief Cancel a pending task
     * @param handle Task handle
     * @return True if task was cancelled
     */
    bool cancel(TaskHandle handle);
    
    /**
     * @brief Cancel all pending tasks
     */
    void cancel_all();
    
    /**
     * @brief Wait for a task to complete
     * @param handle Task handle
     * @param timeout Maximum wait time
     * @return True if task completed (not timed out)
     */
    bool wait(TaskHandle handle, std::chrono::milliseconds timeout);
    
    /**
     * @brief Wait for all tasks to complete
     */
    void wait_all(std::chrono::milliseconds timeout);
    
    /**
     * @brief Get task status
     */
    AsyncStatus get_status(TaskHandle handle) const;
    
    /**
     * @brief Get number of pending tasks
     */
    size_t pending_count() const;
    
    /**
     * @brief Check if any tasks are running
     */
    bool is_busy() const;
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * @brief Set default timeout for operations
     */
    void set_default_timeout(std::chrono::milliseconds timeout) {
        default_timeout_ = timeout;
    }
    
    /**
     * @brief Pause task processing
     */
    void pause();
    
    /**
     * @brief Resume task processing
     */
    void resume();
    
    /**
     * @brief Check if paused
     */
    bool is_paused() const { return paused_; }

private:
    struct Task {
        uint64_t id;
        Priority priority;
        std::function<void()> execute;
        std::atomic<AsyncStatus>* status;
        std::chrono::steady_clock::time_point created;
        
        bool operator<(const Task& other) const {
            // Higher priority first, then earlier creation time
            if (priority != other.priority) {
                return static_cast<int>(priority) < static_cast<int>(other.priority);
            }
            return created > other.created;
        }
    };
    
    Client& client_;
    std::vector<std::thread> workers_;
    std::priority_queue<Task> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> running_{true};
    std::atomic<bool> paused_{false};
    std::atomic<uint64_t> next_task_id_{1};
    std::chrono::milliseconds default_timeout_{5000};
    
    std::map<uint64_t, std::atomic<AsyncStatus>> task_statuses_;
    mutable std::mutex status_mutex_;
    
    void worker_loop();
    uint64_t enqueue_task(Priority priority, std::function<void()> func);
};

// ============================================================================
// Periodic Monitor
// ============================================================================

/**
 * @brief Periodically monitors DIDs and invokes callbacks on change
 */
class PeriodicMonitor {
public:
    explicit PeriodicMonitor(Client& client);
    ~PeriodicMonitor();
    
    // Non-copyable
    PeriodicMonitor(const PeriodicMonitor&) = delete;
    PeriodicMonitor& operator=(const PeriodicMonitor&) = delete;
    
    /**
     * @brief Add DID to monitor
     * @param did Data identifier
     * @param interval Polling interval
     * @param on_change Callback when value changes
     * @param on_error Callback on read error
     */
    void add_did(uint16_t did, std::chrono::milliseconds interval,
                 std::function<void(uint16_t, const std::vector<uint8_t>&)> on_change,
                 ErrorCallback on_error = nullptr);
    
    /**
     * @brief Remove DID from monitoring
     */
    void remove_did(uint16_t did);
    
    /**
     * @brief Start monitoring
     */
    void start();
    
    /**
     * @brief Stop monitoring
     */
    void stop();
    
    /**
     * @brief Check if monitoring is active
     */
    bool is_running() const { return running_; }
    
    /**
     * @brief Get current value of monitored DID
     */
    std::optional<std::vector<uint8_t>> get_current_value(uint16_t did) const;
    
    /**
     * @brief Set global error callback
     */
    void set_error_callback(ErrorCallback callback) { global_error_cb_ = callback; }

private:
    struct MonitoredDID {
        uint16_t did;
        std::chrono::milliseconds interval;
        std::chrono::steady_clock::time_point last_poll;
        std::vector<uint8_t> last_value;
        std::function<void(uint16_t, const std::vector<uint8_t>&)> on_change;
        ErrorCallback on_error;
    };
    
    Client& client_;
    std::map<uint16_t, MonitoredDID> monitored_;
    mutable std::mutex mutex_;
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};
    std::condition_variable cv_;
    ErrorCallback global_error_cb_;
    
    void monitor_loop();
};

// ============================================================================
// Batch Operations
// ============================================================================

/**
 * @brief Execute multiple operations in parallel
 */
class BatchExecutor {
public:
    explicit BatchExecutor(Client& client, size_t max_concurrent = 4);
    
    /**
     * @brief Add read operation to batch
     */
    void add_read(uint16_t did);
    
    /**
     * @brief Add write operation to batch
     */
    void add_write(uint16_t did, const std::vector<uint8_t>& data);
    
    /**
     * @brief Execute all operations
     * @return Map of DID -> result
     */
    std::map<uint16_t, AsyncResult<std::vector<uint8_t>>> execute();
    
    /**
     * @brief Execute with progress callback
     */
    std::map<uint16_t, AsyncResult<std::vector<uint8_t>>> execute(
        std::function<void(size_t completed, size_t total)> progress);
    
    /**
     * @brief Clear pending operations
     */
    void clear();
    
    /**
     * @brief Get number of pending operations
     */
    size_t size() const { return operations_.size(); }

private:
    struct Operation {
        enum Type { Read, Write };
        Type type;
        uint16_t did;
        std::vector<uint8_t> data;  // For writes
    };
    
    Client& client_;
    std::vector<Operation> operations_;
    [[maybe_unused]] size_t max_concurrent_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Run function with timeout
 * @return True if completed within timeout
 */
template<typename Func>
bool run_with_timeout(Func&& func, std::chrono::milliseconds timeout) {
    std::promise<void> promise;
    auto future = promise.get_future();
    
    std::thread t([&]() {
        func();
        promise.set_value();
    });
    
    bool completed = future.wait_for(timeout) == std::future_status::ready;
    
    if (t.joinable()) {
        if (completed) {
            t.join();
        } else {
            t.detach();  // Let it finish in background
        }
    }
    
    return completed;
}

/**
 * @brief Format async status for display
 */
inline const char* status_name(AsyncStatus status) {
    switch (status) {
        case AsyncStatus::Pending: return "Pending";
        case AsyncStatus::Running: return "Running";
        case AsyncStatus::Completed: return "Completed";
        case AsyncStatus::Failed: return "Failed";
        case AsyncStatus::Cancelled: return "Cancelled";
        case AsyncStatus::TimedOut: return "TimedOut";
        default: return "Unknown";
    }
}

} // namespace async
} // namespace uds
