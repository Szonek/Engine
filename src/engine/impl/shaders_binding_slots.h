#pragma once
#include <map>
#include <string>
#include <cassert>

enum class ShaderBindingSlot
{
    CAMERA = 0,
    SCENE = 1,
    LIGHTS = 2,
    SKINNING = 3,
    VIEW_LIGHTING = 4,
    //
    TEXTURE_DIFFUSE = 5,
    TEXTURE_SPECULAR = 6,
    TEXTURE_SHADOW_MAP = 7,
    TEXTURE_SSAO = 8,
    //

    DEBUG_PHYSICS = 9,
    PARTICLES = 10,
    // the particles' texture, and the copy of the scene's depth soft particles fade against
    TEXTURE_PARTICLE = 11,
    TEXTURE_SCENE_DEPTH = 12
};

inline constexpr std::array<ShaderBindingSlot, 13> kAllShaderBindingSlots{
    ShaderBindingSlot::CAMERA,
    ShaderBindingSlot::SCENE,
    ShaderBindingSlot::LIGHTS,
    ShaderBindingSlot::SKINNING,
    ShaderBindingSlot::VIEW_LIGHTING,
    ShaderBindingSlot::TEXTURE_DIFFUSE,
    ShaderBindingSlot::TEXTURE_SPECULAR,
    ShaderBindingSlot::TEXTURE_SHADOW_MAP,
    ShaderBindingSlot::TEXTURE_SSAO,
    ShaderBindingSlot::DEBUG_PHYSICS,
    ShaderBindingSlot::PARTICLES,
    ShaderBindingSlot::TEXTURE_PARTICLE,
    ShaderBindingSlot::TEXTURE_SCENE_DEPTH
};

inline std::string shader_binding_slot_to_str(ShaderBindingSlot s)
{
    switch (s)
    {
    case ShaderBindingSlot::CAMERA:
        return "ENGINE_BINDING_SLOT_CAMERA_DATA";
    case ShaderBindingSlot::SCENE:
        return "ENGINE_BINDING_SLOT_SCENE_DATA";
    case ShaderBindingSlot::LIGHTS:
        return "ENGINE_BINDING_SLOT_LIGHTS_DATA";
    case ShaderBindingSlot::SKINNING:
        return "ENGINE_BINDING_SLOT_SKINNING_DATA";
    case ShaderBindingSlot::VIEW_LIGHTING:
        return "ENGINE_BINDING_SLOT_VIEW_LIGHTING_DATA";
    case ShaderBindingSlot::TEXTURE_DIFFUSE:
        return "ENGINE_BINDING_SLOT_TEXTURE_DIFFUSE";
    case ShaderBindingSlot::TEXTURE_SPECULAR:
        return "ENGINE_BINDING_SLOT_TEXTURE_SPECULAR";
    case ShaderBindingSlot::TEXTURE_SHADOW_MAP:
        return "ENGINE_BINDING_SLOT_TEXTURE_SHADOW_MAP";
    case ShaderBindingSlot::TEXTURE_SSAO:
        return "ENGINE_BINDING_SLOT_TEXTURE_SSAO";
    case ShaderBindingSlot::DEBUG_PHYSICS:
        return "ENGINE_BINDING_SLOT_DEBUG_PHYSICS";
    case ShaderBindingSlot::PARTICLES:
        return "ENGINE_BINDING_SLOT_PARTICLES";
    case ShaderBindingSlot::TEXTURE_PARTICLE:
        return "ENGINE_BINDING_SLOT_TEXTURE_PARTICLE";
    case ShaderBindingSlot::TEXTURE_SCENE_DEPTH:
        return "ENGINE_BINDING_SLOT_TEXTURE_SCENE_DEPTH";
    default:
        assert(false && "Unknown binding slot.");
        return "UNKNOWN";
    }
}