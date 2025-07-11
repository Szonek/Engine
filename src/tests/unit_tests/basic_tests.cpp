#include <engine.h>

#include <gtest/gtest.h>

#include <vector>

TEST(Basic, App_CreateAndDestroy) 
{
    engine_application_create_desc_t desc{};
    desc.name = "Test-App";
    desc.width = 1280;
    desc.height = 720;
    desc.asset_store_path = "";
    desc.fullscreen = false;
    engine_application_t app = nullptr;
    engineApplicationCreate(&app, desc);
    ASSERT_NE(app, nullptr);
    engineApplicationDestroy(app);
}

TEST(Basic, VectorU32)
{
    const std::vector<std::uint32_t> test_values = {13, 0, 0, 37};
    engine_vector_uint32_t vec_str = engineVectorCreateUint32();
    ASSERT_NE(vec_str, nullptr);

    for (const auto& tv : test_values)
    {
        engineVectorPushBackUint32(vec_str, tv);
    }
    ASSERT_EQ(engineVectorSizeUint32(vec_str), test_values.size());

    for (auto i = 0; i < test_values.size(); i++)
    {
        ASSERT_EQ(engineVectorGetUint32(vec_str, i), test_values.at(i));
    }
    engineVectorDestroyUint32(vec_str);
}

TEST(Basic, ContextSystem)
{
    // Create application
    engine_application_create_desc_t desc{};
    desc.name = "Test-Context-App";
    desc.width = 1280;
    desc.height = 720;
    desc.asset_store_path = "";
    desc.fullscreen = false;
    engine_application_t app = nullptr;
    engineApplicationCreate(&app, desc);
    ASSERT_NE(app, nullptr);

    // Create scene
    engine_scene_create_desc_t scene_desc{};
    engine_scene_t scene = nullptr;
    ASSERT_EQ(engineApplicationSceneCreate(app, scene_desc, &scene), ENGINE_RESULT_CODE_OK);
    ASSERT_NE(scene, nullptr);

    // Test context management
    engineSetCurrentApplication(app);
    engineSetCurrentScene(scene);
    
    ASSERT_EQ(engineGetCurrentApplication(), app);
    ASSERT_EQ(engineGetCurrentScene(), scene);

    // Test context-based game object creation
    engine_game_object_t go1 = engineCreateGameObject();
    engine_game_object_t go2 = engineSceneCreateGameObject(scene);
    
    ASSERT_NE(go1, ENGINE_INVALID_GAME_OBJECT_ID);
    ASSERT_NE(go2, ENGINE_INVALID_GAME_OBJECT_ID);
    ASSERT_NE(go1, go2);

    // Test context-based component operations
    engine_name_component_t name_comp1 = engineAddNameComponent(go1);
    engine_name_component_t name_comp2 = engineSceneAddNameComponent(scene, go2);
    
    ASSERT_TRUE(engineHasNameComponent(go1));
    ASSERT_TRUE(engineSceneHasNameComponent(scene, go2));

    // Cleanup
    engineDestroyGameObject(go1);
    engineSceneDestroyGameObject(scene, go2);
    
    engineApplicationSceneDestroy(app, scene);
    engineApplicationDestroy(app);
}