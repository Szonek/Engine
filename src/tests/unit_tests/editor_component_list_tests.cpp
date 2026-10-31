#include "editor_component_list.h"

#include <gtest/gtest.h>

namespace
{
const std::vector<engine::component_choice_t> CHOICES = {
    { "Mesh", "Rendering" },
    { "Collider", "Physics" },
    { "Light", "Rendering" },
    { "Rigid Body", "Physics" },
    { "enemy", "Game" },
};
}  // namespace

TEST(EditorComponentList, ContainsIgnoringCase)
{
    EXPECT_TRUE(engine::contains_ignoring_case("Rigid Body", "body"));
    EXPECT_TRUE(engine::contains_ignoring_case("Rigid Body", "RIGID"));
    EXPECT_TRUE(engine::contains_ignoring_case("Rigid Body", ""));
    EXPECT_FALSE(engine::contains_ignoring_case("Rigid Body", "bodies"));
    EXPECT_FALSE(engine::contains_ignoring_case("Mesh", "Meshes"));
}

TEST(EditorComponentList, NoFilterGroupsEverythingByCategoryInOrder)
{
    const auto groups = engine::group_component_choices(CHOICES, "");
    ASSERT_EQ(groups.size(), 3u);
    EXPECT_EQ(groups[0].category, "Rendering");
    EXPECT_EQ(groups[0].choices, (std::vector<std::size_t>{ 0, 2 }));
    EXPECT_EQ(groups[1].category, "Physics");
    EXPECT_EQ(groups[1].choices, (std::vector<std::size_t>{ 1, 3 }));
    EXPECT_EQ(groups[2].category, "Game");
    EXPECT_EQ(groups[2].choices, (std::vector<std::size_t>{ 4 }));
    EXPECT_EQ(engine::listed_component_choices(groups), (std::vector<std::size_t>{ 0, 2, 1, 3, 4 }));
}

TEST(EditorComponentList, FilterMatchesNamesAndCategories)
{
    // "Collider" and "Light": the categories in the order their first match comes
    const auto by_name = engine::group_component_choices(CHOICES, "li");
    ASSERT_EQ(by_name.size(), 2u);
    EXPECT_EQ(by_name[0].category, "Physics");
    EXPECT_EQ(by_name[0].choices, (std::vector<std::size_t>{ 1 }));
    EXPECT_EQ(by_name[1].category, "Rendering");
    EXPECT_EQ(by_name[1].choices, (std::vector<std::size_t>{ 2 }));

    const auto by_category = engine::group_component_choices(CHOICES, "physics");
    ASSERT_EQ(by_category.size(), 1u);
    EXPECT_EQ(by_category[0].choices, (std::vector<std::size_t>{ 1, 3 }));

    EXPECT_TRUE(engine::group_component_choices(CHOICES, "xyz").empty());
}
