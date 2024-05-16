#pragma once

//#undef TRACY_ENABLE
#include "tracy/Tracy.hpp"

namespace engine
{
#define ENGINE_PROFILE_BEGIN_SESSION(name, filepath) TracyExportedBeginSession(name, filepath)
#define ENGINE_PROFILE_END_SESSION() TracyExportedEndSession()

#define ENGINE_PROFILE_FRAME FrameMark
#define ENGINE_PROFILE_FRAME_N(x) FrameMarkNamed(x)
#define ENGINE_PROFILE_FRAME_START(x) FrameMarkStart(x)
#define ENGINE_PROFILE_FRAME_END(x) FrameMarkEnd(x)
#define ENGINE_PROFILE_SECTION ZoneScoped
#define ENGINE_PROFILE_SECTION_N(x) ZoneScopedN(x)
#define ENGINE_PROFILE_TAG(y, x) ZoneText(x, strlen(x))
#define ENGINE_PROFILE_LOG(text, size) TracyMessage(text, size)
#define ENGINE_PROFILE_VALUE(text, value) TracyPlot(text, value)


}