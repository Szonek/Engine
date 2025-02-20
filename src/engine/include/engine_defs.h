#pragma once

#ifdef _WIN32
#ifdef engine_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
#define ENGINE_API
#endif

#define ENGINE_BIT_MASK(id) 1 << id
#define ENGINE_BIT_MASK_MAX 0x7FFFFFFF

#define ENGINE_APPLICATION_NAME_MAX_LENGTH 256