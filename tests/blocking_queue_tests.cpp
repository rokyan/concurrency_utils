#include <gtest/gtest.h>
#include "concurrency/blocking_queue.hpp"
#include <thread>

using namespace cu;

TEST(BlockingQueueTest, PushAndPop) {
    BlockingQueue<int> queue;
    
    queue.push(42);
    auto value = queue.pop();
    
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 42);
}

TEST(BlockingQueueTest, TryPop) {
    BlockingQueue<int> queue;
    
    auto value = queue.try_pop();
    EXPECT_FALSE(value.has_value());
    
    queue.push(10);
    value = queue.try_pop();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 10);
}

TEST(BlockingQueueTest, ProducerConsumer) {
    BlockingQueue<int> queue;
    constexpr int num_items = 100;
    
    std::thread producer([&queue]() {
        for (int i = 0; i < num_items; ++i) {
            queue.push(i);
        }
        queue.close();
    });
    
    std::thread consumer([&queue]() {
        int count = 0;
        while (auto value = queue.pop()) {
            EXPECT_EQ(value.value(), count++);
        }
        EXPECT_EQ(count, num_items);
    });
    
    producer.join();
    consumer.join();
}

TEST(BlockingQueueTest, BoundedQueue) {
    BlockingQueue<int> queue(2);
    
    queue.push(1);
    queue.push(2);
    
    EXPECT_EQ(queue.size(), 2);
}