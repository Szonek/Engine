#pragma once
#include "engine.h"
#include "logger.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <string>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include <fmt/format.h>

namespace engine
{
// Fixed-size storage of reference counted objects addressed by handle or by name.
// Names are unique and non-empty, so a name identifies exactly one object.
// A handle is the object's slot and the slot's generation, which changes whenever the slot is freed: a handle to an
// object which was released is never valid again, even after its slot is reused (lookups return nothing). Generations
// start at 1, so no handle is 0 (ENGINE_INVALID_OBJECT_HANDLE).
// An object is added with one reference, acquire() adds one and release() drops one; the last release destroys it.
template<typename T, std::size_t SIZE = 1024>
class Atlas
{
    static_assert(std::has_single_bit(SIZE), "the slot is the low bits of a handle");
    static constexpr std::uint32_t SLOT_BITS = static_cast<std::uint32_t>(std::bit_width(SIZE - 1));
    static constexpr std::uint32_t SLOT_MASK = static_cast<std::uint32_t>(SIZE - 1);
    // the generations the high bits of a handle hold; 0 is never used
    static constexpr std::uint64_t GENERATION_COUNT = std::uint64_t{ 1 } << (32 - SLOT_BITS);
    static_assert(ENGINE_INVALID_OBJECT_HANDLE == 0, "a handle with a generation of at least 1 is never the invalid one");

public:
    Atlas()
    {
        free_slots_.resize(SIZE);
        std::generate(free_slots_.begin(), free_slots_.end(), [n = 0]() mutable { return n++; });
        generations_.fill(1);
    }
    Atlas(const Atlas& rhs) = delete;
    Atlas(Atlas&& rhs) = delete;
    Atlas& operator=(const Atlas& rhs) = delete;
    Atlas& operator=(Atlas&& rhs) = delete;
    ~Atlas()
    {
        if (free_slots_.size() != SIZE)
        {
            // print not released objects
            for (std::size_t i = 0; i < SIZE; i++)
            {
                if (occupied_[i])
                {
                    log::log(log::LogLevel::eCritical, fmt::format("Not released object: {} ({} references)\n", names_[i], references_[i]));
                }
            }
            assert(false && "Not all objects were released from atlas");
        }
    }

    // Adds the object with one reference.
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
        if (free_slots_.empty())
        {
            log::log(log::LogLevel::eCritical, fmt::format("Atlas: full, can't add \"{}\".\n", name));
            assert(false && "Atlas is full");
            return ENGINE_INVALID_OBJECT_HANDLE;
        }
        // the slot freed longest ago, so a released handle isn't followed by a new object in its slot right away
        const auto slot = free_slots_.front();
        free_slots_.pop_front();
        objects_[slot] = std::move(t);
        names_[slot] = std::string(name);
        occupied_[slot] = true;
        references_[slot] = 1;
        return make_handle(slot);
    }

    // Adds a reference to the object. Returns false (and logs why) for a handle which isn't valid.
    bool acquire(std::uint32_t handle)
    {
        const auto slot = find_slot(handle);
        if (!slot)
        {
            log::log(log::LogLevel::eError, fmt::format("Atlas: can't acquire {}, it is not a valid handle.\n", handle));
            return false;
        }
        references_[*slot]++;
        return true;
    }

    // Drops a reference to the object and destroys it when it was the last one. Logs an error for a handle which
    // isn't valid (e.g. released one time too many) and does nothing else.
    void release(std::uint32_t handle)
    {
        const auto slot = find_slot(handle);
        if (!slot)
        {
            log::log(log::LogLevel::eError, fmt::format("Atlas: can't release {}, it is not a valid handle.\n", handle));
            return;
        }
        if (--references_[*slot] > 0)
        {
            return;
        }
        // the slot is free before the object is destroyed, so its destructor can release other objects
        T released = std::move(objects_[*slot]);
        objects_[*slot] = T{};
        names_[*slot].clear();
        occupied_[*slot] = false;
        generations_[*slot] = generations_[*slot] + 1 == GENERATION_COUNT ? 1 : generations_[*slot] + 1;
        free_slots_.push_back(*slot);
    }

    // nullptr for a handle which isn't valid (never was, or its object was released)
    const T* get_object(std::uint32_t handle) const
    {
        const auto slot = find_slot(handle);
        return slot ? &objects_[*slot] : nullptr;
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
                return make_handle(i);
            }
        }
        // it's valid not found object - user may want to check if object exists by trying to get it
        return ENGINE_INVALID_OBJECT_HANDLE;
    }
    // "" for a handle which isn't valid
    std::string get_object_name(std::uint32_t handle) const
    {
        const auto slot = find_slot(handle);
        return slot ? names_[*slot] : "";
    }

    // 0 for a handle which isn't valid
    std::uint32_t get_reference_count(std::uint32_t handle) const
    {
        const auto slot = find_slot(handle);
        return slot ? references_[*slot] : 0;
    }

    // handle and name of every object, by slot
    std::vector<std::pair<std::uint32_t, std::string>> get_object_names() const
    {
        std::vector<std::pair<std::uint32_t, std::string>> out;
        for (std::uint32_t i = 0; i < SIZE; i++)
        {
            if (occupied_[i])
            {
                out.emplace_back(make_handle(i), names_[i]);
            }
        }
        return out;
    }

private:
    std::uint32_t make_handle(std::uint32_t slot) const { return (generations_[slot] << SLOT_BITS) | slot; }

    std::optional<std::uint32_t> find_slot(std::uint32_t handle) const
    {
        if (handle == ENGINE_INVALID_OBJECT_HANDLE)
        {
            return std::nullopt;
        }
        const auto slot = handle & SLOT_MASK;
        if (!occupied_[slot] || make_handle(slot) != handle)
        {
            return std::nullopt;
        }
        return slot;
    }

private:
    std::array<T, SIZE> objects_;
    std::array<std::string, SIZE> names_;
    std::array<bool, SIZE> occupied_{};
    std::array<std::uint32_t, SIZE> references_{};
    std::array<std::uint32_t, SIZE> generations_;
    std::deque<std::uint32_t> free_slots_;
};
}
