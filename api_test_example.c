#include "src/engine/include/engine.h"
#include <stdio.h>

/*
 * Example demonstrating the new non-context-based API
 * 
 * OLD API PATTERN:
 *   engineApplicationIsKeyboardButtonDown(app_handle, key);
 *   engineApplicationFrameBegine(app_handle);
 *   engineSceneCreateGameObject(scene_handle);
 *   engineSceneAddTransformComponent(scene_handle, game_object);
 * 
 * NEW API PATTERN:
 *   engineSetActiveApplication(app_handle);
 *   engineSetActiveScene(scene_handle);
 *   engineIsKeyboardButtonDown(key);
 *   engineFrameBegine();
 *   engineCreateGameObject();
 *   engineAddTransformComponent(game_object);
 */

int main()
{
    // Create application (unchanged)
    engine_application_create_desc_t app_desc = {
        .name = "API Test",
        .asset_store_path = NULL,
        .width = 800,
        .height = 600,
        .fullscreen = false,
        .enable_editor = false
    };
    
    engine_application_t app = NULL;
    if (engineApplicationCreate(&app, app_desc) != ENGINE_RESULT_CODE_OK) {
        printf("Failed to create application\n");
        return -1;
    }
    
    // NEW API: Set active application (no need to pass handle to other functions)
    engineSetActiveApplication(app);
    
    // Create scene (updated API - no app handle needed)
    engine_scene_create_desc_t scene_desc = {};
    engine_scene_t scene = NULL;
    if (engineSceneCreate(scene_desc, &scene) != ENGINE_RESULT_CODE_OK) {
        printf("Failed to create scene\n");
        return -1;
    }
    
    // NEW API: Set active scene (no need to pass handle to other functions)
    engineSetActiveScene(scene);
    
    // Main loop demonstrating new API
    for (int i = 0; i < 10; i++) {
        // NEW API: No application handle needed
        engine_application_frame_begine_info_t frame_info = engineFrameBegine();
        
        if (frame_info.events & ENGINE_EVENT_QUIT) {
            break;
        }
        
        // NEW API: No application handle needed for input
        if (engineIsKeyboardButtonDown(ENGINE_KEYBOARD_KEY_ESCAPE)) {
            printf("Escape key pressed!\n");
            break;
        }
        
        // NEW API: No scene handle needed for game objects
        engine_game_object_t obj = engineCreateGameObject();
        
        // NEW API: No scene handle needed for components  
        engine_tranform_component_t transform = engineAddTransformComponent(obj);
        transform.position[0] = (float)i;
        transform.position[1] = 0.0f;
        transform.position[2] = 0.0f;
        engineUpdateTransformComponent(obj, &transform);
        
        printf("Frame %d: Created object %u with transform at (%f, %f, %f)\n", 
               i, obj, transform.position[0], transform.position[1], transform.position[2]);
        
        // NEW API: No application or scene handles needed
        engineFrameSceneUpdate(frame_info.delta_time);
        
        // NEW API: No application handle needed
        engine_application_frame_end_info_t end_info = engineFrameEnd();
        if (!end_info.success) {
            printf("Frame failed\n");
            break;
        }
    }
    
    // Cleanup (unchanged)
    engineSceneDestroy(scene);
    engineApplicationDestroy(app);
    
    printf("API test completed successfully!\n");
    printf("Key differences from old API:\n");
    printf("1. Call engineSetActiveApplication() once at startup\n");
    printf("2. Call engineSetActiveScene() when switching scenes\n");
    printf("3. No need to pass app/scene handles to most functions\n");
    printf("4. Simplified function signatures throughout\n");
    
    return 0;
}