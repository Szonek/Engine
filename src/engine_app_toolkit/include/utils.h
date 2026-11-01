#pragma once
#include "engine.h"

#include <array>
#include <span>
#include <string_view>
#include <vector>
#include <cassert>
#include <cstring>

#ifdef _WIN32
#ifdef engine_app_toolkit_EXPORTS
#define ENGINE_APP_TOOLKIT_API __declspec(dllexport)
#else
#define ENGINE_APP_TOOLKIT_API __declspec(dllimport)
#endif
#else
#define ENGINE_APP_TOOLKIT_API
#endif


inline void set_c_array(std::span<float> in, std::span<const float> data)
{
    assert(in.size() == data.size());
    std::memcpy(in.data(), data.data(), in.size_bytes());
}

inline void set_c_array(std::span<float> in, const engine_fvec2_t& desc)
{
    assert(in.size() == 2);
    in[0] = desc.x;
    in[1] = desc.y;
}

inline void set_c_array(std::span<float> in, const engine_fvec3_t& desc)
{
    assert(in.size() == 3);
    in[0] = desc.x;
    in[1] = desc.y;
    in[2] = desc.z;
}

inline void set_c_array(std::span<float> in, const engine_fvec4_t& desc)
{
    assert(in.size() == 4);
    in[0] = desc.x;
    in[1] = desc.y;
    in[2] = desc.z;
    in[3] = desc.w;
}

template<typename T, std::size_t S>
inline std::array<T, S>  to_array(const T(&data)[S])
{
    std::array<T, S> ret{};
    std::memcpy(ret.data(), data, S * sizeof(T));
    return ret;
}

inline void log(std::string_view str)
{
    engineLog(str.data());
}

// the game objects of the active scene with the game-defined component, in the order of its storage; a copy, so the
// caller may change the scene while going through them
inline std::vector<engine_game_object_t> get_game_objects_with_data_component(engine_component_type_t component_type)
{
    std::vector<engine_game_object_t> game_objects;
    engine_component_view_t view = nullptr;
    engineComponentViewCreate(&view);
    engineComponentViewAttachDataComponent(view, component_type);
    engine_component_iterator_t it = nullptr;
    engine_component_iterator_t end = nullptr;
    engineComponentViewCreateBeginComponentIterator(view, &it);
    engineComponentViewCreateEndComponentIterator(view, &end);
    for (; !engineComponentIteratorCheckEqual(it, end); engineComponentIteratorNext(it))
    {
        game_objects.push_back(engineComponentIteratorGetGameObject(it));
    }
    engineComponentIteratorDelete(it);
    engineComponentIteratorDelete(end);
    engineComponentViewDestroy(view);
    return game_objects;
}