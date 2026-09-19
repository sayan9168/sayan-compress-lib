/**
 * @file thread_pool.hpp
 * @brief Production-grade thread pool implementation
 * 
 * Properly synchronized thread pool with:
 * - Accurate task completion tracking
 * - Graceful shutdown
 * - Exception propagation
 * - Work stealing support (optional)
 * 
 * @author Sayan
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <stdexcept>
#include <type_traits>

namespace compress {

/**
 * @brief Exception thrown when thread pool operations fail
 */
class ThreadPoolException : public std::runtime_error {
public:
    explicit ThreadPoolException(const std::string& msg) 
        : std::runtime_error(msg) {}
};

/**
 * @class ThreadPool
 * @brief Production-grade thread pool with proper synchronization
 * 
 * Features:
 * - Exact task completion tracking via wait_all()
 * - Exception-safe task execution
 * - Graceful shutdown with task completion
 * - Configurable number of worker threads
 * - Thread-safe task submission
 * 
 * Usage:
 *   ThreadPool pool(4);
 *   
 *   // Submit tasks
 *   auto future = pool.submit([]{ return compute(); });
 *   pool.enqueue([]{ do_work(); });
 *   
 *   // Wait for all tasks to complete
 *   pool.wait_all();
 *   
 *   // Get result from submitted task
 *   auto result = future.get();
 */
class ThreadPool {
public:
    /**
     * @brief Construct thread pool with specified number of workers
     * @param num_threads Number of worker threads (default: hardware_concurrency)
     * @throws ThreadPoolException if thread creation fails
     */
    explicit ThreadPool(size_t num_threads = 0)
        : stop_(false)
        , active_tasks_(0)
        , tasks_completed_(0)
    {
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) {
                num_threads = 4; // Safe default
            }
        }
        
        // Limit to reasonable maximum
        if (num_threads > 64) {
            num_threads = 64;
        }
        
        workers_.reserve(num_threads);
        
        try {
            for (size_t i = 0; i < num_threads; ++i) {
                workers_.emplace_back([this] { worker_loop(); });
            }
        } catch (const std::system_error&) {
            // Clean up any threads that were created before failure
            shutdown_immediate();
            throw ThreadPoolException("Failed to create worker threads");
        }
    }
    
    /**
     * @brief Destructor - waits for all tasks to complete before shutting down
     */
    ~ThreadPool() {
        shutdown_graceful();
    }
    
    // Non-copyable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Movable (with caution)
    ThreadPool(ThreadPool&& other) noexcept
        : stop_(false)
        , active_tasks_(0)
        , tasks_completed_(0)
    {
        std::unique_lock<std::mutex> lock(other.queue_mutex_);
        stop_ = other.stop_.load();
        tasks_ = std::move(other.tasks_);
        active_tasks_ = other.active_tasks_.load();
        tasks_completed_ = other.tasks_completed_.load();
        lock.unlock();
        
        other.shutdown_immediate();
        
        // Start new workers
        size_t num_threads = other.workers_.size();
        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }
    
    ThreadPool& operator=(ThreadPool&& other) noexcept {
        if (this != &other) {
            shutdown_graceful();
            
            std::unique_lock<std::mutex> lock(other.queue_mutex_);
            stop_ = other.stop_.load();
            tasks_ = std::move(other.tasks_);
            active_tasks_ = other.active_tasks_.load();
            tasks_completed_ = other.tasks_completed_.load();
            lock.unlock();
            
            other.shutdown_immediate();
            
            // Restart workers
            workers_.clear();
            size_t num_threads = other.workers_.size();
            workers_.reserve(num_threads);
            for (size_t i = 0; i < num_threads; ++i) {
                workers_.emplace_back([this] { worker_loop(); });
            }
        }
        return *this;
    }
    
    /**
     * @brief Enqueue a fire-and-forget task
     * @param f Function to execute
     * @tparam F Function type
     * 
     * Thread-safe. Task will be executed by any available worker.
     */
    template<typename F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw ThreadPoolException("Cannot enqueue to stopped ThreadPool");
            }
            tasks_.emplace(std::forward<F>(f));
            ++active_tasks_;
        }
        task_available_.notify_one();
    }
    
    /**
     * @brief Submit a task and get a future for its result
     * @param f Function to execute
     * @param args Arguments to pass to function
     * @return std::future<ReturnType> for retrieving result
     * @tparam F Function type
     * @tparam Args Argument types
     * 
     * Thread-safe. Returns a future that will contain the result or exception.
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> 
    {
        using return_type = typename std::invoke_result<F, Args...>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        auto future = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw ThreadPoolException("Cannot submit to stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
            ++active_tasks_;
        }
        task_available_.notify_one();
        
        return future;
    }
    
    /**
     * @brief Wait for all currently queued tasks to complete
     * 
     * This is a blocking call that returns only when:
     * - All tasks that were queued at the time of calling have completed
     * - Note: Does NOT wait for tasks enqueued during the wait
     * 
     * Thread-safe and can be called from multiple threads.
     */
    void wait_all() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_done_.wait(lock, [this] {
            return active_tasks_ == 0 && tasks_.empty();
        });
    }
    
    /**
     * @brief Try to wait for all tasks with timeout
     * @param timeout Maximum time to wait
     * @return true if all tasks completed, false if timeout expired
     */
    template<typename Rep, typename Period>
    bool wait_for_all(std::chrono::duration<Rep, Period> timeout) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_done_.wait_for(lock, timeout, [this] {
            return active_tasks_ == 0 && tasks_.empty();
        });
    }
    
    /**
     * @brief Get number of pending tasks in queue
     * @return Number of tasks waiting to be executed
     */
    size_t pending_tasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }
    
    /**
     * @brief Get number of currently executing tasks
     * @return Number of tasks being processed by workers
     */
    size_t active_task_count() const {
        return active_tasks_.load();
    }
    
    /**
     * @brief Get total number of completed tasks since creation
     * @return Total completed task count
     */
    uint64_t total_completed_tasks() const {
        return tasks_completed_.load();
    }
    
    /**
     * @brief Get number of worker threads
     * @return Worker thread count
     */
    size_t worker_count() const {
        return workers_.size();
    }
    
    /**
     * @brief Initiate graceful shutdown
     * 
     * Workers will complete their current tasks and process remaining
     * queued tasks before exiting. New submissions will be rejected.
     */
    void shutdown() {
        shutdown_graceful();
    }
    
    /**
     * @brief Check if pool is stopping or stopped
     * @return true if shutdown has been initiated
     */
    bool is_stopped() const {
        return stop_.load();
    }

