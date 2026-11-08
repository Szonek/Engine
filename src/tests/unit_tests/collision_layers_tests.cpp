#include "collision_layers.h"

#include <gtest/gtest.h>

#include <string>

using engine::ALL_PHYSICS_LAYERS;
using engine::BodyMotion;
using engine::PHYSICS_LAYER_COUNT;

TEST(CollisionLayers, LayerBitOfEveryLayerAndNoneOutOfRange)
{
    for (std::uint32_t layer = 0; layer < PHYSICS_LAYER_COUNT; layer++)
    {
        EXPECT_EQ(engine::layer_bit(layer), 1u << layer);
    }
    EXPECT_EQ(engine::layer_bit(PHYSICS_LAYER_COUNT), 0u);
    EXPECT_EQ(engine::layer_bit(1000), 0u);
}

TEST(CollisionLayers, DefaultsInteract)
{
    EXPECT_TRUE(engine::layers_interact(0, ALL_PHYSICS_LAYERS, 0, ALL_PHYSICS_LAYERS));
    EXPECT_TRUE(engine::layers_interact(0, ALL_PHYSICS_LAYERS, 31, ALL_PHYSICS_LAYERS));
}

TEST(CollisionLayers, BothMasksMustIncludeTheOtherLayer)
{
    // a hitbox (3) colliding with characters (2) only, a character colliding with everything
    const std::uint32_t character = 2;
    const std::uint32_t hitbox = 3;
    EXPECT_TRUE(engine::layers_interact(hitbox, 1u << character, character, ALL_PHYSICS_LAYERS));
    // a wall (1) is not in the hitbox's mask
    EXPECT_FALSE(engine::layers_interact(hitbox, 1u << character, 1, ALL_PHYSICS_LAYERS));
    // the character excludes hitboxes: no pair even though the hitbox includes characters
    EXPECT_FALSE(engine::layers_interact(hitbox, 1u << character, character, ALL_PHYSICS_LAYERS & ~(1u << hitbox)));
}

TEST(CollisionLayers, InteractionIsSymmetric)
{
    const std::uint32_t masks[] = { 0u, ALL_PHYSICS_LAYERS, 0b1010u, 0b0110u, 1u << 31 };
    for (std::uint32_t layer_a : { 0u, 1u, 2u, 3u, 31u })
    {
        for (std::uint32_t layer_b : { 0u, 1u, 2u, 3u, 31u })
        {
            for (const auto mask_a : masks)
            {
                for (const auto mask_b : masks)
                {
                    EXPECT_EQ(engine::layers_interact(layer_a, mask_a, layer_b, mask_b), engine::layers_interact(layer_b, mask_b, layer_a, mask_a));
                }
            }
        }
    }
}

// a component's mask is exact: 0 collides with nothing, not even its own layer
TEST(CollisionLayers, ComponentMaskZeroCollidesWithNothing)
{
    for (std::uint32_t layer = 0; layer < PHYSICS_LAYER_COUNT; layer++)
    {
        EXPECT_FALSE(engine::layers_interact(layer, 0, layer, ALL_PHYSICS_LAYERS));
        EXPECT_FALSE(engine::layers_interact(0, ALL_PHYSICS_LAYERS, layer, 0));
    }
}

TEST(CollisionLayers, LayerOutOfRangeInteractsWithNothing)
{
    EXPECT_FALSE(engine::layers_interact(PHYSICS_LAYER_COUNT, ALL_PHYSICS_LAYERS, 0, ALL_PHYSICS_LAYERS));
    EXPECT_FALSE(engine::query_sees(1u, PHYSICS_LAYER_COUNT));
}

// a query's mask of 0 is no filter: it sees every layer
TEST(CollisionLayers, QueryMaskZeroSeesEveryLayer)
{
    for (std::uint32_t layer = 0; layer < PHYSICS_LAYER_COUNT; layer++)
    {
        EXPECT_TRUE(engine::query_sees(0, layer));
        EXPECT_TRUE(engine::query_sees(1u << layer, layer));
        EXPECT_FALSE(engine::query_sees(~(1u << layer), layer));
    }
}

TEST(CollisionLayers, BodyMotionRule)
{
    EXPECT_FALSE(engine::body_motions_interact(BodyMotion::eStatic, BodyMotion::eStatic));
    EXPECT_FALSE(engine::body_motions_interact(BodyMotion::eStatic, BodyMotion::eKinematic));
    EXPECT_FALSE(engine::body_motions_interact(BodyMotion::eKinematic, BodyMotion::eStatic));
    EXPECT_TRUE(engine::body_motions_interact(BodyMotion::eKinematic, BodyMotion::eKinematic));
    EXPECT_TRUE(engine::body_motions_interact(BodyMotion::eDynamic, BodyMotion::eStatic));
    EXPECT_TRUE(engine::body_motions_interact(BodyMotion::eStatic, BodyMotion::eDynamic));
    EXPECT_TRUE(engine::body_motions_interact(BodyMotion::eDynamic, BodyMotion::eKinematic));
    EXPECT_TRUE(engine::body_motions_interact(BodyMotion::eDynamic, BodyMotion::eDynamic));
}

TEST(CollisionLayers, NamesStartWithDefaultOnly)
{
    const engine::PhysicsLayerNames names;
    EXPECT_EQ(std::string(names.get(0)), "Default");
    for (std::uint32_t layer = 1; layer < PHYSICS_LAYER_COUNT; layer++)
    {
        EXPECT_EQ(std::string(names.get(layer)), "");
    }
    EXPECT_EQ(names.get(PHYSICS_LAYER_COUNT), nullptr);
}

TEST(CollisionLayers, NamesAreSetAndOutOfRangeIsRefused)
{
    engine::PhysicsLayerNames names;
    EXPECT_TRUE(names.set(2, "Character"));
    EXPECT_TRUE(names.set(0, "Ground"));
    EXPECT_TRUE(names.set(31, "Last"));
    EXPECT_FALSE(names.set(PHYSICS_LAYER_COUNT, "Nope"));
    EXPECT_EQ(std::string(names.get(2)), "Character");
    EXPECT_EQ(std::string(names.get(0)), "Ground");
    EXPECT_EQ(std::string(names.get(31)), "Last");
    // a name set back to "" is unnamed again
    EXPECT_TRUE(names.set(2, ""));
    EXPECT_EQ(std::string(names.get(2)), "");
}
