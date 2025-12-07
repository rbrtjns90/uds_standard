#include "uds_async.hpp"
#include <algorithm>

namespace uds {
namespace async {

// ============================================================================
// AsyncClient Implementation
// ============================================================================

AsyncClient::AsyncClient(Client& client, size_t num_workers)
    : client_(client) {
    // Start worker threads
    for (size_t i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&AsyncClient::worker_loop, this);
    }
}

AsyncClient::~AsyncClient() {
    running_ = false;
    queue_cv_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void AsyncClient::worker_loop() {
    while (running_) {
        Task task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !running_ || (!task_queue_.empty() && !paused_);
            });
            
            if (!running_) break;
            if (task_queue_.empty() || paused_) continue;
            
            task = task_queue_.top();
            task_queue_.pop();
        }
        
        // Update status to running
        if (task.status) {
            task.status->store(AsyncStatus::Running);
        }
        
        // Execute task
        try {
            task.execute();
        } catch (...) {
            if (task.status) {
                task.status->store(AsyncStatus::Failed);
            }
        }
    }
}

uint64_t AsyncClient::enqueue_task(Priority priority, std::function<void()> func) {
    uint64_t id = next_task_id_++;
    
    {
        std::lock_guard<std::mutex> status_lock(status_mutex_);
        task_statuses_[id].store(AsyncStatus::Pending);
    }
    
    Task task;
    task.id = id;
    task.priority = priority;
    task.execute = std::move(func);
    task.created = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> status_lock(status_mutex_);
        task.status = &task_statuses_[id];
    }
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }
    
    queue_cv_.notify_one();
    return id;
}

TaskHandle AsyncClient::read_did_async(uint16_t did,
                                       ResultCallback<std::vector<uint8_t>> callback,
                                       Priority priority) {
    auto id = enqueue_task(priority, [this, did, callback]() {
        AsyncResult<std::vector<uint8_t>> result;
        auto start = std::chrono::steady_clock::now();
        
        auto response = client_.read_data_by_identifier(did);
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (response.ok) {
            result.status = AsyncStatus::Completed;
            result.value = response.payload;
        } else {
            result.status = AsyncStatus::Failed;
            result.nrc = response.nrc.code;
            result.error_message = "Read DID failed";
        }
        
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            if (task_statuses_.count(0)) {  // Simplified - would need proper task ID tracking
                task_statuses_[0].store(result.status);
            }
        }
        
        if (callback) {
            callback(result);
        }
    });
    
    return TaskHandle(id);
}

std::future<AsyncResult<std::vector<uint8_t>>> AsyncClient::read_did_future(uint16_t did) {
    auto promise = std::make_shared<std::promise<AsyncResult<std::vector<uint8_t>>>>();
    auto future = promise->get_future();
    
    read_did_async(did, [promise](const AsyncResult<std::vector<uint8_t>>& result) {
        promise->set_value(result);
    });
    
    return future;
}

TaskHandle AsyncClient::read_dids_async(const std::vector<uint16_t>& dids,
                                        ResultCallback<std::map<uint16_t, std::vector<uint8_t>>> callback,
                                        Priority priority) {
    auto id = enqueue_task(priority, [this, dids, callback]() {
        AsyncResult<std::map<uint16_t, std::vector<uint8_t>>> result;
        auto start = std::chrono::steady_clock::now();
        
        result.status = AsyncStatus::Completed;
        
        for (uint16_t did : dids) {
            auto response = client_.read_data_by_identifier(did);
            if (response.ok) {
                result.value[did] = response.payload;
            } else {
                // Continue reading other DIDs even if one fails
                result.status = AsyncStatus::Failed;
                result.error_message = "One or more DIDs failed to read";
            }
        }
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (callback) {
            callback(result);
        }
    });
    
    return TaskHandle(id);
}

TaskHandle AsyncClient::write_did_async(uint16_t did, const std::vector<uint8_t>& data,
                                        ResultCallback<bool> callback,
                                        Priority priority) {
    auto id = enqueue_task(priority, [this, did, data, callback]() {
        AsyncResult<bool> result;
        auto start = std::chrono::steady_clock::now();
        
        auto response = client_.write_data_by_identifier(did, data);
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (response.ok) {
            result.status = AsyncStatus::Completed;
            result.value = true;
        } else {
            result.status = AsyncStatus::Failed;
            result.value = false;
            result.nrc = response.nrc.code;
            result.error_message = "Write DID failed";
        }
        
        if (callback) {
            callback(result);
        }
    });
    
    return TaskHandle(id);
}

