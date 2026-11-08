#include "smoke.h"

#include <gtest/gtest.h>

using project_c::SmokeTuning;
using project_c::cloud_radius;
using project_c::segment_touches_sphere;
using project_c::smoke_landing;

namespace
{
SmokeTuning tuning()
{
    return SmokeTuning{ 12.0f, 8.0f, 3.0f, 1.0f, 0.25f, 6.0f, 0.5f, 3.0f, 5.0f };
}
}  // namespace

TEST(Smoke, CloudGrowsHoldsAndFades)
{
    const auto t = tuning();
    EXPECT_EQ(cloud_radius(-0.1f, t), 0.0f);
    EXPECT_EQ(cloud_radius(0.0f, t), 0.0f);
    EXPECT_NEAR(cloud_radius(0.125f, t), 1.5f, 1e-5f);
    EXPECT_NEAR(cloud_radius(0.25f, t), 3.0f, 1e-5f);
    EXPECT_NEAR(cloud_radius(3.0f, t), 3.0f, 1e-5f);
    EXPECT_NEAR(cloud_radius(5.5f, t), 3.0f, 1e-5f);
    EXPECT_NEAR(cloud_radius(5.75f, t), 1.5f, 1e-5f);
    EXPECT_EQ(cloud_radius(6.0f, t), 0.0f);
    EXPECT_EQ(cloud_radius(10.0f, t), 0.0f);
}

TEST(Smoke, CloudWithoutGrowOrFadeIsFullAtOnce)
{
    auto t = tuning();
    t.grow_seconds = 0.0f;
    t.fade_seconds = 0.0f;
    EXPECT_EQ(cloud_radius(0.0f, t), 3.0f);
    EXPECT_EQ(cloud_radius(5.99f, t), 3.0f);
}

TEST(Smoke, SegmentThroughTheSphereTouchesIt)
{
    const glm::vec3 centre(0.0f, 1.0f, 0.0f);
    EXPECT_TRUE(segment_touches_sphere(glm::vec3(-5.0f, 1.0f, 0.0f), glm::vec3(5.0f, 1.0f, 0.0f), centre, 1.0f));
    // through it off the centre
    EXPECT_TRUE(segment_touches_sphere(glm::vec3(-5.0f, 1.5f, 0.5f), glm::vec3(5.0f, 1.5f, 0.5f), centre, 1.0f));
}

TEST(Smoke, SegmentInsideOrEndingInsideTouchesIt)
{
    const glm::vec3 centre(0.0f);
    EXPECT_TRUE(segment_touches_sphere(glm::vec3(-0.2f, 0.0f, 0.0f), glm::vec3(0.2f, 0.0f, 0.0f), centre, 1.0f));
    EXPECT_TRUE(segment_touches_sphere(glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(5.0f, 0.0f, 0.0f), centre, 1.0f));
}

TEST(Smoke, SegmentTouchingTheSurfaceTouchesIt)
{
    EXPECT_TRUE(segment_touches_sphere(glm::vec3(-5.0f, 1.0f, 0.0f), glm::vec3(5.0f, 1.0f, 0.0f), glm::vec3(0.0f), 1.0f));
}

TEST(Smoke, SegmentMissingTheSphereDoesNot)
{
    const glm::vec3 centre(0.0f);
    EXPECT_FALSE(segment_touches_sphere(glm::vec3(-5.0f, 2.0f, 0.0f), glm::vec3(5.0f, 2.0f, 0.0f), centre, 1.0f));
    // pointing at it but ending before it
    EXPECT_FALSE(segment_touches_sphere(glm::vec3(-5.0f, 0.0f, 0.0f), glm::vec3(-2.0f, 0.0f, 0.0f), centre, 1.0f));
    // no cloud
    EXPECT_FALSE(segment_touches_sphere(glm::vec3(-5.0f, 0.0f, 0.0f), glm::vec3(5.0f, 0.0f, 0.0f), centre, 0.0f));
}

TEST(Smoke, ZeroLengthSegmentIsAPoint)
{
    const glm::vec3 centre(0.0f);
    EXPECT_TRUE(segment_touches_sphere(glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f), centre, 1.0f));
    EXPECT_FALSE(segment_touches_sphere(glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f), centre, 1.0f));
}

TEST(Smoke, LandingWithinRangeIsTheCursorPoint)
{
    const glm::vec3 landing = smoke_landing(glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(4.0f, 2.0f, 5.0f), 8.0f);
    EXPECT_EQ(landing, glm::vec3(4.0f, 2.0f, 5.0f));
}

TEST(Smoke, LandingBeyondRangeIsClampedToItFlat)
{
    const glm::vec3 landing = smoke_landing(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(20.0f, 1.0f, 0.0f), 8.0f);
    EXPECT_NEAR(landing.x, 8.0f, 1e-5f);
    EXPECT_NEAR(landing.y, 1.0f, 1e-5f);
    EXPECT_NEAR(landing.z, 0.0f, 1e-5f);
    const glm::vec3 diagonal = smoke_landing(glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(31.0f, 0.0f, 41.0f), 10.0f);
    EXPECT_NEAR(glm::distance(glm::vec2(diagonal.x, diagonal.z), glm::vec2(1.0f, 1.0f)), 10.0f, 1e-4f);
}
