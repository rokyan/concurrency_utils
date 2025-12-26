#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace cu {

template<typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(size_t max_size = 0) 
        : max_size_(max_size) {}

    void push(T value);
    std::optional<T> pop();
    std::optional<T> try_pop();
    
    void close();
    bool is_closed() const noexcept;
    size_t size() const noexcept;
    bool empty() const noexcept;

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> queue_;
    size_t max_size_;
    bool closed_ = false;
};

template<typename T>
void BlockingQueue<T>::push(T value) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (max_size_ > 0) {
        not_full_.wait(lock, [this] { 
            return queue_.size() < max_size_ || closed_; 
        });
    }
    
    if (closed_) {
        throw std::runtime_error("push on closed queue");
    }
    
    queue_.push(std::move(value));
    not_empty_.notify_one();
}

template<typename T>
std::optional<T> BlockingQueue<T>::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [this] { 
        return !queue_.empty() || closed_; 
    });
    
    if (queue_.empty()) {
        return std::nullopt;
    }
    
    T value = std::move(queue_.front());
    queue_.pop();
    not_full_.notify_one();
    
    return value;
}

template<typename T>
std::optional<T> BlockingQueue<T>::try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.empty()) {
        return std::nullopt;
    }
    
    T value = std::move(queue_.front());
    queue_.pop();
    not_full_.notify_one();
    
    return value;
}

template<typename T>
void BlockingQueue<T>::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
}

template<typename T>
bool BlockingQueue<T>::is_closed() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
}

template<typename T>
size_t BlockingQueue<T>::size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

template<typename T>
bool BlockingQueue<T>::empty() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

} // namespace concurrency