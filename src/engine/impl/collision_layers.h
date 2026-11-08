#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "components/physics_layers.h"

// Collision layers (like Unity's layers and layer masks, Godot's collision_layer / collision_mask, Jolt's object
// layers): every collider and character controller is on one layer and has the mask of layers it collides with. Two
// bodies interact when each one's layer is in the other's mask; physics queries see a body when its layer is in the
// query's mask. GL-free and Bullet-free, unit tested; PhysicsWorld applies it.
namespace engine
{
inline constexpr std::uint32_t PHYSICS_LAYER_COUNT = ENGINE_PHYSICS_LAYER_COUNT;
inline constexpr std::uint32_t PHYSICS_LAYER_DEFAULT = ENGINE_PHYSICS_LAYER_DEFAULT;
inline constexpr std::uint32_t ALL_PHYSICS_LAYERS = ENGINE_PHYSICS_ALL_LAYERS;

// the bit of a layer in a mask; none for a layer out of range
constexpr std::uint32_t layer_bit(std::uint32_t layer)
{
    return ENGINE_PHYSICS_LAYER_BIT(layer);
}

// Two bodies given by their layer bits and collides-with masks interact when each one's layer is in the other's mask
// (symmetric). A mask of 0 collides with nothing.
constexpr bool layer_bits_interact(std::uint32_t bit_a, std::uint32_t mask_a, std::uint32_t bit_b, std::uint32_t mask_b)
{
    return (mask_a & bit_b) != 0 && (mask_b & bit_a) != 0;
}

constexpr bool layers_interact(std::uint32_t layer_a, std::uint32_t collides_with_a, std::uint32_t layer_b, std::uint32_t collides_with_b)
{
    return layer_bits_interact(layer_bit(layer_a), collides_with_a, layer_bit(layer_b), collides_with_b);
}

// Whether a query (ray cast, overlap) with `query_mask` sees a body whose layer bit is `bit`: a mask of 0 is no layer
// filter (every layer). The body's own collides-with mask doesn't matter to queries.
constexpr bool query_sees_bit(std::uint32_t query_mask, std::uint32_t bit)
{
    return query_mask == 0 || (query_mask & bit) != 0;
}

constexpr bool query_sees(std::uint32_t query_mask, std::uint32_t layer)
{
    return query_sees_bit(query_mask, layer_bit(layer));
}

// how a body moves, for the fixed rule below
enum class BodyMotion
{
    eStatic,
    eKinematic,
    eDynamic,
};

// The fixed rule checked with the layers: a pair of bodies neither of which the simulation moves, and one of which is
// static (static with static, kinematic with static), never interacts: no contact could do anything (Bullet and
// Unity skip these pairs too). Kinematic bodies interact with each other (hitboxes see characters).
constexpr bool body_motions_interact(BodyMotion a, BodyMotion b)
{
    if (a == BodyMotion::eDynamic || b == BodyMotion::eDynamic)
    {
        return true;
    }
    return a == BodyMotion::eKinematic && b == BodyMotion::eKinematic;
}

// The names of the layers, application-wide (the game names its layers once at startup; the editor shows them).
// Layer 0 is "Default"; the others start unnamed ("").
class PhysicsLayerNames
{
public:
    PhysicsLayerNames() { names_[PHYSICS_LAYER_DEFAULT] = "Default"; }

    // false (nothing changed) for a layer out of range
    bool set(std::uint32_t layer, std::string_view name)
    {
        if (layer >= PHYSICS_LAYER_COUNT)
        {
            return false;
        }
        names_[layer] = std::string(name);
        return true;
    }

    // "" for an unnamed layer, nullptr for a layer out of range
    const char* get(std::uint32_t layer) const
    {
        return layer < PHYSICS_LAYER_COUNT ? names_[layer].c_str() : nullptr;
    }

private:
    std::array<std::string, PHYSICS_LAYER_COUNT> names_;
};
}  // namespace engine
