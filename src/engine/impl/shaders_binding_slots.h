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
    //
    TEXTURE_DIFFUSE = 5,
    TEXTURE_SPECULAR = 6,
    //

    DEBUG_PHYSICS = 9
};

inline constexpr std::array<ShaderBindingSlot, 7> kAllShaderBindingSlots{
    ShaderBindingSlot::CAMERA,
    ShaderBindingSlot::SCENE,
    ShaderBindingSlot::LIGHTS,
    ShaderBindingSlot::SKINNING,
    ShaderBindingSlot::TEXTURE_DIFFUSE,
    ShaderBindingSlot::TEXTURE_SPECULAR,
    ShaderBindingSlot::DEBUG_PHYSICS
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
    case ShaderBindingSlot::TEXTURE_DIFFUSE:
        return "ENGINE_BINDING_SLOT_TEXTURE_DIFFUSE";
    case ShaderBindingSlot::TEXTURE_SPECULAR:
        return "ENGINE_BINDING_SLOT_TEXTURE_SPECULAR";
    case ShaderBindingSlot::DEBUG_PHYSICS:
        return "ENGINE_BINDING_SLOT_DEBUG_PHYSICS";
    default:
        assert(false && "Unknown binding slot.");
        return "UNKNOWN";
    }
}