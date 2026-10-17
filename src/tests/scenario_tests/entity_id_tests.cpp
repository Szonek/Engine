#include "scenario_harness.h"

#include <scenes/scene_test.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Persistent game object IDs: the same level built twice gives every game object the same ID, so files can
// reference game objects by it.
namespace
{
using EntityIdTest = scenario::ScenarioTest;

struct NamedId
{
    engine_game_object_id_t id;
    std::string name;
    bool operator==(const NamedId&) const = default;
};

// every named game object of the active scene, ordered by ID
std::vector<NamedId> named_ids()
{
    engine_component_view_t view{};
    engineComponentViewCreate(&view);
    engineComponentViewAttachNameComponent(view);
    engine_component_iterator_t it{};
    engine_component_iterator_t end{};
    engineComponentViewCreateBeginComponentIterator(view, &it);
    engineComponentViewCreateEndComponentIterator(view, &end);

    std::vector<NamedId> out;
    while (!engineComponentIteratorCheckEqual(it, end))
    {
        const auto go = engineComponentIteratorGetGameObject(it);
        out.push_back({ engineGameObjectGetId(go), engineGameObjectGetNameComponent(go).name });
        engineComponentIteratorNext(it);
    }
    engineComponentIteratorDelete(it);
    engineComponentIteratorDelete(end);
    engineComponentViewDestroy(view);

    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return out;
}

TEST_F(EntityIdTest, EveryGameObjectHasAUniqueIdItCanBeFoundBy)
{
    load_scene<project_c::TestScene>();
    step();

    const auto ids = named_ids();
    ASSERT_FALSE(ids.empty());
    std::set<engine_game_object_id_t> unique;
    for (const auto& [id, name] : ids)
    {
        EXPECT_NE(id, ENGINE_INVALID_GAME_OBJECT_PERSISTENT_ID) << name;
        EXPECT_TRUE(unique.insert(id).second) << "duplicate ID " << id << " (" << name << ")";
        const auto go = engineGameObjectFindById(id);
        ASSERT_NE(go, ENGINE_INVALID_GAME_OBJECT_ID) << name;
        EXPECT_EQ(engineGameObjectGetId(go), id) << name;
    }
}

TEST_F(EntityIdTest, LevelBuiltTwiceGetsTheSameIds)
{
    load_scene<project_c::TestScene>();
    step();
    const auto first = named_ids();
    scenario::app().unregister_scene(project_c::TestScene::get_name());

    load_scene<project_c::TestScene>();
    step();
    const auto second = named_ids();

    ASSERT_FALSE(first.empty());
    EXPECT_EQ(first, second);
}

TEST_F(EntityIdTest, DestroyedGameObjectCanNoLongerBeFound)
{
    load_scene<project_c::TestScene>();
    step();
    const auto go = engineGameObjectCreate();
    const auto id = engineGameObjectGetId(go);
    ASSERT_NE(id, ENGINE_INVALID_GAME_OBJECT_PERSISTENT_ID);
    EXPECT_EQ(engineGameObjectFindById(id), go);

    engineGameObjectDestroy(go);
    EXPECT_EQ(engineGameObjectFindById(id), ENGINE_INVALID_GAME_OBJECT_ID);
    // IDs are not reused
    const auto next = engineGameObjectCreate();
    EXPECT_GT(engineGameObjectGetId(next), id);
    engineGameObjectDestroy(next);
}
}  // namespace
