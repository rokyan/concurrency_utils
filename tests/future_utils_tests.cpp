#include <gtest/gtest.h>
#include "concurrency/future_utils.hpp"
#include <thread>
#include <stdexcept>

namespace tests
{

// ============================================================================
// BASIC UTILITIES TESTS
// ============================================================================

TEST(FutureUtilsTest, WaitAll)
{
    std::vector<std::future<int>> futures;

    for (int idx = 0; idx < 5; ++idx)
    {
        futures.push_back(std::async(std::launch::async, [idx]()
        {
            return idx * 2;
        }));
    }

    auto results = cu::wait_all(futures);

    ASSERT_EQ(results.size(), 5);
    for (size_t idx = 0; idx < results.size(); ++idx)
    {
        EXPECT_EQ(results[idx], idx * 2);
    }
}

TEST(FutureUtilsTest, IsReady)
{
    auto future = std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });

    // Should not be ready immediately.
    EXPECT_FALSE(cu::is_ready(future));

    // Wait and check again.
    future.wait();
    EXPECT_TRUE(cu::is_ready(future));
}

TEST(FutureUtilsTest, WaitAny)
{
    std::vector<std::future<int>> futures;

    // First future completes quickly
    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    }));

    // Second future takes longer
    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return 100;
    }));

    auto [index, value] = cu::wait_any(futures);

    EXPECT_EQ(index, 0);  // First future should complete first
    EXPECT_EQ(value, 42);
}

// ============================================================================
// TIMEOUT UTILITIES TESTS
// ============================================================================

TEST(FutureUtilsTest, WaitAllForSuccess)
{
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 3; ++i)
    {
        futures.push_back(std::async(std::launch::async, [i]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i * 10;
        }));
    }

    auto results = cu::wait_all_for(futures, std::chrono::seconds(1));

    ASSERT_TRUE(results.has_value());
    EXPECT_EQ(results->size(), 3);
    EXPECT_EQ((*results)[0], 0);
    EXPECT_EQ((*results)[1], 10);
    EXPECT_EQ((*results)[2], 20);
}

TEST(FutureUtilsTest, WaitAllForTimeout)
{
    std::vector<std::future<int>> futures;

    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 42;
    }));

    auto results = cu::wait_all_for(futures, std::chrono::milliseconds(50));

    EXPECT_FALSE(results.has_value());  // Should timeout
}

TEST(FutureUtilsTest, WaitAnyForSuccess)
{
    std::vector<std::future<int>> futures;

    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return 42;
    }));

    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 100;
    }));

    auto result = cu::wait_any_for(futures, std::chrono::seconds(1));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, 0);
    EXPECT_EQ(result->second, 42);
}

TEST(FutureUtilsTest, WaitAnyForTimeout)
{
    std::vector<std::future<int>> futures;

    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 42;
    }));

    auto result = cu::wait_any_for(futures, std::chrono::milliseconds(50));

    EXPECT_FALSE(result.has_value());  // Should timeout
}

TEST(FutureUtilsTest, WaitForSuccess)
{
    auto future = std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    });

    auto result = cu::wait_for(future, std::chrono::seconds(1));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(FutureUtilsTest, WaitForTimeout)
{
    auto future = std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 42;
    });

    auto result = cu::wait_for(future, std::chrono::milliseconds(50));

    EXPECT_FALSE(result.has_value());  // Should timeout
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

TEST(FutureUtilsTest, WaitAllSafeAllSuccess)
{
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 3; ++i)
    {
        futures.push_back(std::async(std::launch::async, [i]() { return i * 10; }));
    }

    auto results = cu::wait_all_safe(futures);

    ASSERT_EQ(results.size(), 3);
    for (size_t i = 0; i < results.size(); ++i)
    {
        EXPECT_TRUE(results[i].is_success());
        EXPECT_FALSE(results[i].has_error());
        EXPECT_EQ(results[i].value.value(), i * 10);
    }
}

TEST(FutureUtilsTest, WaitAllSafeSomeFailures)
{
    std::vector<std::future<int>> futures;

    // Success
    futures.push_back(std::async(std::launch::async, []() { return 42; }));

    // Failure
    futures.push_back(std::async(std::launch::async, []() -> int
    {
        throw std::runtime_error("Test error");
    }));

    // Success
    futures.push_back(std::async(std::launch::async, []() { return 100; }));

    auto results = cu::wait_all_safe(futures);

    ASSERT_EQ(results.size(), 3);

    // First result: success
    EXPECT_TRUE(results[0].is_success());
    EXPECT_EQ(results[0].value.value(), 42);

    // Second result: failure
    EXPECT_FALSE(results[1].is_success());
    EXPECT_TRUE(results[1].has_error());
    EXPECT_FALSE(results[1].has_value());

    // Third result: success
    EXPECT_TRUE(results[2].is_success());
    EXPECT_EQ(results[2].value.value(), 100);
}

// ============================================================================
// FUTURE CREATION TESTS
// ============================================================================

TEST(FutureUtilsTest, MakeReadyFuture)
{
    auto future = cu::make_ready_future(42);

    EXPECT_TRUE(cu::is_ready(future));
    EXPECT_EQ(future.get(), 42);
}

TEST(FutureUtilsTest, MakeReadyFutureVoid)
{
    auto future = cu::make_ready_future();

    EXPECT_TRUE(cu::is_ready(future));
    EXPECT_NO_THROW(future.get());
}

TEST(FutureUtilsTest, MakeExceptionalFuture)
{
    auto ex = std::make_exception_ptr(std::runtime_error("Test error"));
    auto future = cu::make_exceptional_future<int>(ex);

    EXPECT_TRUE(cu::is_ready(future));
    EXPECT_THROW(future.get(), std::runtime_error);
}

