 #pragma once

#include <shared_mutex>
#include <optional>
#include <vector>
#include <atomic>
#include <algorithm>

namespace cu
{

template<typename Key, typename Value, typename Hash = std::hash<Key>>
class lookup_table final
{
private:
    class bucket final
    {
    private:
        using entry_type = std::pair<Key, Value>;

    public:
        template<typename V>
        bool add_or_update(const Key& key, V&& value)
        {
            std::unique_lock lock{entries_mutex};

            if (const auto it = find(key); it != entries.end())
            {
                it->second = std::forward<V>(value);
                return false;
            }

            entries.emplace_back(key, std::forward<V>(value));
            return true;
        }

        std::optional<Value> get(const Key& key) const
        {
            std::shared_lock lock{entries_mutex};

            if (const auto it = find(key); it != entries.cend())
            {
                return it->second;
            }

            return std::nullopt;
        }

        bool remove(const Key& key)
        {
            std::unique_lock lock{entries_mutex};

            const auto it = find(key);

            if (it != entries.end())
            {
                entries.erase(it);
                return true;
            }

            return false;
        }

        size_t size() const noexcept
        {
            return entries.size();
        }

    private:
        std::vector<entry_type>::iterator find(const Key& key)
        {
            return std::find_if(entries.begin(), entries.end(), [&key](const entry_type& entry) {
                return entry.first == key;
            });
        }

        std::vector<entry_type>::const_iterator find(const Key& key) const
        {
            return std::find_if(entries.cbegin(), entries.cend(), [&key](const entry_type& entry) {
                return entry.first == key;
            });
        }

    private:
        std::vector<entry_type> entries;
        mutable std::shared_mutex entries_mutex;

        friend class lookup_table<Key, Value, Hash>;
    };

    static constexpr size_t DEFAULT_BUCKET_COUNT = 17;
    static constexpr float DEFAULT_LOAD_FACTOR_THRESHOLD = 1.0f;

public:
    lookup_table(size_t bucket_count = DEFAULT_BUCKET_COUNT,
        float load_factor_threshold = DEFAULT_LOAD_FACTOR_THRESHOLD)
        : buckets(std::max(bucket_count, size_t(1)))
        , entry_count{0}
        , load_factor_threshold{load_factor_threshold > 0.0f ? load_factor_threshold : DEFAULT_LOAD_FACTOR_THRESHOLD}
    {
    }

    template<typename V>
    void add_or_update(const Key& key, V&& value)
    {
        {
            std::shared_lock lock{buckets_mutex};
            const size_t index = get_bucket_index(key);

            const bool added_or_updated = buckets[index].add_or_update(key, std::forward<V>(value));

            if (added_or_updated)
            {
                entry_count.fetch_add(1, std::memory_order_release);
            }
        }

        if (should_resize())
        {
            const size_t new_count = calculate_new_bucket_count(buckets.size());
            resize_internal(new_count);
        }
    }

    std::optional<Value> get(const Key& key) const
    {
        std::shared_lock lock{buckets_mutex};
        const size_t index = get_bucket_index(key);
        return buckets[index].get(key);
    }

    bool remove(const Key& key)
    {
        bool removed = false;

        {
            std::shared_lock lock{buckets_mutex};
            const size_t index = get_bucket_index(key);

            removed = buckets[index].remove(key);

            if (removed)
            {
                entry_count.fetch_sub(1, std::memory_order_release);
            }
        }

        return removed;
    }

    void resize(size_t new_bucket_count)
    {
        if (new_bucket_count == 0)
        {
            return;
        }

        resize_internal(new_bucket_count);
    }

    size_t size() const noexcept
    {
        return entry_count.load(std::memory_order_acquire);
    }

    size_t bucket_count() const noexcept
    {
        std::shared_lock lock{buckets_mutex};
        return buckets.size();
    }

    float load_factor() const noexcept
    {
        return calculate_load_factor();
    }

private:
    size_t get_bucket_index(const Key& key) const
    {
        return hash(key) % buckets.size();
    }

    float calculate_load_factor() const noexcept
    {
        const size_t bucket_count = buckets.size();

        if (bucket_count == 0)
        {
            return 0.0f;
        }

        return static_cast<float>(entry_count.load(std::memory_order_acquire)) /
           static_cast<float>(bucket_count);
    }

    bool should_resize() const noexcept
    {
        return calculate_load_factor() > load_factor_threshold;
    }

    static size_t calculate_new_bucket_count(size_t current_count) noexcept
    {
        // Simple 2x growth strategy.
        return current_count * 2;
    }

    void resize_internal(size_t new_bucket_count)
    {
        // Acquire exclusive lock - blocks ALL operations.
        std::unique_lock lock{buckets_mutex};

        // Double-check (another thread may have resized).
        if (new_bucket_count == buckets.size())
        {
            return;
        }

        // Create new bucket array.
        std::vector<bucket> new_buckets(new_bucket_count);

        // Rehash all entries (no bucket locks needed - we have exclusive access).
        for (auto& old_bucket : buckets)
        {
            for (const auto& entry : old_bucket.entries)
            {
                const size_t new_index = hash(entry.first) % new_bucket_count;
                new_buckets[new_index].add_or_update(entry.first, entry.second);
            }
        }

        // Atomic swap.
        buckets = std::move(new_buckets);
    }

private:
    Hash hash;
    std::vector<bucket> buckets;
    mutable std::shared_mutex buckets_mutex;
    std::atomic<size_t> entry_count{0};
    float load_factor_threshold{DEFAULT_LOAD_FACTOR_THRESHOLD};
};

} // end namespace cc