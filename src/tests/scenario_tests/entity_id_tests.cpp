#include "sandbox_scene.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Persistent game object IDs: the same scene built twice gives every game object the same ID, so files can
// reference game objects by it.
namespace
{
class EntityIdTest : public scenario::GameplayTest
{
protected:
    // A bit of everything the level is made of. Instancing a prefab also creates and destroys its joint nodes,
    // the creation history that could disturb the IDs.
    void load_small_scene()
    {
        auto* scene = load_sandbox_scene();
        scene->build().floor_area(-2.0f, -2.0f, 2.0f, 2.0f);
        scene->build().wall(-2.5f, 0.0f, 90.0f);
        scene->build().wall(2.5f, 0.0f, 90.0f);
        scene->build().player(0.0f, 0.0f);
        scene->build().enemy(1.0f, 1.0f);
        scene->build().enemy(-1.0f, 1.0f);
        scene->build().chest(0.0f, 0.0f, -1.5f);
        scene->build().weapon_on_ground(1.0f, -1.0f);
        step();
    }
};

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
    load_small_scene();

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

TEST_F(EntityIdTest, SceneBuiltTwiceGetsTheSameIds)
{
    load_small_scene();
    const auto first = named_ids();
    scenario::app().unregister_scene(scenario::SandboxScene::get_name());

    load_small_scene();
    const auto second = named_ids();

    ASSERT_FALSE(first.empty());
    EXPECT_EQ(first, second);
}

TEST_F(EntityIdTest, DestroyedGameObjectCanNoLongerBeFound)
{
    load_sandbox_scene();
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
