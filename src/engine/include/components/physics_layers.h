#pragma once

// Collision layers (like Unity's layers, Godot's collision_layer / collision_mask): every collider and character
// controller is on one layer (0..ENGINE_PHYSICS_LAYER_COUNT-1) and has a collides-with mask, one bit per layer. Two
// bodies interact (collide, or a trigger reports the other) only when each one's layer is in the other's mask; a mask
// of 0 collides with nothing. A new component is on ENGINE_PHYSICS_LAYER_DEFAULT and collides with every layer.
// Physics queries take a layer mask of their own (engine_physics_query_filter_t). Names: enginePhysicsSetLayerName.
#define ENGINE_PHYSICS_LAYER_COUNT 32
#define ENGINE_PHYSICS_LAYER_DEFAULT 0
// a mask of every layer (what a new component collides with)
#define ENGINE_PHYSICS_ALL_LAYERS 0xFFFFFFFFu
// a layer's bit in a mask; 0 for a layer out of range
#define ENGINE_PHYSICS_LAYER_BIT(layer) ((layer) < ENGINE_PHYSICS_LAYER_COUNT ? (1u << (layer)) : 0u)
