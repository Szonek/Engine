#include <engine_string.h>
#include "engine_string_impl_def.h"


// Create a new engine_string_t
engine_string_t* engineStringCreate(const char* initial_value) {
    engine_string_t* wrapper = new engine_string_t;
    wrapper->str = std::string(initial_value);
    return wrapper;
}

// Delete a engine_string_t
void engineStringDestroy(engine_string_t* str) {
    delete str;
}

// Get the length of the string
size_t engineStringLength(const engine_string_t* str) {
    return str->str.length();
}

// Get a pointer to the C string
const char* engineStringCStr(const engine_string_t* str) {
    return str->str.c_str();
}

// Set the value of the string
void engineStringSet(engine_string_t* str, const char* new_value) {
    str->str.assign(new_value);
}

// Append a value to the string
void engineStringAppend(engine_string_t* str, const char* value) {
    str->str.append(value);
}

// Clear the string
void engineStringClear(engine_string_t* str) {
    str->str.clear();
}