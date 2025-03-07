#include <engine_vector.h>
#include "engine_vector_impl_def.h"
#include <engine.h>

#include <cassert>

#define ENGINE_DEFINE_VECTOR_FUNCTIONS(FUNC_AFFIX, STRUCT_AFFIX, ELEMENT_TYPE) \
    engine_vector_##STRUCT_AFFIX##_t engineVectorCreate##FUNC_AFFIX() { \
        engine_vector_##STRUCT_AFFIX##_t ret = new _engine_vector_##STRUCT_AFFIX##_t; \
        return ret; \
    } \
    void engineVectorDestroy##FUNC_AFFIX(engine_vector_##STRUCT_AFFIX##_t vec) { \
        delete vec; \
    } \
    void engineVectorPushBack##FUNC_AFFIX(engine_vector_##STRUCT_AFFIX##_t vec, ELEMENT_TYPE value) { \
        vec->data.push_back(value); \
    } \
    void engineVectorPopBack##FUNC_AFFIX(engine_vector_##STRUCT_AFFIX##_t vec) { \
        if (vec->data.empty()) { \
            assert(!"pop_back on empty vector"); \
        } \
        vec->data.pop_back(); \
    } \
    ELEMENT_TYPE engineVectorGet##FUNC_AFFIX(const engine_vector_##STRUCT_AFFIX##_t vec, size_t index) { \
        if (index >= vec->data.size()) { \
            assert(!"index out of range"); \
        } \
        return vec->data[index]; \
    } \
    void engineVectorSet##FUNC_AFFIX(engine_vector_##STRUCT_AFFIX##_t vec, size_t index, ELEMENT_TYPE value) { \
        if (index >= vec->data.size()) { \
            assert(!"index out of range"); \
        } \
        vec->data[index] = value; \
    } \
    void engineVectorResize##FUNC_AFFIX(const engine_vector_##STRUCT_AFFIX##_t vec, size_t new_size) { \
        vec->data.resize(new_size); \
    } \
    size_t engineVectorSize##FUNC_AFFIX(const engine_vector_##STRUCT_AFFIX##_t vec) { \
        return vec->data.size(); \
    } \
    void engineVectorErase##FUNC_AFFIX(const engine_vector_##STRUCT_AFFIX##_t vec, size_t idx) {\
        vec->data.erase(vec->data.begin() + idx); \
    }

// Example of defining a vector for int
ENGINE_DEFINE_VECTOR_FUNCTIONS(Uint32, uint32, uint32_t)
ENGINE_DEFINE_VECTOR_FUNCTIONS(Bool, bool, bool)
ENGINE_DEFINE_VECTOR_FUNCTIONS(EngineString, engine_string, engine_string_t*)
ENGINE_DEFINE_VECTOR_FUNCTIONS(EngineUiDataVariant, engine_ui_data_variant, engine_ui_data_variant_t)
