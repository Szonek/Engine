# Context-Based API Usage Example

This document demonstrates how the new context-based API simplifies engine usage by eliminating the need to pass application and scene handles to every function call.

## Before: Handle-Based API

```c
// Old way - must pass handles to every function
engine_application_t app = /* ... */;
engine_scene_t scene = /* ... */;

// Create game objects
engine_game_object_t player = engineSceneCreateGameObject(scene);
engine_game_object_t enemy = engineSceneCreateGameObject(scene);

// Add components
engine_name_component_t player_name = engineSceneAddNameComponent(scene, player);
engine_transform_component_t player_transform = engineSceneAddTransformComponent(scene, player);
engine_mesh_component_t player_mesh = engineSceneAddMeshComponent(scene, player);
engine_material_component_t player_material = engineSceneAddMaterialComponent(scene, player);

engine_name_component_t enemy_name = engineSceneAddNameComponent(scene, enemy);
engine_transform_component_t enemy_transform = engineSceneAddTransformComponent(scene, enemy);
engine_mesh_component_t enemy_mesh = engineSceneAddMeshComponent(scene, enemy);
engine_material_component_t enemy_material = engineSceneAddMaterialComponent(scene, enemy);

// Check components
if (engineSceneHasNameComponent(scene, player)) {
    // Update component
    engineSceneUpdateNameComponent(scene, player, &player_name);
}

// Destroy objects
engineSceneDestroyGameObject(scene, player);
engineSceneDestroyGameObject(scene, enemy);
```

## After: Context-Based API

```c
// New way - set context once, then use simplified functions
engine_application_t app = /* ... */;
engine_scene_t scene = /* ... */;

// Set contexts once
engineSetCurrentApplication(app);
engineSetCurrentScene(scene);

// Create game objects (no scene parameter needed)
engine_game_object_t player = engineCreateGameObject();
engine_game_object_t enemy = engineCreateGameObject();

// Add components (no scene parameter needed)
engine_name_component_t player_name = engineAddNameComponent(player);
engine_transform_component_t player_transform = engineAddTransformComponent(player);
engine_mesh_component_t player_mesh = engineAddMeshComponent(player);
engine_material_component_t player_material = engineAddMaterialComponent(player);

engine_name_component_t enemy_name = engineAddNameComponent(enemy);
engine_transform_component_t enemy_transform = engineAddTransformComponent(enemy);
engine_mesh_component_t enemy_mesh = engineAddMeshComponent(enemy);
engine_material_component_t enemy_material = engineAddMaterialComponent(enemy);

// Check components (no scene parameter needed)
if (engineHasNameComponent(player)) {
    // Update component
    engineUpdateNameComponent(player, &player_name);
}

// Destroy objects (no scene parameter needed)
engineDestroyGameObject(player);
engineDestroyGameObject(enemy);
```

## Benefits

1. **Reduced Verbosity**: No need to pass scene handles to every function
2. **Cleaner Code**: Less parameter clutter in function calls
3. **Backward Compatibility**: All existing handle-based functions still work
4. **Flexibility**: Can switch contexts when working with multiple scenes

## Context Management

```c
// Get current contexts
engine_application_t current_app = engineGetCurrentApplication();
engine_scene_t current_scene = engineGetCurrentScene();

// Switch to different scene
engineSetCurrentScene(another_scene);
engine_game_object_t obj = engineCreateGameObject(); // Creates in another_scene

// Switch back
engineSetCurrentScene(original_scene);
```

## Function Mapping

| Old Handle-Based Function | New Context-Based Function |
|----------------------------|----------------------------|
| `engineSceneCreateGameObject(scene)` | `engineCreateGameObject()` |
| `engineSceneDestroyGameObject(scene, go)` | `engineDestroyGameObject(go)` |
| `engineSceneAddNameComponent(scene, go)` | `engineAddNameComponent(go)` |
| `engineSceneGetNameComponent(scene, go)` | `engineGetNameComponent(go)` |
| `engineSceneUpdateNameComponent(scene, go, comp)` | `engineUpdateNameComponent(go, comp)` |
| `engineSceneRemoveNameComponent(scene, go)` | `engineRemoveNameComponent(go)` |
| `engineSceneHasNameComponent(scene, go)` | `engineHasNameComponent(go)` |
| `engineSceneAddTransformComponent(scene, go)` | `engineAddTransformComponent(go)` |
| `engineSceneGetTransformComponent(scene, go)` | `engineGetTransformComponent(go)` |
| `engineSceneUpdateTransformComponent(scene, go, comp)` | `engineUpdateTransformComponent(go, comp)` |
| `engineSceneRemoveTransformComponent(scene, go)` | `engineRemoveTransformComponent(go)` |
| `engineSceneHasTransformComponent(scene, go)` | `engineHasTransformComponent(go)` |
| `engineSceneAddMeshComponent(scene, go)` | `engineAddMeshComponent(go)` |
| `engineSceneGetMeshComponent(scene, go)` | `engineGetMeshComponent(go)` |
| `engineSceneUpdateMeshComponent(scene, go, comp)` | `engineUpdateMeshComponent(go, comp)` |
| `engineSceneRemoveMeshComponent(scene, go)` | `engineRemoveMeshComponent(go)` |
| `engineSceneHasMeshComponent(scene, go)` | `engineHasMeshComponent(go)` |
| `engineSceneAddMaterialComponent(scene, go)` | `engineAddMaterialComponent(go)` |
| `engineSceneGetMaterialComponent(scene, go)` | `engineGetMaterialComponent(go)` |
| `engineSceneUpdateMaterialComponent(scene, go, comp)` | `engineUpdateMaterialComponent(go, comp)` |
| `engineSceneRemoveMaterialComponent(scene, go)` | `engineRemoveMaterialComponent(go)` |
| `engineSceneHasMaterialComponent(scene, go)` | `engineHasMaterialComponent(go)` |