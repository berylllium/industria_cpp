#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>

template<typename T>
struct FreeList
{
    struct Iterator
    {
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;  // or also value_type*
        using reference         = T&;  // or also value_type&

        Iterator(uint64_t cur_index, uint64_t capacity, T* data, bool* free_indices)
            : cur_index {cur_index}, capacity {capacity}, data {data}, free_indices {free_indices} {}

        reference operator * () { return data[cur_index]; }
        pointer operator -> () { return data + cur_index; }

        // Increment.
        Iterator& operator ++ ()
        {
            // Find next element.
            for (uint64_t i = cur_index + 1; i < capacity; i++)
            {
                if (!free_indices[i])
                {
                    // Found next element.
                    cur_index = i;
                    return *this;
                }
            }

            cur_index = capacity;
            return *this;
        }

        Iterator operator ++ (int)
        {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }
        
        // Decrement.
        Iterator& operator -- ()
        {
            // Find next element.
            for (uint64_t i = cur_index - 1; i >= 0; i--)
            {
                if (!free_indices[i])
                {
                    // Found next element.
                    cur_index = i;
                    return *this;
                }
            }

            cur_index = capacity;
            return *this;
        }

        Iterator operator -- (int)
        {
            Iterator temp = *this;
            --(*this);
            return temp;
        }

        friend bool operator == (const Iterator& a, const Iterator& b) { return a.cur_index == b.cur_index; };
        friend bool operator != (const Iterator& a, const Iterator& b) { return a.cur_index != b.cur_index; };  

    private:
        uint64_t cur_index;
        uint64_t capacity;
        T* data;
        bool* free_indices;
    };

    std::allocator<T> alloc;
    using alloc_traits = std::allocator_traits<decltype(alloc)>;

    uint64_t capacity;
    T* data = nullptr;

    std::unique_ptr<bool[]> free_indices;

    FreeList() = default;

    FreeList(FreeList&) = delete;

    FreeList(FreeList&& other)
    {
        capacity = other.capacity;

        data = other.data;
        other.data = nullptr;

        free_indices.reset(other.free_indices.release());
    }

    ~FreeList()
    {
        if (!data) return;

        // Destroy valid objects.
        for (uint64_t i = 0; i < capacity; i++)
        {
            if (!free_indices[i])
            {
                alloc_traits::destroy(alloc, data + i);
            }
        }

        // Deallocate allocation.
        alloc.deallocate(data, capacity);
    }

    FreeList& operator = (FreeList&) = delete;

    FreeList& operator = (FreeList&& other)
    {
        this->~FreeList();

        capacity = other.capacity;

        data = other.data;
        other.data = nullptr;

        free_indices.reset(other.free_indices.release());

        return *this;
    }
    
    static std::optional<FreeList> create(uint64_t initial_capacity = 4) 
    {
        FreeList out;

        out.capacity = initial_capacity;

        out.data = out.alloc.allocate(out.capacity);

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

        alloc_traits::construct(alloc, data + *insert_idx, element);

        free_indices[*insert_idx] = false;

        return *insert_idx;
    }

    uint64_t insert(T&& element)
    {
        auto insert_idx = find_empty_index();

        if (!insert_idx.has_value())
        {
            grow();
            return insert(std::forward<T>(element));
        }

        alloc_traits::construct(alloc, data + *insert_idx, std::forward<T>(element));

        free_indices[*insert_idx] = false;

        return *insert_idx;
    }

    template<typename... Args>
    uint64_t emplace(Args&&... args)
    {
        auto insert_idx = find_empty_index();

        if (!insert_idx.has_value())
        {
            grow();
            return emplace(std::forward<Args>(args)...);
        }

        alloc_traits::construct(alloc, data + *insert_idx, std::forward<Args>(args)...);

        free_indices[*insert_idx] = false;

        return *insert_idx;
    }

    std::optional<T*> get(uint64_t idx)
    {
        if (free_indices[idx])
        {
            return std::nullopt;
        }

        return &data[idx];
    }

    void free(uint64_t idx)
    {
        
        if (!free_indices[idx])
        {
            alloc_traits::destroy(alloc, data + idx);
        }

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
        uint64_t new_capacity = capacity * 1.5f;

        T* new_data = alloc.allocate(new_capacity);

        std::unique_ptr<bool[]> new_free_indices = std::make_unique<bool[]>(new_capacity);

        std::memcpy(new_data, data, capacity);
        
        std::memset(new_free_indices.get(), true, new_capacity);
        std::memcpy(new_free_indices.get(), free_indices.get(), capacity);

        alloc.deallocate(data, capacity);
        data = new_data;

        capacity = new_capacity;

        new_free_indices.reset(new_free_indices.release());
    }

    Iterator begin() { return Iterator(0, capacity, data, free_indices); }
    Iterator end() { return Iterator(capacity, capacity, data, free_indices); }
};