// ============================================================================
// BATCHING AND PARTIAL RESULTS TESTS
// ============================================================================

TEST(FutureUtilsTest, WaitN)
{
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 5; ++i)
    {
        futures.push_back(std::async(std::launch::async, [i]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
            return i;
        }));
    }

    // Wait for first 3 to complete
    auto results = cu::wait_n(futures, 3);

    EXPECT_EQ(results.size(), 3);

    // Results should be from the first 3 futures (which complete fastest)
    for (const auto& [index, value] : results)
    {
        EXPECT_LT(index, 5);
        EXPECT_EQ(value, index);
    }
}

TEST(FutureUtilsTest, WaitNMoreThanAvailable)
{
    std::vector<std::future<int>> futures;

    futures.push_back(std::async(std::launch::async, []() { return 1; }));
    futures.push_back(std::async(std::launch::async, []() { return 2; }));

    // Request more than available
    auto results = cu::wait_n(futures, 10);

    EXPECT_EQ(results.size(), 2);  // Should get only 2
}

TEST(FutureUtilsTest, CollectReady)
{
    std::vector<std::future<int>> futures;

    // Fast futures
    futures.push_back(cu::make_ready_future(1));
    futures.push_back(cu::make_ready_future(2));

    // Slow future
    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 3;
    }));

    auto results = cu::collect_ready(futures);

    EXPECT_EQ(results.size(), 2);  // Only ready ones
    EXPECT_EQ(results[0].second, 1);
    EXPECT_EQ(results[1].second, 2);
}

// ============================================================================
// QUERY UTILITIES TESTS
// ============================================================================

TEST(FutureUtilsTest, CountReady)
{
    std::vector<std::future<int>> futures;

    futures.push_back(cu::make_ready_future(1));
    futures.push_back(cu::make_ready_future(2));
    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 3;
    }));

    EXPECT_EQ(cu::count_ready(futures), 2);
}

TEST(FutureUtilsTest, AllReady)
{
    std::vector<std::future<int>> futures1;
    futures1.push_back(cu::make_ready_future(1));
    futures1.push_back(cu::make_ready_future(2));

    EXPECT_TRUE(cu::all_ready(futures1));

    std::vector<std::future<int>> futures2;
    futures2.push_back(cu::make_ready_future(1));
    futures2.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 2;
    }));

    EXPECT_FALSE(cu::all_ready(futures2));
}

TEST(FutureUtilsTest, AnyReady)
{
    std::vector<std::future<int>> futures1;
    futures1.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 1;
    }));

    EXPECT_FALSE(cu::any_ready(futures1));

    std::vector<std::future<int>> futures2;
    futures2.push_back(cu::make_ready_future(1));
    futures2.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 2;
    }));

    EXPECT_TRUE(cu::any_ready(futures2));
}

// ============================================================================
// SPECIALIZED PATTERNS TESTS
// ============================================================================

TEST(FutureUtilsTest, RaceFirstSuccessAllSuccess)
{
    std::vector<std::future<int>> futures;

    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 42;
    }));

    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 100;
    }));

    auto result = cu::race_first_success(futures);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(FutureUtilsTest, RaceFirstSuccessWithFailures)
{
    std::vector<std::future<int>> futures;

    // Fast failure
    futures.push_back(std::async(std::launch::async, []() -> int
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        throw std::runtime_error("Fast fail");
    }));

    // Slow success
    futures.push_back(std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 42;
    }));

    auto result = cu::race_first_success(futures);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(FutureUtilsTest, RaceFirstSuccessAllFail)
{
    std::vector<std::future<int>> futures;

    futures.push_back(std::async(std::launch::async, []() -> int
    {
        throw std::runtime_error("Error 1");
    }));

    futures.push_back(std::async(std::launch::async, []() -> int
    {
        throw std::runtime_error("Error 2");
    }));

    auto result = cu::race_first_success(futures);

    EXPECT_FALSE(result.has_value());
}

TEST(FutureUtilsTest, WaitAllWithProgress)
{
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 5; ++i)
    {
        futures.push_back(std::async(std::launch::async, [i]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i * 10;
        }));
    }

    std::vector<std::pair<size_t, size_t>> progress;

    auto results = cu::wait_all_with_progress(futures,
        [&progress](size_t completed, size_t total)
        {
            progress.emplace_back(completed, total);
        });

    EXPECT_EQ(results.size(), 5);
    EXPECT_EQ(progress.size(), 5);

    // Verify progress tracking
    for (size_t i = 0; i < progress.size(); ++i)
    {
        EXPECT_EQ(progress[i].first, i + 1);
        EXPECT_EQ(progress[i].second, 5);
    }
}

TEST(FutureUtilsTest, ThenContinuation)
{
    auto future = std::async(std::launch::async, []()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    });

    auto continuation = cu::then(future, [](int value)
    {
        return value * 2;
    });

    EXPECT_EQ(continuation.get(), 84);
}

TEST(FutureUtilsTest, ThenContinuationVoidResult)
{
    auto future = std::async(std::launch::async, []()
    {
        return 42;
    });

    int captured = 0;
    auto continuation = cu::then(future, [&captured](int value)
    {
        captured = value * 2;
    });

    continuation.get();
    EXPECT_EQ(captured, 84);
}

TEST(FutureUtilsTest, ThenContinuationWithException)
{
    auto future = std::async(std::launch::async, []() -> int
    {
        throw std::runtime_error("Test error");
    });

    auto continuation = cu::then(future, [](int value)
    {
        return value * 2;
    });

    EXPECT_THROW(continuation.get(), std::runtime_error);
}

} // namespace tests