TaskHandle AsyncClient::session_control_async(Session session,
                                              ResultCallback<bool> callback,
                                              Priority priority) {
    auto id = enqueue_task(priority, [this, session, callback]() {
        AsyncResult<bool> result;
        auto start = std::chrono::steady_clock::now();
        
        auto response = client_.diagnostic_session_control(session);
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (response.ok) {
            result.status = AsyncStatus::Completed;
            result.value = true;
        } else {
            result.status = AsyncStatus::Failed;
            result.value = false;
            result.nrc = response.nrc.code;
        }
        
        if (callback) {
            callback(result);
        }
    });
    
    return TaskHandle(id);
}

TaskHandle AsyncClient::security_access_async(
    uint8_t level,
    std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)> key_calculator,
    ResultCallback<bool> callback,
    Priority priority) {
    
    auto id = enqueue_task(priority, [this, level, key_calculator, callback]() {
        AsyncResult<bool> result;
        auto start = std::chrono::steady_clock::now();
        
        // Request seed
        auto seed_response = client_.security_access_request_seed(level);
        if (!seed_response.ok) {
            result.status = AsyncStatus::Failed;
            result.value = false;
            result.nrc = seed_response.nrc.code;
            result.error_message = "Failed to get seed";
            if (callback) callback(result);
            return;
        }
        
        // Calculate key
        auto key = key_calculator(seed_response.payload);
        
        // Send key
        auto key_response = client_.security_access_send_key(level + 1, key);
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (key_response.ok) {
            result.status = AsyncStatus::Completed;
            result.value = true;
        } else {
            result.status = AsyncStatus::Failed;
            result.value = false;
            result.nrc = key_response.nrc.code;
            result.error_message = "Key rejected";
        }
        
        if (callback) {
            callback(result);
        }
    });
    
    return TaskHandle(id);
}

TaskHandle AsyncClient::routine_control_async(uint8_t control_type, uint16_t routine_id,
                                              const std::vector<uint8_t>& params,
                                              ResultCallback<std::vector<uint8_t>> callback,
                                              Priority priority) {
    auto id = enqueue_task(priority, [this, control_type, routine_id, params, callback]() {
        AsyncResult<std::vector<uint8_t>> result;
        auto start = std::chrono::steady_clock::now();
        
        auto response = client_.routine_control(static_cast<RoutineAction>(control_type), routine_id, params);
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (response.ok) {
            result.status = AsyncStatus::Completed;
            result.value = response.payload;
        } else {
            result.status = AsyncStatus::Failed;
            result.nrc = response.nrc.code;
        }
        
        if (callback) {
            callback(result);
        }
    });
    
    return TaskHandle(id);
}

bool AsyncClient::cancel(TaskHandle handle) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    auto it = task_statuses_.find(handle.id());
    if (it != task_statuses_.end()) {
        AsyncStatus expected = AsyncStatus::Pending;
        return it->second.compare_exchange_strong(expected, AsyncStatus::Cancelled);
    }
    return false;
}

void AsyncClient::cancel_all() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    for (auto& [id, status] : task_statuses_) {
        AsyncStatus expected = AsyncStatus::Pending;
        status.compare_exchange_strong(expected, AsyncStatus::Cancelled);
    }
}

bool AsyncClient::wait(TaskHandle handle, std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        auto status = get_status(handle);
        if (status != AsyncStatus::Pending && status != AsyncStatus::Running) {
            return true;
        }
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void AsyncClient::wait_all(std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    
    while (pending_count() > 0) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

AsyncStatus AsyncClient::get_status(TaskHandle handle) const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    auto it = task_statuses_.find(handle.id());
    if (it != task_statuses_.end()) {
        return it->second.load();
    }
    return AsyncStatus::Failed;
}

size_t AsyncClient::pending_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return task_queue_.size();
}

bool AsyncClient::is_busy() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    for (const auto& [id, status] : task_statuses_) {
        if (status.load() == AsyncStatus::Running) {
            return true;
        }
    }
    return pending_count() > 0;
}

void AsyncClient::pause() {
    paused_ = true;
}

void AsyncClient::resume() {
    paused_ = false;
    queue_cv_.notify_all();
}

// ============================================================================
// PeriodicMonitor Implementation
// ============================================================================

PeriodicMonitor::PeriodicMonitor(Client& client)
    : client_(client) {
}

PeriodicMonitor::~PeriodicMonitor() {
    stop();
}

