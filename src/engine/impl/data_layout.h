#pragma once

namespace engine
{
enum class DataLayout
{
    eRGBA_U8 = 0,
    eRGB_U8 = 1,
    eR_U8 = 2,

    // ..
    // ..
    // ..
    eRGBA_FP32,
    eR_FP32,

    //..
    //..

    eR_U32,

    // depth and stencil formats
    eDEPTH24_STENCIL8_U32,
    eCount
};
}