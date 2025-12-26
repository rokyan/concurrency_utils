#include <gtest/gtest.h>
#include "concurrency/thread_pool.hpp"
#include <numeric>

using namespace cu;

TEST(ThreadPoolTest, BasicExecution) {
    ThreadPool pool(4);
    
    auto future = pool.enqueue([]() {
        return 42;
    });
    
    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, MultipleTasksv) {
    ThreadPool pool(4);
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.enqueue([i]() {
            return i * i;
        }));
    }
    
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(futures[i].get(), i * i);
    }
}

TEST(ThreadPoolTest, WithArguments) {
    ThreadPool pool(2);
    
    auto add = [](int a, int b) { return a + b; };
    auto future = pool.enqueue(add, 5, 3);
    
    EXPECT_EQ(future.get(), 8);
}

TEST(ThreadPoolTest, ThreadCount) {
    ThreadPool pool(4);
    EXPECT_EQ(pool.size(), 4);
}