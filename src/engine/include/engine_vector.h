#pragma once
#include "engine_defs.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

#include <stddef.h>

    // Macro to define a vector for a specific type
#define ENGINE_DECLARE_VECTOR_TYPE(NAME, ELEMENT_TYPE) \
    typedef struct _engine_vector_##NAME##_t* engine_vector_##NAME##_t; \
    ENGINE_API engine_vector_##NAME##_t engineVectorCreate_##NAME(); \
    ENGINE_API void engineVectorDestroy_##NAME(engine_vector_##NAME##_t vec); \
    ENGINE_API void engineVectorPushBack_##NAME(engine_vector_##NAME##_t vec, ELEMENT_TYPE value); \
    ENGINE_API void engineVectorPopBack_##NAME(engine_vector_##NAME##_t vec); \
    ENGINE_API ELEMENT_TYPE engineVectorGet_##NAME(const engine_vector_##NAME##_t vec, size_t index); \
    ENGINE_API void engineVectorSet_##NAME(engine_vector_##NAME##_t vec, size_t index, ELEMENT_TYPE value); \
    ENGINE_API void engineVectorResize_##NAME(const engine_vector_##NAME##_t vec, size_t new_size); \
    ENGINE_API size_t engineVectorSize_##NAME(const engine_vector_##NAME##_t vec);

// Example of declaring a vector for int
ENGINE_DECLARE_VECTOR_TYPE(int, int)
typedef struct _engine_ui_document_data_binding_struct_member_t engine_ui_document_data_binding_struct_member_t;
ENGINE_DECLARE_VECTOR_TYPE(ui_document_data_binding_struct_member, engine_ui_document_data_binding_struct_member_t)

#ifdef __cplusplus
}
#endif