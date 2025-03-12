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