#include "bounds.h"

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
engine::Aabb box_at(const glm::vec3& center, float half = 0.5f)
{
    return engine::Aabb{ center - glm::vec3(half), center + glm::vec3(half) };
}

// a camera at the origin looking down -z
engine::Frustum perspective_frustum()
{
    const auto projection = glm::perspective(glm::radians(60.0f), 1.0f, 1.0f, 100.0f);
    const auto view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    return engine::Frustum::from_view_projection(projection * view);
}
}  // namespace

TEST(BoundsTest, TransformedBoxIsTheBoxAroundItsCorners)
{
    const engine::Aabb box{ glm::vec3(0.0f), glm::vec3(2.0f, 1.0f, 4.0f) };
    // a quarter turn around y, then moved: local x goes to world -z, local z to world x
    const auto matrix = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 5.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const auto world = engine::transform_aabb(box, matrix);
    EXPECT_NEAR(world.min.x, 10.0f, 1e-4f);
    EXPECT_NEAR(world.max.x, 14.0f, 1e-4f);
    EXPECT_NEAR(world.min.y, 5.0f, 1e-4f);
    EXPECT_NEAR(world.max.y, 6.0f, 1e-4f);
    EXPECT_NEAR(world.min.z, -2.0f, 1e-4f);
    EXPECT_NEAR(world.max.z, 0.0f, 1e-4f);
}

TEST(BoundsTest, TransformedBoxFollowsScale)
{
    const engine::Aabb box{ glm::vec3(-1.0f), glm::vec3(1.0f) };
    const auto world = engine::transform_aabb(box, glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 3.0f, 4.0f)));
    EXPECT_NEAR(world.min.x, -2.0f, 1e-5f);
    EXPECT_NEAR(world.max.y, 3.0f, 1e-5f);
    EXPECT_NEAR(world.max.z, 4.0f, 1e-5f);
}

TEST(BoundsTest, PerspectiveFrustumSeesWhatIsInFrontOfTheCamera)
{
    const auto frustum = perspective_frustum();
    EXPECT_TRUE(frustum.intersects(box_at(glm::vec3(0.0f, 0.0f, -10.0f))));
    // behind the camera, before the near plane, past the far plane
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(0.0f, 0.0f, 10.0f))));
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(0.0f, 0.0f, -0.2f), 0.1f)));
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(0.0f, 0.0f, -150.0f))));
    // to the sides: at a distance of 10 the view is about 5.8 wide each way
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(20.0f, 0.0f, -10.0f))));
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(-20.0f, 0.0f, -10.0f))));
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(0.0f, 20.0f, -10.0f))));
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(0.0f, -20.0f, -10.0f))));
}

TEST(BoundsTest, BoxCrossingAPlaneIsSeen)
{
    const auto frustum = perspective_frustum();
    // its center is outside the right plane, its side reaches in
    EXPECT_TRUE(frustum.intersects(box_at(glm::vec3(7.0f, 0.0f, -10.0f), 2.0f)));
    // crossing the near plane
    EXPECT_TRUE(frustum.intersects(box_at(glm::vec3(0.0f, 0.0f, -0.5f), 1.0f)));
    // a box around the whole frustum
    EXPECT_TRUE(frustum.intersects(box_at(glm::vec3(0.0f), 500.0f)));
}

TEST(BoundsTest, OrthographicFrustumIsABox)
{
    // what the shadow camera and the game's camera use
    const auto projection = glm::ortho(-10.0f, 10.0f, -5.0f, 5.0f, 0.0f, 50.0f);
    const auto view = glm::lookAt(glm::vec3(0.0f, 20.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    const auto frustum = engine::Frustum::from_view_projection(projection * view);
    EXPECT_TRUE(frustum.intersects(box_at(glm::vec3(0.0f))));
    EXPECT_TRUE(frustum.intersects(box_at(glm::vec3(9.0f, 0.0f, 4.0f))));
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(12.0f, 0.0f, 0.0f))));
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(0.0f, 0.0f, 7.0f))));
    // above the camera, and below its far plane
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(0.0f, 25.0f, 0.0f))));
    EXPECT_FALSE(frustum.intersects(box_at(glm::vec3(0.0f, -35.0f, 0.0f))));
}
