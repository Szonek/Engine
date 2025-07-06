#pragma once
#include <engine.h>

#include <string>
#include <string_view>
#include <cassert>

namespace engine
{

class ScopedProfiler
{
public:
    ScopedProfiler(const char* name)
        : ctx_(engineProfileZoneStart(name, true))
    {
        assert(ctx_ != nullptr);
    }
    ScopedProfiler(const ScopedProfiler& rhs) = delete;
    ScopedProfiler& operator=(const ScopedProfiler& rhs) = delete;
    ScopedProfiler(ScopedProfiler&&) = delete;
    ScopedProfiler& operator=(ScopedProfiler&&) = delete;
    ~ScopedProfiler()
    {
        engineProfileZoneEnd(ctx_);
    }
private:
    engine_profiler_ctx_t* ctx_ = nullptr;
};

}