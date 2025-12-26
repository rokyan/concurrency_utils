#include <gtest/gtest.h>
#include "concurrency/future_utils.hpp"

namespace tests
{

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

} // namespace tests