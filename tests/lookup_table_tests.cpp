#include <gtest/gtest.h>
#include "concurrency/lookup_table.hpp"
#include <thread>
#include <vector>

namespace tests
{

TEST(LookupTableTest, AddOrUpdateAndGet)
{
    cu::lookup_table<int, std::string> table;

    table.add_or_update(1, "one");
    table.add_or_update(2, "two");
    table.add_or_update(1, "uno");

    auto value1 = table.get(1);
    auto value2 = table.get(2);
    auto value3 = table.get(3);

    ASSERT_TRUE(value1.has_value());
    EXPECT_EQ(value1.value(), "uno");

    ASSERT_TRUE(value2.has_value());
    EXPECT_EQ(value2.value(), "two");

    EXPECT_FALSE(value3.has_value());
}

TEST(LookupTableTest, AddOrUpdateAndRemove)
{
    cu::lookup_table<int, std::string> table;

    table.add_or_update(1, "one");
    table.add_or_update(2, "two");

    ASSERT_TRUE(table.remove(1));
    ASSERT_FALSE(table.remove(3));
}

TEST(LookupTableTest, ManualResize)
{
    cu::lookup_table<int, std::string> table(5);

    table.add_or_update(1, "one");
    table.add_or_update(2, "two");

    EXPECT_EQ(table.bucket_count(), 5);
    EXPECT_EQ(table.size(), 2);

    table.resize(10);

    EXPECT_EQ(table.bucket_count(), 10);
    EXPECT_EQ(table.size(), 2);

    // Verify entries still accessible after resize
    auto value1 = table.get(1);
    auto value2 = table.get(2);

    ASSERT_TRUE(value1.has_value());
    EXPECT_EQ(value1.value(), "one");

    ASSERT_TRUE(value2.has_value());
    EXPECT_EQ(value2.value(), "two");
}

TEST(LookupTableTest, AutomaticResize)
{
    // Start with 2 buckets and threshold 0.5
    // After adding 1 element: load_factor = 1/2 = 0.5 (not > 0.5, no resize)
    // After adding 2nd element: load_factor = 2/2 = 1.0 (> 0.5, triggers resize!)
    cu::lookup_table<int, std::string> table(2, 0.5f);

    EXPECT_EQ(table.bucket_count(), 2);
    EXPECT_FLOAT_EQ(table.load_factor(), 0.0f);

    // Add first element - should NOT trigger resize
    table.add_or_update(1, "one");
    EXPECT_EQ(table.bucket_count(), 2);  // No resize yet
    EXPECT_FLOAT_EQ(table.load_factor(), 0.5f);  // Exactly at threshold

    // Add second element - should trigger resize
    table.add_or_update(2, "two");
    EXPECT_FLOAT_EQ(table.load_factor(), 0.5f);  // 2/4 after resize to 4 buckets
    EXPECT_GT(table.bucket_count(), 2);  // Should be resized to 4
    EXPECT_EQ(table.size(), 2);

    // Verify entries still accessible after resize
    auto val1 = table.get(1);
    auto val2 = table.get(2);
    ASSERT_TRUE(val1.has_value());
    EXPECT_EQ(val1.value(), "one");
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(val2.value(), "two");
}

TEST(LookupTableTest, LoadFactorCalculation)
{
    cu::lookup_table<int, std::string> table(10);

    EXPECT_FLOAT_EQ(table.load_factor(), 0.0f);

    for (int i = 0; i < 5; ++i)
    {
        table.add_or_update(i, "value");
    }

    EXPECT_FLOAT_EQ(table.load_factor(), 0.5f);

    for (int i = 5; i < 10; ++i)
    {
        table.add_or_update(i, "value");
    }

    EXPECT_FLOAT_EQ(table.load_factor(), 1.0f);
}

TEST(LookupTableTest, SizeTrackingWithUpdates)
{
    cu::lookup_table<int, std::string> table;

    table.add_or_update(1, "one");
    EXPECT_EQ(table.size(), 1);

    table.add_or_update(1, "uno");  // Update, not add
    EXPECT_EQ(table.size(), 1);

    table.add_or_update(2, "two");
    EXPECT_EQ(table.size(), 2);

    table.remove(1);
    EXPECT_EQ(table.size(), 1);

    table.remove(999);  // Non-existent
    EXPECT_EQ(table.size(), 1);
}

TEST(LookupTableTest, ConcurrentAddAndResize)
{
    cu::lookup_table<int, int> table(10);

    const int num_threads = 4;
    const int adds_per_thread = 100;

    std::vector<std::thread> threads;

    // Launch threads that add entries
    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&table, t, adds_per_thread]() {
            for (int i = 0; i < adds_per_thread; ++i)
            {
                table.add_or_update(t * adds_per_thread + i, i);
            }
        });
    }

    // Wait for completion
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Verify all entries present
    EXPECT_EQ(table.size(), num_threads * adds_per_thread);

    for (int t = 0; t < num_threads; ++t)
    {
        for (int i = 0; i < adds_per_thread; ++i)
        {
            auto value = table.get(t * adds_per_thread + i);
            ASSERT_TRUE(value.has_value());
            EXPECT_EQ(value.value(), i);
        }
    }
}

