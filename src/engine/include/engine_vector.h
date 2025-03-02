#pragma once
#include "engine_defs.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

#include <stddef.h>

    // Macro to define a vector for a specific type
#define DECLARE_VECTOR_TYPE(VECTOR_TYPE, ELEMENT_TYPE) \
    typedef struct _##VECTOR_TYPE##* VECTOR_TYPE; \
    ENGINE_API VECTOR_TYPE VECTOR_TYPE##_create(); \
    ENGINE_API void VECTOR_TYPE##_destroy(VECTOR_TYPE vec); \
    ENGINE_API void VECTOR_TYPE##_push_back(VECTOR_TYPE vec, ELEMENT_TYPE value); \
    ENGINE_API void VECTOR_TYPE##_pop_back(VECTOR_TYPE vec); \
    ENGINE_API ELEMENT_TYPE VECTOR_TYPE##_get(const VECTOR_TYPE vec, size_t index); \
    ENGINE_API void VECTOR_TYPE##_set(VECTOR_TYPE vec, size_t index, ELEMENT_TYPE value); \
    ENGINE_API size_t VECTOR_TYPE##_size(const VECTOR_TYPE* vec);

// Example of declaring a vector for int
DECLARE_VECTOR_TYPE(engine_vector_int_t, int)
// You can declare more vector types here, e.g.:
// DECLARE_VECTOR_TYPE(engine_vector_float_t, float)
// DECLARE_VECTOR_TYPE(engine_vector_double_t, double)

#ifdef __cplusplus
}
#endif