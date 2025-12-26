#pragma once

#include <future>
#include <vector>
#include <type_traits>

namespace cu {

// Wait for all futures to complete
template<typename T>
std::vector<T> wait_all(std::vector<std::future<T>>& futures);

// Wait for any future to complete
template<typename T>
std::pair<size_t, T> wait_any(std::vector<std::future<T>>& futures);

// Check if future is ready without blocking
template<typename T>
bool is_ready(const std::future<T>& future);

// Implementation
template<typename T>
std::vector<T> wait_all(std::vector<std::future<T>>& futures) {
    std::vector<T> results;
    results.reserve(futures.size());
    
    for (auto& future : futures) {
        results.push_back(future.get());
    }
    
    return results;
}

template<typename T>
std::pair<size_t, T> wait_any(std::vector<std::future<T>>& futures) {
    while (true) {
        for (size_t i = 0; i < futures.size(); ++i) {
            if (is_ready(futures[i])) {
                return {i, futures[i].get()};
            }
        }
        std::this_thread::yield();
    }
}

template<typename T>
bool is_ready(const std::future<T>& future) {
    return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

} // namespace concurrency