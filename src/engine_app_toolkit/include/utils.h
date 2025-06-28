#pragma once
#include "engine.h"

#include <array>
#include <span>
#include <string_view>
#include <cassert>


#if __ANDROID__
constexpr const bool K_IS_ANDROID = true;
#else  
constexpr const bool K_IS_ANDROID = false;
#endif

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

inline void set_c_array(std::span<float> in, const engine_color_desc_t& desc)
{
    assert(in.size() == 4);
    in[0] = desc.r;
    in[1] = desc.g;
    in[2] = desc.b;
    in[3] = desc.a;
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