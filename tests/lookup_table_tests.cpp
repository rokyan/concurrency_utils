#include <gtest/gtest.h>
#include "concurrency/lookup_table.hpp"

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

} // namespace tests
