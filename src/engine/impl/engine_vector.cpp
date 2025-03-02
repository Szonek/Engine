#include <engine_vector.h>
#include "engine_vector_impl_def.h"
#include <engine.h>

#include <cassert>

#define ENGINE_DEFINE_VECTOR_FUNCTIONS(NAME, ELEMENT_TYPE) \
    engine_vector_##NAME##_t engineVectorCreate_##NAME() { \
        engine_vector_##NAME##_t ret = new _engine_vector_##NAME##_t; \
        return ret; \
    } \
    void engineVectorDestroy_##NAME(engine_vector_##NAME##_t vec) { \
        delete vec; \
    } \
    void engineVectorPushBack_##NAME(engine_vector_##NAME##_t vec, ELEMENT_TYPE value) { \
        vec->data.push_back(value); \
    } \
    void engineVectorPopBack_##NAME(engine_vector_##NAME##_t vec) { \
        if (vec->data.empty()) { \
            assert(!"pop_back on empty vector"); \
        } \
        vec->data.pop_back(); \
    } \
    ELEMENT_TYPE engineVectorGet_##NAME(const engine_vector_##NAME##_t vec, size_t index) { \
        if (index >= vec->data.size()) { \
            assert(!"index out of range"); \
        } \
        return vec->data[index]; \
    } \
    void engineVectorSet_##NAME(engine_vector_##NAME##_t vec, size_t index, ELEMENT_TYPE value) { \
        if (index >= vec->data.size()) { \
            assert(!"index out of range"); \
        } \
        vec->data[index] = value; \
    } \
    void engineVectorResize_##NAME(const engine_vector_##NAME##_t vec, size_t new_size) { \
        vec->data.resize(new_size); \
    } \
    size_t engineVectorSize_##NAME(const engine_vector_##NAME##_t vec) { \
        return vec->data.size(); \
    }

// Example of defining a vector for int
ENGINE_DEFINE_VECTOR_FUNCTIONS(int, int)
ENGINE_DEFINE_VECTOR_FUNCTIONS(ui_document_data_binding_struct_member, engine_ui_document_data_binding_struct_member_t)