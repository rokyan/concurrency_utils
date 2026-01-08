 #pragma once 

#include <shared_mutex>
#include <optional>
#include <vector>

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
        void add_or_update(const Key& key, V&& value)
        {
            std::unique_lock lock{entries_mutex};

            if (const auto it = find(key); it != entries.end())
            {
                it->second = std::forward<V>(value);
                return;
            }

            entries.emplace_back(key, std::forward<V>(value));
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
    };

    static constexpr size_t DEFAULT_BUCKET_COUNT = 17;

public:
    lookup_table(size_t bucket_count = DEFAULT_BUCKET_COUNT)
        : buckets(bucket_count)
    {
    }

    template<typename V>
    void add_or_update(const Key& key, V&& value)
    {
        const size_t index = get_bucket_index(key);

        std::shared_lock lock{buckets_mutex};
        buckets[index].add_or_update(key, std::forward<V>(value));
    }

    std::optional<Value> get(const Key& key) const
    {
        const size_t index = get_bucket_index(key);

        std::shared_lock lock{buckets_mutex};
        return buckets[index].get(key);
    }

    bool remove(const Key& key)
    {
        const size_t index = get_bucket_index(key);

        std::shared_lock lock{buckets_mutex};
        return buckets[index].remove(key);
    }
 
private:
    size_t get_bucket_index(const Key& key) const
    {
        return hash(key) % buckets.size();
    }

private:
    Hash hash;
    std::vector<bucket> buckets;
    mutable std::shared_mutex buckets_mutex;
};

} // end namespace cc