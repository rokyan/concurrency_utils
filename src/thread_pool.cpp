#include "concurrency/thread_pool.hpp"

namespace cu
{

thread_pool::thread_pool(size_t num_threads)
{
    workers.reserve(num_threads);

    for (size_t i = 0; i < num_threads; ++i)
    {
        workers.emplace_back(&thread_pool::worker_thread, this);
    }
}

thread_pool::~thread_pool()
{
    shutdown();
}

void thread_pool::worker_thread()
{
    while (true)
    {
        std::function<void()> task;
        
        {
            std::unique_lock lock(queue_mutex);

            condition.wait(lock, [this]
            {
                return stop || !tasks.empty();
            });
            
            if (stop && tasks.empty())
            {
                return;
            }
            
            task = std::move(tasks.front());
            tasks.pop();
        }
        
        task();
    }
}

void thread_pool::shutdown()
{
    {
        std::unique_lock lock(queue_mutex);
        stop = true;
    }

    condition.notify_all();

    for (auto& worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    
    workers.clear();
}

size_t thread_pool::size() const noexcept
{
    return workers.size();
}

} // namespace cu