#pragma once

#include <future>
#include <vector>
#include <type_traits>

namespace cu
{

// Wait for all futures to complete.
template<typename T>
std::vector<T> wait_all(std::vector<std::future<T>>& futures)
{
    std::vector<T> results;
    results.reserve(futures.size());
    
    for (std::future<T>& future : futures)
    {
        results.push_back(future.get());
    }
    
    return results;
}

// Wait for any future to complete
template<typename T>
std::pair<std::size_t, T> wait_any(std::vector<std::future<T>>& futures)
{
    while (true)
    {
        for (std::size_t idx = 0; idx < futures.size(); ++idx)
        {
            if (is_ready(futures[idx]))
            {
                return {idx, futures[idx].get()};
            }
        }

        std::this_thread::yield();
    }
}

// Check if future is ready without blocking.
template<typename T>
bool is_ready(const std::future<T>& future)
{
    return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

} // namespace cu