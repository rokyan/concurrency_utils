#pragma once

#include <future>
#include <vector>
#include <type_traits>
#include <chrono>
#include <optional>
#include <exception>
#include <thread>
#include <algorithm>
#include <memory>

namespace cu
{

// ============================================================================
// BASIC UTILITIES
// ============================================================================

// Check if future is ready without blocking.
template<typename T>
bool is_ready(const std::future<T>& future)
{
    return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

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

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

// ============================================================================
// TIMEOUT UTILITIES
// ============================================================================

// Wait for all futures with timeout
template<typename T, typename Rep, typename Period>
std::optional<std::vector<T>> wait_all_for(
    std::vector<std::future<T>>& futures,
    const std::chrono::duration<Rep, Period>& timeout)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    std::vector<T> results;
    results.reserve(futures.size());

    for (auto& future : futures)
    {
        auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::seconds(0) ||
            future.wait_for(remaining) != std::future_status::ready)
        {
            return std::nullopt;  // Timeout
        }
        results.push_back(future.get());
    }

    return results;
}

// Wait for any future with timeout
template<typename T, typename Rep, typename Period>
std::optional<std::pair<std::size_t, T>> wait_any_for(
    std::vector<std::future<T>>& futures,
    const std::chrono::duration<Rep, Period>& timeout)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline)
    {
        for (std::size_t idx = 0; idx < futures.size(); ++idx)
        {
            if (is_ready(futures[idx]))
            {
                return std::make_pair(idx, futures[idx].get());
            }
        }

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    return std::nullopt;  // Timeout
}

// Wait for a single future with timeout
template<typename T, typename Rep, typename Period>
std::optional<T> wait_for(
    std::future<T>& future,
    const std::chrono::duration<Rep, Period>& timeout)
{
    if (future.wait_for(timeout) == std::future_status::ready)
    {
        return future.get();
    }
    return std::nullopt;
}

// ============================================================================
// ERROR HANDLING UTILITIES
// ============================================================================

// Result type that captures both success and failure
template<typename T>
struct future_result
{
    std::size_t index;
    std::optional<T> value;
    std::exception_ptr error;

    bool has_value() const noexcept { return value.has_value(); }
    bool has_error() const noexcept { return error != nullptr; }
    bool is_success() const noexcept { return has_value() && !has_error(); }
};

// Wait for all futures, collect exceptions instead of throwing
template<typename T>
std::vector<future_result<T>> wait_all_safe(std::vector<std::future<T>>& futures)
{
    std::vector<future_result<T>> results;
    results.reserve(futures.size());

    for (std::size_t idx = 0; idx < futures.size(); ++idx)
    {
        future_result<T> result{idx, std::nullopt, nullptr};

        try
        {
            result.value = futures[idx].get();
        }
        catch (...)
        {
            result.error = std::current_exception();
        }

        results.push_back(std::move(result));
    }

    return results;
}

// ============================================================================
// FUTURE CREATION UTILITIES
// ============================================================================

// Create an already-ready future with a value
template<typename T>
std::future<T> make_ready_future(T value)
{
    std::promise<T> promise;
    promise.set_value(std::move(value));
    return promise.get_future();
}

// Specialization for void
inline std::future<void> make_ready_future()
{
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
}

// Create an already-failed future with an exception
template<typename T>
std::future<T> make_exceptional_future(std::exception_ptr ex)
{
    std::promise<T> promise;
    promise.set_exception(ex);
    return promise.get_future();
}

// ============================================================================
// BATCHING AND PARTIAL RESULTS
// ============================================================================

// Wait for first N futures to complete
template<typename T>
std::vector<std::pair<std::size_t, T>> wait_n(
    std::vector<std::future<T>>& futures,
    std::size_t count)
{
    std::vector<std::pair<std::size_t, T>> results;
    results.reserve(std::min(count, futures.size()));

    std::vector<bool> consumed(futures.size(), false);

    while (results.size() < count && results.size() < futures.size())
    {
        for (std::size_t idx = 0; idx < futures.size(); ++idx)
        {
            if (!consumed[idx] && is_ready(futures[idx]))
            {
                results.emplace_back(idx, futures[idx].get());
                consumed[idx] = true;

                if (results.size() >= count)
                    break;
            }
        }

        if (results.size() < count)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    return results;
}

// Get all currently ready futures (non-blocking)
template<typename T>
std::vector<std::pair<std::size_t, T>> collect_ready(std::vector<std::future<T>>& futures)
{
    std::vector<std::pair<std::size_t, T>> results;

    for (std::size_t idx = 0; idx < futures.size(); ++idx)
    {
        if (is_ready(futures[idx]))
        {
            results.emplace_back(idx, futures[idx].get());
        }
    }

    return results;
}

// ============================================================================
// QUERY UTILITIES
// ============================================================================

// Count how many futures are ready
template<typename T>
std::size_t count_ready(const std::vector<std::future<T>>& futures)
{
    return std::count_if(futures.begin(), futures.end(),
                         [](const auto& f) { return is_ready(f); });
}

// Check if all futures are ready
template<typename T>
bool all_ready(const std::vector<std::future<T>>& futures)
{
    return std::all_of(futures.begin(), futures.end(),
                      [](const auto& f) { return is_ready(f); });
}

// Check if any future is ready
template<typename T>
bool any_ready(const std::vector<std::future<T>>& futures)
{
    return std::any_of(futures.begin(), futures.end(),
                      [](const auto& f) { return is_ready(f); });
}

// ============================================================================
// SPECIALIZED PATTERNS
// ============================================================================

// Race: return result of first successful future, ignore failures
template<typename T>
std::optional<T> race_first_success(std::vector<std::future<T>>& futures)
{
    std::vector<bool> failed(futures.size(), false);
    std::size_t fail_count = 0;

    while (fail_count < futures.size())
    {
        for (std::size_t idx = 0; idx < futures.size(); ++idx)
        {
            if (!failed[idx] && is_ready(futures[idx]))
            {
                try
                {
                    return futures[idx].get();
                }
                catch (...)
                {
                    failed[idx] = true;
                    fail_count++;
                }
            }
        }

        if (fail_count < futures.size())
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    return std::nullopt;  // All failed
}

// Wait for all with progress callback
template<typename T, typename Callback>
std::vector<T> wait_all_with_progress(
    std::vector<std::future<T>>& futures,
    Callback&& callback)
{
    std::vector<T> results;
    results.reserve(futures.size());

    for (std::size_t idx = 0; idx < futures.size(); ++idx)
    {
        results.push_back(futures[idx].get());
        callback(idx + 1, futures.size());  // completed, total
    }

    return results;
}

// Transform future result with a function (continuation)
template<typename T, typename F>
auto then(std::future<T>& future, F&& func) -> std::future<std::invoke_result_t<F, T>>
{
    using result_type = std::invoke_result_t<F, T>;

    auto promise = std::make_shared<std::promise<result_type>>();
    auto result_future = promise->get_future();

    std::thread([promise, &future, func = std::forward<F>(func)]() mutable {
        try
        {
            if constexpr (std::is_void_v<result_type>)
            {
                func(future.get());
                promise->set_value();
            }
            else
            {
                promise->set_value(func(future.get()));
            }
        }
        catch (...)
        {
            promise->set_exception(std::current_exception());
        }
    }).detach();

    return result_future;
}

} // namespace cu