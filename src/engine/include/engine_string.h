#pragma once
#include "engine_defs.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

// Define the opaque type for the C API
typedef struct _engine_string_t* engine_string_t;

// Create a new engine_string_t
ENGINE_API engine_string_t engineStringCreate(const char* initial_value);

// Delete a engine_string_t
ENGINE_API void engineStringDestroy(engine_string_t str);

// Get the length of the string
ENGINE_API size_t engineStringLength(const engine_string_t str);

// Get a pointer to the C string
ENGINE_API const char* engineStringCStr(const engine_string_t str);

// Set the value of the string
ENGINE_API void engineStringSet(engine_string_t str, const char* new_value);

// Append a value to the string
ENGINE_API void engineStringAppend(engine_string_t str, const char* value);

// Clear the string
ENGINE_API void engineStringClear(engine_string_t str);

#ifdef __cplusplus
}
#endif