#pragma once
#include <engine_vector.h>

#include <vector>



#define ENGINE_DEFINE_VECTOR_IMPL_TYPE(STRUCT_AFFIX, ELEMENT_TYPE) \
    struct _engine_vector_##STRUCT_AFFIX##_t { \
        std::vector<ELEMENT_TYPE> data; \
    }; \


ENGINE_DEFINE_VECTOR_IMPL_TYPE(uint32, uint32_t)