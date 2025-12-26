#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace cu
{

template<typename T>
class blocking_queue
{
public:
    explicit blocking_queue(std::size_t max_size = 0)
        : max_size{max_size}
    {}

    void push(T value);
    std::optional<T> pop();
    std::optional<T> try_pop();
    
    void close();
    bool is_closed() const noexcept;

    std::size_t size() const noexcept;
    bool empty() const noexcept;

private:
    mutable std::mutex mutex;
    std::condition_variable not_empty;
    std::condition_variable not_full;
    std::queue<T> queue;
    std::size_t max_size;
    bool closed{false};
};

template<typename T>
void blocking_queue<T>::push(T value)
{
    std::unique_lock lock(mutex);
    
    if (max_size > 0)
    {
        not_full.wait(lock, [this]
        {
            return queue.size() < max_size || closed;
        });
    }

    if (closed)
    {
        throw std::runtime_error("push on closed queue");
    }

    queue.push(std::move(value));
    not_empty.notify_one();
}

template<typename T>
std::optional<T> blocking_queue<T>::pop()
{
    std::unique_lock lock(mutex);

    not_empty.wait(lock, [this]
    {
        return !queue.empty() || closed;
    });
    
    if (queue.empty())
    {
        return std::nullopt;
    }
    
    T value = std::move(queue.front());
    queue.pop();
    not_full.notify_one();
    
    return value;
}

template<typename T>
std::optional<T> blocking_queue<T>::try_pop()
{
    std::lock_guard lock(mutex);
    
    if (queue.empty())
    {
        return std::nullopt;
    }

    T value = std::move(queue.front());
    queue.pop();
    not_full.notify_one();

    return value;
}

template<typename T>
void blocking_queue<T>::close()
{
    std::lock_guard lock(mutex);

    closed = true;

    not_empty.notify_all();
    not_full.notify_all();
}

template<typename T>
bool blocking_queue<T>::is_closed() const noexcept
{
    std::lock_guard lock(mutex);
    return closed;
}

template<typename T>
size_t blocking_queue<T>::size() const noexcept
{
    std::lock_guard lock(mutex);
    return queue.size();
}

template<typename T>
bool blocking_queue<T>::empty() const noexcept
{
    std::lock_guard lock(mutex);
    return queue.empty();
}

} // namespace cu