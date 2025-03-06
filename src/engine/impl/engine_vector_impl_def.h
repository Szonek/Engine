#pragma once
#include <engine_vector.h>
#include "engine_string_impl_def.h"

#include <vector>



#define ENGINE_DEFINE_VECTOR_IMPL_TYPE(STRUCT_AFFIX, ELEMENT_TYPE) \
    struct _engine_vector_##STRUCT_AFFIX##_t { \
        std::vector<ELEMENT_TYPE> data; \
    }; \


ENGINE_DEFINE_VECTOR_IMPL_TYPE(uint32, uint32_t)
ENGINE_DEFINE_VECTOR_IMPL_TYPE(bool, bool)
ENGINE_DEFINE_VECTOR_IMPL_TYPE(engine_string, engine_string_t*)