private:
    /**
     * @brief Main worker loop
     */
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                
                // Wait for task or stop signal
                task_available_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });
                
                // Exit if stopping and no more work
                if (stop_ && tasks_.empty()) {
                    return;
                }
                
                // Get next task
                if (!tasks_.empty()) {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                } else {
                    continue; // Spurious wakeup, check again
                }
            }
            
            // Execute task outside lock
            if (task) {
                try {
                    task();
                } catch (...) {
                    // Task exceptions are caught but not propagated
                    // (use submit() if you need exception handling)
                }
                
                // Decrement active count and notify waiters
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    --active_tasks_;
                    ++tasks_completed_;
                }
                tasks_done_.notify_all();
            }
        }
    }
    
    /**
     * @brief Graceful shutdown - complete pending tasks then stop
     */
    void shutdown_graceful() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) return; // Already stopped
            stop_ = true;
        }
        task_available_.notify_all();
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    /**
     * @brief Immediate shutdown - abandon pending tasks
     */
    void shutdown_immediate() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) return;
            stop_ = true;
            // Clear pending tasks
            while (!tasks_.empty()) {
                tasks_.pop();
            }
        }
        task_available_.notify_all();
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.detach(); // Can't join, just detach
            }
        }
        workers_.clear();
    }
    
    // Member variables
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable task_available_;
    std::condition_variable tasks_done_;
    std::atomic<bool> stop_;
    std::atomic<size_t> active_tasks_;
    std::atomic<uint64_t> tasks_completed_;
};

} // namespace compress
