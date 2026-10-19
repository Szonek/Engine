#include "sandbox_scene.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

// The hierarchy: a game object's children are the game objects whose parent component points at it, without a size
// limit. The folder component only organizes the editor's hierarchy view.
namespace
{
using HierarchyTest = scenario::GameplayTest;

void set_parent(engine_game_object_t child, engine_game_object_t parent)
{
    auto pc = engineGameObjectAddParentComponent(child);
    pc.parent = parent;
    engineGameObjectUpdateParentComponent(child, &pc);
}

std::vector<engine_game_object_t> children_of(engine_game_object_t go)
{
    std::vector<engine_game_object_t> ret;
    for (size_t i = 0; i < engineGameObjectGetChildrenCount(go); i++)
    {
        ret.push_back(engineGameObjectGetChild(go, i));
    }
    return ret;
}

TEST_F(HierarchyTest, ParentCanHaveMoreChildrenThanTheOldFixedLimit)
{
    load_sandbox_scene();
    const auto parent = engineGameObjectCreate();
    std::vector<engine_game_object_t> expected;
    for (int i = 0; i < 300; i++)
    {
        const auto child = engineGameObjectCreate();
        set_parent(child, parent);
        expected.push_back(child);
    }
    EXPECT_EQ(engineGameObjectGetChildrenCount(parent), expected.size());
    // in the order they were parented
    EXPECT_EQ(children_of(parent), expected);
}

TEST_F(HierarchyTest, GameObjectWithoutChildrenHasNone)
{
    load_sandbox_scene();
    const auto go = engineGameObjectCreate();
    EXPECT_EQ(engineGameObjectGetChildrenCount(go), 0u);
    EXPECT_EQ(engineGameObjectGetChild(go, 0), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(HierarchyTest, ChildOutOfRangeIsInvalid)
{
    load_sandbox_scene();
    const auto parent = engineGameObjectCreate();
    set_parent(engineGameObjectCreate(), parent);
    EXPECT_NE(engineGameObjectGetChild(parent, 0), ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_EQ(engineGameObjectGetChild(parent, 1), ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(HierarchyTest, RemovingTheParentComponentRemovesTheChild)
{
    load_sandbox_scene();
    const auto parent = engineGameObjectCreate();
    const auto first = engineGameObjectCreate();
    const auto second = engineGameObjectCreate();
    const auto third = engineGameObjectCreate();
    set_parent(first, parent);
    set_parent(second, parent);
    set_parent(third, parent);

    engineGameObjectRemoveParentComponent(second);
    EXPECT_EQ(children_of(parent), (std::vector<engine_game_object_t>{ first, third }));
}

TEST_F(HierarchyTest, DestroyingAChildRemovesItFromItsParent)
{
    load_sandbox_scene();
    const auto parent = engineGameObjectCreate();
    const auto first = engineGameObjectCreate();
    const auto second = engineGameObjectCreate();
    set_parent(first, parent);
    set_parent(second, parent);

    engineGameObjectDestroy(first);
    EXPECT_EQ(children_of(parent), (std::vector<engine_game_object_t>{ second }));
}

TEST_F(HierarchyTest, DestroyingAParentDetachesItsChildren)
{
    load_sandbox_scene();
    const auto parent = engineGameObjectCreate();
    std::vector<engine_game_object_t> children;
    for (int i = 0; i < 200; i++)
    {
        children.push_back(engineGameObjectCreate());
        set_parent(children.back(), parent);
    }

    engineGameObjectDestroy(parent);
    for (const auto child : children)
    {
        EXPECT_TRUE(engineGameObjectIsValid(child));
        EXPECT_FALSE(engineGameObjectHasParentComponent(child));
    }
}

TEST_F(HierarchyTest, UpdatingTheParentComponentWithTheSameParentDoesNotListTheChildTwice)
{
    load_sandbox_scene();
    const auto parent = engineGameObjectCreate();
    const auto child = engineGameObjectCreate();
    set_parent(child, parent);

    const auto pc = engineGameObjectGetParentComponent(child);
    engineGameObjectUpdateParentComponent(child, &pc);
    EXPECT_EQ(engineGameObjectGetChildrenCount(parent), 1u);
}

TEST_F(HierarchyTest, FolderComponentIsEmptyWhenAdded)
{
    load_sandbox_scene();
    const auto go = engineGameObjectCreate();
    EXPECT_FALSE(engineGameObjectHasFolderComponent(go));

    auto folder = engineGameObjectAddFolderComponent(go);
    EXPECT_TRUE(engineGameObjectHasFolderComponent(go));
    EXPECT_STREQ(folder.path, "");

    std::strcpy(folder.path, "environment/walls");
    engineGameObjectUpdateFolderComponent(go, &folder);
    EXPECT_STREQ(engineGameObjectGetFolderComponent(go).path, "environment/walls");

    engineGameObjectRemoveFolderComponent(go);
    EXPECT_FALSE(engineGameObjectHasFolderComponent(go));
}

TEST_F(HierarchyTest, FolderDoesNotMakeAParent)
{
    load_sandbox_scene();
    const auto go = engineGameObjectCreate();
    auto folder = engineGameObjectAddFolderComponent(go);
    std::strcpy(folder.path, "lights");
    engineGameObjectUpdateFolderComponent(go, &folder);
    EXPECT_FALSE(engineGameObjectHasParentComponent(go));
}
}  // namespace
