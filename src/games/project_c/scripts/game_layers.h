#pragma once
#include <engine.h>

#include <cstdint>

namespace project_c
{
// The game's collision layers (the engine's physics_layers.h: a layer per collider and character controller, and the
// layers it collides with). Named by AppProjectC at startup (register_layer_names), set by LevelBuilder and the scripts
// spawning objects; scene files save the numbers.
enum class Layer : std::uint32_t
{
    // anything not sorted into one of the others
    eDefault = ENGINE_PHYSICS_LAYER_DEFAULT,
    // floors, walls, obstacles, the house's parts, chests: what blocks sight and plunges
    eLevelGeometry = 1,
    // the player's and the enemies' character controllers
    eCharacter = 2,
    // attack triggers and enemy blades: they only look for characters
    eHitbox = 3,
    // coins and weapons on the ground: they land on the level and on each other, and are picked up by characters
    ePickup = 4,
};

constexpr std::uint32_t layer_bit(Layer layer)
{
    return ENGINE_PHYSICS_LAYER_BIT(static_cast<std::uint32_t>(layer));
}

// the layers a body on `layer` collides with
constexpr std::uint32_t collides_with(Layer layer)
{
    switch (layer)
    {
    case Layer::eHitbox:
        return layer_bit(Layer::eCharacter);
    case Layer::ePickup:
        return layer_bit(Layer::eLevelGeometry) | layer_bit(Layer::eCharacter) | layer_bit(Layer::ePickup);
    case Layer::eDefault:
    case Layer::eLevelGeometry:
    case Layer::eCharacter:
        break;
    }
    return ENGINE_PHYSICS_ALL_LAYERS;
}

// What blocks sight, reachability and the plunge: every layer but the characters', the hitboxes' and the pickups', so
// a collider left on Default (a new one in the editor) blocks like level geometry.
constexpr std::uint32_t BLOCKING_LAYERS = ENGINE_PHYSICS_ALL_LAYERS
    & ~(layer_bit(Layer::eCharacter) | layer_bit(Layer::eHitbox) | layer_bit(Layer::ePickup));

// puts a collider or character controller component (before it is added or updated) on `layer`
template<typename Component>
void set_layer(Component& component, Layer layer)
{
    component.layer = static_cast<std::uint32_t>(layer);
    component.collides_with = collides_with(layer);
}

// the names the editor shows (the engine names layer 0 "Default")
inline void register_layer_names()
{
    enginePhysicsSetLayerName(static_cast<std::uint32_t>(Layer::eLevelGeometry), "LevelGeometry");
    enginePhysicsSetLayerName(static_cast<std::uint32_t>(Layer::eCharacter), "Character");
    enginePhysicsSetLayerName(static_cast<std::uint32_t>(Layer::eHitbox), "Hitbox");
    enginePhysicsSetLayerName(static_cast<std::uint32_t>(Layer::ePickup), "Pickup");
}
}  // namespace project_c
