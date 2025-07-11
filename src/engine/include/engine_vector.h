#pragma once
#include "engine_defs.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

    // Macro to define a vector for a specific type
#define ENGINE_DECLARE_VECTOR_TYPE(FUNC_AFFIX, STRUCT_AFFIX, ELEMENT_TYPE) \
    typedef struct _engine_vector_##STRUCT_AFFIX##_t* engine_vector_##STRUCT_AFFIX##_t; \
    ENGINE_API engine_vector_##STRUCT_AFFIX##_t engineVectorCreate##FUNC_AFFIX(); \
    ENGINE_API void engineVectorDestroy##FUNC_AFFIX(engine_vector_##STRUCT_AFFIX##_t vec); \
    ENGINE_API void engineVectorPushBack##FUNC_AFFIX(engine_vector_##STRUCT_AFFIX##_t vec, ELEMENT_TYPE value); \
    ENGINE_API void engineVectorPopBack##FUNC_AFFIX(engine_vector_##STRUCT_AFFIX##_t vec); \
    ENGINE_API ELEMENT_TYPE engineVectorGet##FUNC_AFFIX(const engine_vector_##STRUCT_AFFIX##_t vec, size_t index); \
    ENGINE_API void engineVectorSet##FUNC_AFFIX(engine_vector_##STRUCT_AFFIX##_t vec, size_t index, ELEMENT_TYPE value); \
    ENGINE_API void engineVectorResize##FUNC_AFFIX(const engine_vector_##STRUCT_AFFIX##_t vec, size_t new_size); \
    ENGINE_API size_t engineVectorSize##FUNC_AFFIX(const engine_vector_##STRUCT_AFFIX##_t vec); \
    ENGINE_API void engineVectorErase##FUNC_AFFIX(const engine_vector_##STRUCT_AFFIX##_t vec, size_t idx);

// Example of declaring a vector for int
ENGINE_DECLARE_VECTOR_TYPE(Uint32, uint32, uint32_t);
ENGINE_DECLARE_VECTOR_TYPE(Bool, bool, bool);
typedef struct engine_string_t engine_string_t;
ENGINE_DECLARE_VECTOR_TYPE(EngineString, engine_string, engine_string_t*);
typedef struct _engine_ui_data_variant_t engine_ui_data_variant_t;
ENGINE_DECLARE_VECTOR_TYPE(EngineUiDataVariant, engine_ui_data_variant, engine_ui_data_variant_t);
typedef struct _engine_joint_desc_t engine_joint_desc_t;
ENGINE_DECLARE_VECTOR_TYPE(EngineJointDesc, engine_joint_desc, engine_joint_desc_t*);
#ifdef __cplusplus
}
#endif