#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>

template<typename T>
struct FreeList
{
    uint64_t capacity;
    std::unique_ptr<T[]> data;
    std::unique_ptr<bool[]> free_indices;

    FreeList() = default;
    
    static std::optional<FreeList> create(uint64_t initial_capacity = 4) 
    {
        FreeList out;

        out.capacity = initial_capacity;

        out.data = std::make_unique<T[]>(out.capacity);
        out.free_indices = std::make_unique<bool[]>(out.capacity);

        std::memset(out.free_indices.get(), true, out.capacity);

        return out;
    }

    uint64_t insert(T& element)
    {
        auto insert_idx = find_empty_index();

        if (!insert_idx.has_value())
        {
            grow();
            return insert(element);
        }

        data[insert_idx.value()] = element;
        free_indices[insert_idx.value()] = false;

        return insert_idx.value();
    }

    void free(uint64_t idx)
    {
        data[idx].~T();
        free_indices[idx] = true;
    }

    std::optional<uint64_t> find_empty_index()
    {
        for (uint64_t i = 0; i < capacity; i++)
        {
            if (free_indices[i])
            {
                return i;
            }
        }

        return std::nullopt;
    }

    void grow()
    {
        uint64_t new_capacity = capacity * 0.5f;
        std::unique_ptr<T[]> new_data = std::make_unique<T[]>(new_capacity);
        std::unique_ptr<bool[]> new_free_indices = std::make_unique<bool[]>(new_capacity);

        std::memcpy(new_data.get(), data.get(), capacity);
        
        std::memset(new_free_indices.get(), true, new_capacity);
        std::memcpy(new_free_indices.get(), free_indices.get(), capacity);

        data.reset(new_data.release());
        new_free_indices.reset(new_free_indices.release());
    }
};
