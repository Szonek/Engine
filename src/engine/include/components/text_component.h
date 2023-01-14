#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdint.h>
typedef uint32_t engine_font_t;

typedef struct _engine_text_component_t
{
    engine_font_t font_handle;
    const char* text;
} engine_text_component_t;

#ifdef __cplusplus
}
#endif // cpp