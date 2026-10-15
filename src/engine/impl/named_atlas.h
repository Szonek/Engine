#pragma once
#include "engine.h"
#include "logger.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <deque>

#include <fmt/format.h>

namespace engine
{
// Fixed-size storage of objects addressed by index or by name.
// Names are unique and non-empty, so a name identifies exactly one object.
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
            // print not removed objects
            for (std::size_t i = 0; i < SIZE; i++)
            {
                if (occupied_[i])
                {
                    log::log(log::LogLevel::eCritical, fmt::format("Not released object: {}\n", names_[i]));
                }
            }
            assert(false && "Not all objects were removed from atlas");
        }
    }

    // Returns ENGINE_INVALID_OBJECT_HANDLE (and logs why) when the name is empty, already used or the atlas is full.
    std::uint32_t add_object(std::string_view name, T&& t)
    {
        if (name.empty())
        {
            log::log(log::LogLevel::eError, "Atlas: can't add an object with an empty name.\n");
            return ENGINE_INVALID_OBJECT_HANDLE;
        }
        if (get_object(name) != ENGINE_INVALID_OBJECT_HANDLE)
        {
            log::log(log::LogLevel::eError, fmt::format("Atlas: an object named \"{}\" already exists. Names must be unique.\n", name));
            return ENGINE_INVALID_OBJECT_HANDLE;
        }
        if (free_indices_.empty())
        {
            log::log(log::LogLevel::eCritical, fmt::format("Atlas: full, can't add \"{}\".\n", name));
            assert(false && "Atlas is full");
            return ENGINE_INVALID_OBJECT_HANDLE;
        }
        const auto current_idx_ = free_indices_.front();
        objects_[current_idx_] = std::move(t);
        names_[current_idx_] = std::string(name);
        occupied_[current_idx_] = true;
        free_indices_.pop_front();
        return current_idx_;
    }

    void remove_object(std::uint32_t idx)
    {
        if (idx >= SIZE || !occupied_[idx])
        {
            assert(false && "Object not found");
            return;
        }
        objects_[idx] = T{};
        names_[idx].clear();
        occupied_[idx] = false;
        free_indices_.push_front(idx);
    }

    void remove_object(std::string_view name)
    {
        const auto idx = get_object(name);
        if (idx == ENGINE_INVALID_OBJECT_HANDLE)
        {
            assert(false && "Object not found");
            return;
        }
        remove_object(idx);
    }

    const T* get_object(std::uint32_t idx) const
    {
        if (idx >= SIZE)
        {
            assert(false && "Index out of bounds");
            return nullptr;
        }
        if (!occupied_[idx])
        {
            // it's valid not found object - user may want to check if object exists by trying to get it
            return nullptr;
        }
        return &objects_[idx];
    }

    std::uint32_t get_object(std::string_view name) const
    {
        if (name.empty())
        {
            return ENGINE_INVALID_OBJECT_HANDLE;
        }
        for (std::uint32_t i = 0; i < SIZE; i++)
        {
            if (occupied_[i] && names_[i] == name)
            {
                return i;
            }
        }
        // it's valid not found object - user may want to check if object exists by trying to get it
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    std::string get_object_name(std::uint32_t idx) const
    {
        if (idx >= SIZE)
        {
            assert(false && "Index out of bounds");
            return "";
        }
        return names_[idx];
    }


    const std::array<T, SIZE>& get_objects_view() const { return objects_; }
    std::array<T, SIZE>& get_objects_view() { return objects_; }
private:
    std::array<T, SIZE> objects_;
    std::array<std::string, SIZE> names_;
    std::array<bool, SIZE> occupied_{};
    std::deque<std::uint32_t> free_indices_;
};
}
