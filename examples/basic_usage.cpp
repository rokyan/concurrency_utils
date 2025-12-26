#include <iostream>
#include "concurrency/thread_pool.hpp"
#include "concurrency/blocking_queue.hpp"

int main() {
    // Thread Pool example
    cu::thread_pool pool(4);
    
    std::vector<std::future<int>> results;
    for (int i = 0; i < 10; ++i) {
        results.push_back(pool.enqueue([i]() {
            return i * i;
        }));
    }
    
    for (auto& result : results) {
        std::cout << "Result: " << result.get() << std::endl;
    }
    
    // Blocking Queue example
    cu::blocking_queue<int> queue;
    queue.push(42);
    
    if (auto value = queue.pop()) {
        std::cout << "Popped: " << value.value() << std::endl;
    }
    
    return 0;
}