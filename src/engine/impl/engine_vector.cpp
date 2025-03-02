#include <engine_vector.h>
#include "engine_vector_impl_def.h"

#include <cassert>

#define ENGINE_DEFINE_VECTOR_FUNCTIONS(VECTOR_TYPE, ELEMENT_TYPE) \
    VECTOR_TYPE VECTOR_TYPE##_create() { \
        VECTOR_TYPE ret = new _##VECTOR_TYPE; \
        return ret; \
    } \
    void VECTOR_TYPE##_destroy(VECTOR_TYPE vec) { \
        delete vec; \
    } \
    void VECTOR_TYPE##_push_back(VECTOR_TYPE vec, ELEMENT_TYPE value) { \
        vec->data.push_back(value); \
    } \
    void VECTOR_TYPE##_pop_back(VECTOR_TYPE vec) { \
        if (vec->data.empty()) { \
            assert(!"pop_back on empty vector"); \
        } \
        vec->data.pop_back(); \
    } \
    ELEMENT_TYPE VECTOR_TYPE##_get(const VECTOR_TYPE vec, size_t index) { \
        if (index >= vec->data.size()) { \
            assert(!"index out of range"); \
        } \
        return vec->data[index]; \
    } \
    void VECTOR_TYPE##_set(VECTOR_TYPE vec, size_t index, ELEMENT_TYPE value) { \
        if (index >= vec->data.size()) { \
            assert(!"index out of range"); \
        } \
        vec->data[index] = value; \
    } \
    size_t VECTOR_TYPE##_size(const VECTOR_TYPE vec) { \
        return vec->data.size(); \
    }

// Example of defining a vector for int
ENGINE_DEFINE_VECTOR_FUNCTIONS(engine_vector_int_t, int)
// You can define more vector types here, e.g.:
// ENGINE_DEFINE_VECTOR_FUNCTIONS(engine_vector_float_t, float)
// ENGINE_DEFINE_VECTOR_FUNCTIONS(engine_vector_double_t, double)