include(${PROJECT_SOURCE_DIR}/cmake/GoogleTest.cmake)
include(GoogleTest)

# Test sources
set(TEST_SOURCES
    thread_pool_tests.cpp
    blocking_queue_tests.cpp
    future_utils_tests.cpp
)

# Create test executable
add_executable(concurrency_tests ${TEST_SOURCES} "lookup_table_tests.cpp")

target_link_libraries(concurrency_tests
    PRIVATE
        ${PROJECT_NAME}
        GTest::gtest
        GTest::gtest_main
)

# Discover tests
gtest_discover_tests(concurrency_tests)