#pragma once
#include "engine.h" 

#include <array>
#include <cstdint>
#include <string>
#include <deque>

namespace engine
{
template<typename T, std::size_t SIZE = 1024>
class Atlas
{
public:
    Atlas()
    {
        free_indices_.resize(SIZE);
        std::generate(free_indices_.begin(), free_indices_.end(), [n = 0]() mutable { return n++; });
    }
    Atlas(const Atlas& rhs) = delete;
    Atlas(Atlas&& rhs) = delete;
    Atlas& operator=(const Atlas& rhs) = delete;
    Atlas& operator=(Atlas&& rhs) = delete;
    ~Atlas()
    {
        if (free_indices_.size() != SIZE)
        {
            assert(false && "Not all objects were removed from atlas");
        }
    }

    std::uint32_t add_object(std::string_view name, T&& t)
    {
        if (free_indices_.empty())
        {
            assert(false && "Atlas is full");
            return ENGINE_INVALID_OBJECT_HANDLE;
        }
        const auto current_idx_ = free_indices_.front();
        objects_[current_idx_] = std::move(t);
        names_[current_idx_] = name.data();
        free_indices_.pop_front();
        return current_idx_;
    }

    void remove_object(std::uint32_t idx)
    {
        objects_[idx] = T{};
        names_[idx] = "";
        free_indices_.push_front(idx);
    }

    void remove_object(std::string_view name)
    {
        for (std::uint32_t i = 0; const auto & n : names_)
        {
            if (n.compare(name) == 0)
            {
                remove_object(i);
                break;
            }
            i++;
        }
        assert(false && "Object not found");
    }

    const T* get_object(std::uint32_t idx) const
    {
        if (idx >= SIZE)
        {
            assert(false && "Index out of bounds");
            return nullptr;
        }
        if (names_[idx].empty())
        {
            // it's valid not found object - user may want to check if object exists by trying to get it
            return nullptr;
        }
        return &objects_[idx];
    }

    std::uint32_t get_object(std::string_view name) const
    {
        for (std::uint32_t i = 0; const auto & n : names_)
        {
            if (n.compare(name) == 0)
            {
                return i;
            }
            i++;
        }
        // it's valid not found object - user may want to check if object exists by trying to get it
        return ENGINE_INVALID_OBJECT_HANDLE;
    }

    const std::array<T, SIZE>& get_objects_view() const { return objects_; }
private:
    std::array<T, SIZE> objects_;
    std::array<std::string, SIZE> names_;
    std::deque<std::uint32_t> free_indices_;
};
}