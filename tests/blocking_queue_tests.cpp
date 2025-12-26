#include <gtest/gtest.h>
#include "concurrency/blocking_queue.hpp"
#include <thread>

namespace tests
{

TEST(BlockingQueueTest, PushAndPop)
{
    cu::blocking_queue<int> queue;

    queue.push(42);
    auto value = queue.pop();

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 42);
}

TEST(BlockingQueueTest, TryPop)
{
    cu::blocking_queue<int> queue;

    auto value = queue.try_pop();
    EXPECT_FALSE(value.has_value());

    queue.push(10);
    value = queue.try_pop();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 10);
}

TEST(BlockingQueueTest, ProducerConsumer)
{
    cu::blocking_queue<int> queue;
    constexpr int num_items = 100;

    std::thread producer([&queue]()
    {
        for (int idx = 0; idx < num_items; ++idx)
        {
            queue.push(idx);
        }
        queue.close();
    });

    std::thread consumer([&queue]()
    {
        int count = 0;

        while (auto value = queue.pop())
        {
            EXPECT_EQ(value.value(), count++);
        }

        EXPECT_EQ(count, num_items);
    });

    producer.join();
    consumer.join();
}

TEST(BlockingQueueTest, BoundedQueue)
{
    cu::blocking_queue<int> queue(2);

    queue.push(1);
    queue.push(2);

    EXPECT_EQ(queue.size(), 2);
}

} // namespace tests