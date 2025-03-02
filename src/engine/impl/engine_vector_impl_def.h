#pragma once
#include <engine_vector.h>

#include <vector>

#define ENGINE_DEFINE_VECTOR_IMPL_TYPE(VECTOR_TYPE, ELEMENT_TYPE) \
    struct _##VECTOR_TYPE { \
        std::vector<ELEMENT_TYPE> data; \
    }; \



// Example of defining a vector for int
ENGINE_DEFINE_VECTOR_IMPL_TYPE(engine_vector_int_t, int)
// You can define more vector types here, e.g.:
// ENGINE_DEFINE_VECTOR_IMPL_TYPE(engine_vector_float_t, float)
// ENGINE_DEFINE_VECTOR_IMPL_TYPE(engine_vector_double_t, double)