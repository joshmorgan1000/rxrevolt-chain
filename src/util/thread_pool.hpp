#ifndef RXREVOLTCHAIN_UTIL_THREAD_POOL_HPP
#define RXREVOLTCHAIN_UTIL_THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <future>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

/**
 * @file thread_pool.hpp
 * @brief A simple thread pool for RxRevoltChain, allowing tasks to be queued
 *        and executed by a fixed number of worker threads.
 *
 * Usage Example:
 *  @code
 *    rxrevoltchain::util::ThreadPool pool(4); // 4 threads
 *    auto result = pool.enqueue([](int x) { return x*x; }, 10);
 *    // do other work...
 *    std::cout << "Result: " << result.get() << std::endl;
 *  @endcode
 */

namespace rxrevoltchain {
namespace util {

/**
 * @class ThreadPool
 * @brief A basic fixed-size thread pool implementation.
 *
 * - Constructor spawns a given number of worker threads.
 * - enqueue(...) can be used to schedule tasks for asynchronous execution.
 * - Destructor gracefully shuts down the pool, waiting for all tasks to finish.
 */
class ThreadPool
{
public:
    /**
     * @brief Construct a new ThreadPool object.
     * @param threadCount Number of worker threads to create. If zero, uses hardware concurrency.
     */
    explicit ThreadPool(size_t threadCount = 0)
        : stop_(false)
    {
        if (threadCount == 0) {
            threadCount = std::max<size_t>(1, std::thread::hardware_concurrency());
        }
        workers_.reserve(threadCount);

        for (size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this] {
                // Each worker thread runs this loop
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        // Wait until there's a task in the queue, or the pool is stopping
                        condVar_.wait(lock, [this] {
                            return !taskQueue_.empty() || stop_;
                        });

                        if (stop_ && taskQueue_.empty()) {
                            // Pool is stopping and no tasks remain
                            return;
                        }
                        // Pop a task from the queue
                        task = std::move(taskQueue_.front());
                        taskQueue_.pop();
                    }
                    // Execute the task
                    task();
                }
            });
        }
    }

    /**
     * @brief Destroy the ThreadPool object.
     *        Notifies all workers to stop and joins them.
     */
    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        // Wake all threads
        condVar_.notify_all();

        // Join all threads
        for (auto &worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    /**
     * @brief Enqueue a task into the thread pool for asynchronous execution.
     * @tparam F A callable type (function, functor, lambda).
     * @tparam Args Parameter pack for arguments to F.
     * @param f The callable to execute.
     * @param args Arguments to pass to the callable.
     * @return A std::future<ReturnType> that can be used to retrieve the result.
     *
     * Example:
     *  @code
     *    auto result = pool.enqueue([](int x){ return x+1; }, 42);
     *    // ...
     *    std::cout << result.get() << std::endl; // prints 43
     *  @endcode
     */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        // Create a packaged task from the callable
        auto taskPtr = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = taskPtr->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            // Emplace a wrapper lambda into the task queue
            taskQueue_.emplace([taskPtr]() { (*taskPtr)(); });
        }
        condVar_.notify_one();
        return res;
    }

private:
    std::vector<std::thread> workers_;               ///< The pool of worker threads
    std::queue<std::function<void()>> taskQueue_;    ///< Task queue
    std::mutex queueMutex_;                          ///< Mutex to protect the queue
    std::condition_variable condVar_;                ///< Condition variable for task readiness
    bool stop_;                                      ///< Signals the pool to stop accepting new tasks
};

} // namespace util
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_UTIL_THREAD_POOL_HPP