void PeriodicMonitor::add_did(uint16_t did, std::chrono::milliseconds interval,
                              std::function<void(uint16_t, const std::vector<uint8_t>&)> on_change,
                              ErrorCallback on_error) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    MonitoredDID entry;
    entry.did = did;
    entry.interval = interval;
    entry.last_poll = std::chrono::steady_clock::now() - interval;  // Poll immediately
    entry.on_change = on_change;
    entry.on_error = on_error;
    
    monitored_[did] = entry;
}

void PeriodicMonitor::remove_did(uint16_t did) {
    std::lock_guard<std::mutex> lock(mutex_);
    monitored_.erase(did);
}

void PeriodicMonitor::start() {
    if (running_) return;
    
    running_ = true;
    monitor_thread_ = std::thread(&PeriodicMonitor::monitor_loop, this);
}

void PeriodicMonitor::stop() {
    if (!running_) return;
    
    running_ = false;
    cv_.notify_all();
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

std::optional<std::vector<uint8_t>> PeriodicMonitor::get_current_value(uint16_t did) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = monitored_.find(did);
    if (it != monitored_.end() && !it->second.last_value.empty()) {
        return it->second.last_value;
    }
    return std::nullopt;
}

void PeriodicMonitor::monitor_loop() {
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::milliseconds min_wait{1000};
        
        std::vector<std::pair<uint16_t, MonitoredDID*>> to_poll;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [did, entry] : monitored_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - entry.last_poll);
                
                if (elapsed >= entry.interval) {
                    to_poll.emplace_back(did, &entry);
                } else {
                    auto remaining = entry.interval - elapsed;
                    min_wait = std::min(min_wait, remaining);
                }
            }
        }
        
        // Poll outside of lock
        for (auto& [did, entry] : to_poll) {
            try {
                auto response = client_.read_data_by_identifier(did);
                
                std::lock_guard<std::mutex> lock(mutex_);
                entry->last_poll = std::chrono::steady_clock::now();
                
                if (response.ok) {
                    if (entry->last_value != response.payload) {
                        entry->last_value = response.payload;
                        if (entry->on_change) {
                            entry->on_change(did, response.payload);
                        }
                    }
                } else {
                    std::string error = "Read failed for DID 0x" + std::to_string(did);
                    if (entry->on_error) {
                        entry->on_error(error);
                    } else if (global_error_cb_) {
                        global_error_cb_(error);
                    }
                }
            } catch (const std::exception& e) {
                if (global_error_cb_) {
                    global_error_cb_(e.what());
                }
            }
        }
        
        // Wait for next poll or stop signal
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, min_wait, [this] { return !running_; });
    }
}

// ============================================================================
// BatchExecutor Implementation
// ============================================================================

BatchExecutor::BatchExecutor(Client& client, size_t max_concurrent)
    : client_(client), max_concurrent_(max_concurrent) {
}

void BatchExecutor::add_read(uint16_t did) {
    Operation op;
    op.type = Operation::Read;
    op.did = did;
    operations_.push_back(op);
}

void BatchExecutor::add_write(uint16_t did, const std::vector<uint8_t>& data) {
    Operation op;
    op.type = Operation::Write;
    op.did = did;
    op.data = data;
    operations_.push_back(op);
}

std::map<uint16_t, AsyncResult<std::vector<uint8_t>>> BatchExecutor::execute() {
    return execute(nullptr);
}

std::map<uint16_t, AsyncResult<std::vector<uint8_t>>> BatchExecutor::execute(
    std::function<void(size_t, size_t)> progress) {
    
    std::map<uint16_t, AsyncResult<std::vector<uint8_t>>> results;
    size_t completed = 0;
    size_t total = operations_.size();
    
    // For simplicity, execute sequentially
    // A full implementation would use thread pool for concurrent execution
    for (const auto& op : operations_) {
        AsyncResult<std::vector<uint8_t>> result;
        auto start = std::chrono::steady_clock::now();
        
        if (op.type == Operation::Read) {
            auto response = client_.read_data_by_identifier(op.did);
            if (response.ok) {
                result.status = AsyncStatus::Completed;
                result.value = response.payload;
            } else {
                result.status = AsyncStatus::Failed;
                result.nrc = response.nrc.code;
            }
        } else {
            auto response = client_.write_data_by_identifier(op.did, op.data);
            if (response.ok) {
                result.status = AsyncStatus::Completed;
                result.value = op.data;
            } else {
                result.status = AsyncStatus::Failed;
                result.nrc = response.nrc.code;
            }
        }
        
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        results[op.did] = result;
        completed++;
        
        if (progress) {
            progress(completed, total);
        }
    }
    
    return results;
}

void BatchExecutor::clear() {
    operations_.clear();
}

} // namespace async
} // namespace uds
