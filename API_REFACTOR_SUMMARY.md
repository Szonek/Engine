# Engine API Refactor: Context-Based to Non-Context-Based

## Overview
This refactor removes the need to pass `engine_application_t` and `engine_scene_t` handles to most API functions by introducing global active application and scene state.

## Key Changes

### New Global Context Management
- `engineSetActiveApplication(engine_application_t handle)` - Set the active application
- `engineSetActiveScene(engine_scene_t scene)` - Set the active scene  
- `engineGetActiveApplication()` - Get current active application
- `engineGetActiveScene()` - Get current active scene

### Function Signature Changes

#### User Input (Application Functions)
| Old API | New API |
|---------|---------|
| `engineApplicationIsKeyboardButtonDown(app, key)` | `engineIsKeyboardButtonDown(key)` |
| `engineApplicationIsKeyboardButtonUp(app, key)` | `engineIsKeyboardButtonUp(key)` |
| `engineApplicationGetMouseCoords(app)` | `engineGetMouseCoords()` |
| `engineApplicationIsMouseButtonDown(app, button)` | `engineIsMouseButtonDown(button)` |
| `engineApplicationIsMouseButtonUp(app, button)` | `engineIsMouseButtonUp(button)` |
| `engineApplicationGetFingerInfo(app, infos)` | `engineGetFingerInfo(infos)` |

#### Frame Handling
| Old API | New API |
|---------|---------|
| `engineApplicationFrameBegine(app)` | `engineFrameBegine()` |
| `engineApplicationFrameSceneUpdate(app, scene, dt)` | `engineFrameSceneUpdate(dt)` |
| `engineApplicationFrameEnd(app)` | `engineFrameEnd()` |

#### Resource Management
| Old API | New API |
|---------|---------|
| `engineApplicationCreateShader(app, desc, name, out)` | `engineCreateShader(desc, name, out)` |
| `engineApplicationGetShaderByName(app, name)` | `engineGetShaderByName(name)` |
| `engineApplicationDestroyShader(app, shader)` | `engineDestroyShader(shader)` |
| `engineApplicationCreateFontFromFile(app, file, name)` | `engineCreateFontFromFile(file, name)` |
| `engineApplicationCreateGeometryFromDesc(app, desc, out)` | `engineCreateGeometryFromDesc(desc, out)` |
| `engineApplicationGetGeometryByName(app, name)` | `engineGetGeometryByName(name)` |
| `engineApplicationCreateTexture2DFromFile(app, file, cs, name, out)` | `engineCreateTexture2DFromFile(file, cs, name, out)` |

#### Scene and Game Objects  
| Old API | New API |
|---------|---------|
| `engineApplicationSceneCreate(app, desc, out)` | `engineSceneCreate(desc, out)` |
| `engineApplicationSceneDestroy(app, scene)` | `engineSceneDestroy(scene)` |
| `engineSceneCreateGameObject(scene)` | `engineCreateGameObject()` |
| `engineSceneDestroyGameObject(scene, obj)` | `engineDestroyGameObject(obj)` |

#### Physics
| Old API | New API |
|---------|---------|
| `engineScenePhysicsSetGravityVector(scene, gravity)` | `enginePhysicsSetGravityVector(gravity)` |
| `engineScenePhysicsGetNumCollisions(scene)` | `enginePhysicsGetNumCollisions()` |
| `engineScenePhysicsRayCast(scene, ignore, count, ray, dist)` | `enginePhysicsRayCast(ignore, count, ray, dist)` |

#### Components (Examples)
| Old API | New API |
|---------|---------|
| `engineSceneAddTransformComponent(scene, obj)` | `engineAddTransformComponent(obj)` |
| `engineSceneGetTransformComponent(scene, obj)` | `engineGetTransformComponent(obj)` |
| `engineSceneUpdateTransformComponent(scene, obj, comp)` | `engineUpdateTransformComponent(obj, comp)` |
| `engineSceneAddNameComponent(scene, obj)` | `engineAddNameComponent(obj)` |
| `engineSceneAddCameraComponent(scene, obj)` | `engineAddCameraComponent(obj)` |

## Usage Pattern

### Old Pattern
```c
engine_application_t app;
engineApplicationCreate(&app, desc);

engine_scene_t scene;
engineApplicationSceneCreate(app, scene_desc, &scene);

// Every function call needs explicit handles
if (engineApplicationIsKeyboardButtonDown(app, KEY_SPACE)) {
    engine_game_object_t obj = engineSceneCreateGameObject(scene);
    engine_transform_component_t tc = engineSceneAddTransformComponent(scene, obj);
    engineSceneUpdateTransformComponent(scene, obj, &tc);
}
```

### New Pattern
```c
engine_application_t app;
engineApplicationCreate(&app, desc);
engineSetActiveApplication(app);  // Set once

engine_scene_t scene;
engineSceneCreate(scene_desc, &scene);
engineSetActiveScene(scene);  // Set when switching scenes

// Simplified function calls
if (engineIsKeyboardButtonDown(KEY_SPACE)) {
    engine_game_object_t obj = engineCreateGameObject();
    engine_transform_component_t tc = engineAddTransformComponent(obj);
    engineUpdateTransformComponent(obj, &tc);
}
```

## Benefits

1. **Simplified API**: Function signatures are cleaner with fewer parameters
2. **Less error-prone**: No need to track and pass handles everywhere
3. **Easier to use**: More intuitive for common use cases
4. **Reduced boilerplate**: Less repetitive code in user applications

## Migration Notes

1. Add `engineSetActiveApplication()` call after creating application
2. Add `engineSetActiveScene()` call when switching scenes
3. Update function calls to remove application/scene handle parameters
4. The underlying engine functionality remains unchanged - only the API interface is simplified

## Implementation Details

- Global state is managed with static variables in engine.cpp
- Thread safety is not addressed in this refactor (would be follow-up work)
- Assertions are added to ensure active application/scene are set before use
- The IApplication framework automatically sets the active application
- Scene updates automatically set the active scene during processing