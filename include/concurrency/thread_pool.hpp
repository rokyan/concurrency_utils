#pragma once

#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

namespace cu
{

class thread_pool
{
public:
    explicit thread_pool(std::size_t num_threads = std::thread::hardware_concurrency());
    ~thread_pool();

    // Delete copy operations.
    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;

    // Move operations.
    thread_pool(thread_pool&&) noexcept = default;
    thread_pool& operator=(thread_pool&&) noexcept = default;

    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    void shutdown();
    std::size_t size() const noexcept;

private:
    void worker_thread();

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop = false;
};

template<typename F, typename... Args>
auto thread_pool::enqueue(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>
    (
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        if (stop)
        {
            throw std::runtime_error("enqueue on stopped thread_pool");
        }

        tasks.emplace([task]() { (*task)(); });
    }

    condition.notify_one();

    return result;
}

} // namespace cu