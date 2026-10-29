#include "hierarchy.h"
#include "components_internals/children_internal_component.h"
#include "engine.h"

#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include <cstdint>
#include <vector>

namespace
{
// a registry with the hierarchy listeners, parented the C API way: the component is added, then updated
class HierarchyTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        engine::connect_hierarchy(registry_);
        // entity 0 is the invalid game object ID: reserved like the scene does
        registry_.create();
    }

    void set_parent(entt::entity child, entt::entity parent)
    {
        if (!registry_.all_of<engine_parent_component_t>(child))
        {
            registry_.emplace<engine_parent_component_t>(child);
        }
        engine_parent_component_t component{};
        component.parent = static_cast<std::uint32_t>(parent);
        registry_.replace<engine_parent_component_t>(child, component);
    }

    std::vector<entt::entity> children(entt::entity parent) const
    {
        const auto* component = registry_.try_get<engine::children_internal_component_t>(parent);
        return component ? component->children : std::vector<entt::entity>{};
    }

    entt::registry registry_;
};
}  // namespace

TEST_F(HierarchyTest, ChildrenAreListedInTheOrderTheyWereParented)
{
    const auto parent = registry_.create();
    const auto a = registry_.create();
    const auto b = registry_.create();
    set_parent(b, parent);
    set_parent(a, parent);
    EXPECT_EQ(children(parent), (std::vector<entt::entity>{ b, a }));
}

TEST_F(HierarchyTest, UpdateKeepingTheParentKeepsTheChildsPlace)
{
    const auto parent = registry_.create();
    const auto a = registry_.create();
    const auto b = registry_.create();
    set_parent(a, parent);
    set_parent(b, parent);
    set_parent(a, parent);
    EXPECT_EQ(children(parent), (std::vector<entt::entity>{ a, b }));
}

TEST_F(HierarchyTest, ReparentingMovesTheChildToTheNewParentsList)
{
    const auto old_parent = registry_.create();
    const auto new_parent = registry_.create();
    const auto child = registry_.create();
    set_parent(child, old_parent);
    set_parent(child, new_parent);
    EXPECT_TRUE(children(old_parent).empty());
    EXPECT_EQ(children(new_parent), (std::vector<entt::entity>{ child }));
}

TEST_F(HierarchyTest, RemovingTheParentComponentUnlistsTheChild)
{
    const auto parent = registry_.create();
    const auto child = registry_.create();
    set_parent(child, parent);
    registry_.erase<engine_parent_component_t>(child);
    EXPECT_TRUE(children(parent).empty());
    // parented again afterwards, it is listed once
    set_parent(child, parent);
    EXPECT_EQ(children(parent), (std::vector<entt::entity>{ child }));
}

TEST_F(HierarchyTest, DestroyedChildLeavesTheList)
{
    const auto parent = registry_.create();
    const auto a = registry_.create();
    const auto b = registry_.create();
    set_parent(a, parent);
    set_parent(b, parent);
    registry_.destroy(a);
    EXPECT_EQ(children(parent), (std::vector<entt::entity>{ b }));
}

TEST_F(HierarchyTest, DestroyedParentLeavesItsChildrenWithoutParent)
{
    const auto parent = registry_.create();
    const auto child = registry_.create();
    set_parent(child, parent);
    registry_.destroy(parent);
    ASSERT_TRUE(registry_.valid(child));
    EXPECT_FALSE(registry_.all_of<engine_parent_component_t>(child));
}

// A scene prefab parents its game objects to the instance's root, then reads the file's parents over it: destroying the
// old parent (the root) afterwards neither touches the moved child nor fails on it.
TEST_F(HierarchyTest, DestroyingTheOldParentLeavesAMovedChildAlone)
{
    const auto old_parent = registry_.create();
    const auto new_parent = registry_.create();
    const auto child = registry_.create();
    set_parent(new_parent, old_parent);
    set_parent(child, old_parent);
    set_parent(child, new_parent);
    // the child is gone from the old parent's list, so destroying it (children first, like Scene::destroy_entity)
    // reaches the child only through its new parent
    EXPECT_EQ(children(old_parent), (std::vector<entt::entity>{ new_parent }));
    registry_.destroy(child);
    registry_.destroy(new_parent);
    registry_.destroy(old_parent);
    EXPECT_FALSE(registry_.valid(old_parent));
}

TEST_F(HierarchyTest, ReparentedChildKeepsItsNewParentWhenTheOldOneIsDestroyed)
{
    const auto old_parent = registry_.create();
    const auto new_parent = registry_.create();
    const auto child = registry_.create();
    set_parent(child, old_parent);
    set_parent(child, new_parent);
    registry_.destroy(old_parent);
    ASSERT_TRUE(registry_.all_of<engine_parent_component_t>(child));
    EXPECT_EQ(static_cast<entt::entity>(registry_.get<engine_parent_component_t>(child).parent), new_parent);
    EXPECT_EQ(children(new_parent), (std::vector<entt::entity>{ child }));
}
