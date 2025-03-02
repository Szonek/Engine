#pragma once
#include <engine_vector.h>

#include <vector>



#define ENGINE_DEFINE_VECTOR_IMPL_TYPE(NAME, ELEMENT_TYPE) \
    struct _engine_vector_##NAME##_t { \
        std::vector<ELEMENT_TYPE> data; \
    }; \


ENGINE_DEFINE_VECTOR_IMPL_TYPE(int, int)
ENGINE_DEFINE_VECTOR_IMPL_TYPE(ui_document_data_binding_struct_member, engine_ui_document_data_binding_struct_member_t)
