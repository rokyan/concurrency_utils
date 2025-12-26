#include <gtest/gtest.h>
#include "concurrency/future_utils.hpp"

using namespace cu;

TEST(FutureUtilsTest, WaitAll) {
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [i]() {
            return i * 2;
        }));
    }
    
    auto results = wait_all(futures);
    
    ASSERT_EQ(results.size(), 5);
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_EQ(results[i], i * 2);
    }
}

TEST(FutureUtilsTest, IsReady) {
    auto future = std::async(std::launch::async, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });
    
    // Should not be ready immediately
    EXPECT_FALSE(is_ready(future));
    
    // Wait and check again
    future.wait();
    EXPECT_TRUE(is_ready(future));
}