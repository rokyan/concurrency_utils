#include <gtest/gtest.h>
#include "concurrency/thread_pool.hpp"
#include <numeric>

namespace tests
{

TEST(ThreadPoolTest, BasicExecution)
{
    cu::thread_pool pool{4};
    
    auto future = pool.enqueue([]()
    {
        return 42;
    });
    
    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, MultipleTasksv)
{
    cu::thread_pool pool{4};
    std::vector<std::future<int>> futures;
    
    for (int idx = 0; idx < 10; ++idx)
    {
        futures.push_back(pool.enqueue([idx]()
        {
            return idx * idx;
        }));
    }
    
    for (int idx = 0; idx < 10; ++idx)
    {
        EXPECT_EQ(futures[idx].get(), idx * idx);
    }
}

TEST(ThreadPoolTest, WithArguments)
{
    cu::thread_pool pool{2};

    auto add = [](int a, int b) { return a + b; };
    auto future = pool.enqueue(add, 5, 3);

    EXPECT_EQ(future.get(), 8);
}

TEST(ThreadPoolTest, ThreadCount)
{
    cu::thread_pool pool{4};
    EXPECT_EQ(pool.size(), 4);
}

} // namespace tests