#include "editor_gizmo_visibility.h"

#include <gtest/gtest.h>

// each view draws the game's gizmos by its own toggle
TEST(EditorGizmoVisibility, EditorViewUsesItsToggle)
{
    EXPECT_TRUE(engine::are_gizmos_visible(true, true, false));
    EXPECT_FALSE(engine::are_gizmos_visible(true, false, true));
}

TEST(EditorGizmoVisibility, GameViewUsesItsToggle)
{
    EXPECT_TRUE(engine::are_gizmos_visible(false, false, true));
    EXPECT_FALSE(engine::are_gizmos_visible(false, true, false));
}