TEST(LookupTableTest, ConcurrentManualResize)
{
    // Use high threshold (10.0) to disable automatic resizing during data population
    cu::lookup_table<int, std::string> table(5, 10.0f);

    // Add some data (won't trigger automatic resize with threshold 10.0)
    for (int i = 0; i < 10; ++i)
    {
        table.add_or_update(i, "value" + std::to_string(i));
    }

    // Verify no automatic resize happened
    EXPECT_EQ(table.bucket_count(), 5);
    EXPECT_EQ(table.size(), 10);

    std::vector<std::thread> threads;

    // Launch threads that resize
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&table]() {
            table.resize(100);
        });
    }

    // Wait for completion
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Verify final state
    EXPECT_EQ(table.bucket_count(), 100);
    EXPECT_EQ(table.size(), 10);

    // Verify data integrity
    for (int i = 0; i < 10; ++i)
    {
        auto value = table.get(i);
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), "value" + std::to_string(i));
    }
}

TEST(LookupTableTest, ResizeDuringHeavyLoad)
{
    // Use very high threshold (100.0) to disable automatic resizing
    // We want to test manual resize under load, not automatic resizing
    cu::lookup_table<int, int> table(10, 100.0f);

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Writer threads
    for (int t = 0; t < 2; ++t)
    {
        threads.emplace_back([&table, &stop, t]() {
            int counter = t * 1000;
            while (!stop.load())
            {
                table.add_or_update(counter++, counter);
                std::this_thread::yield();
            }
        });
    }

    // Reader threads
    for (int t = 0; t < 2; ++t)
    {
        threads.emplace_back([&table, &stop]() {
            while (!stop.load())
            {
                table.get(std::rand() % 1000);
                std::this_thread::yield();
            }
        });
    }

    // Resizer thread - performs manual resize and stops the test
    threads.emplace_back([&table, &stop]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        table.resize(100);
        stop.store(true);
    });

    // Wait for completion
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Verify no crashes, reasonable state, and that manual resize succeeded
    EXPECT_GT(table.size(), 0);
    EXPECT_EQ(table.bucket_count(), 100);
}

TEST(LookupTableTest, ResizeToInvalidSizes)
{
    cu::lookup_table<int, std::string> table(10);

    table.add_or_update(1, "one");

    size_t original_bucket_count = table.bucket_count();

    // Resize to 0 should be ignored
    table.resize(0);
    EXPECT_EQ(table.bucket_count(), original_bucket_count);

    // Data should remain intact
    EXPECT_TRUE(table.get(1).has_value());
}

} // namespace tests
