#pragma once
#include "prefab.h"

#include <memory>
#include <string>
#include <string_view>

namespace engine
{
// A scene file used as a prefab (like Godot's instanced scenes, Unity's nested prefabs or Unreal's Level Instances),
// named by its path under assets/scenes ("rooms/room_a.json"), so a level can place a room any number of times, each
// with its own transform. Instantiating it loads the file's game objects into the scene under the instance's root:
// the game object which places it, which carries its placement. A Godot or Unity prefab has one root of its own; here
// the placing game object stands for it, so any scene file can be placed, a level's hundreds of top-level game
// objects too. The file's game objects without a parent are parented to the root.
// A game object's node path is its persistent ID in the file ("12"), that of a game object of a prefab instance in the
// file is the ID of the instance's root in the file and its node path there ("12/Body", "12/5/Body" when it is in a
// scene prefab placed there), so overrides reach into nested instances. What the file references inside itself (a
// house's roof props) is referenced inside each instance.
// The file is read and checked once, when the prefab is loaded; a change to it shows up in the instances loaded after.
// The assets it uses are held by the scene the instance is in (SceneAssets), as for a scene file.
// A file which places itself, directly or through other files, would never end: the instance inside itself is not
// created (logged).
class ScenePrefab : public Prefab
{
public:
    // nullptr (logged) when the file can't be read or isn't a scene file
    static std::unique_ptr<ScenePrefab> load(std::string_view asset_id);

    const std::string& get_name() const override { return name_; }

    // A root it creates gets the file's name (its stem) and a transform, like a model's root.
    std::vector<PrefabInstanceNode> instantiate(Scene& scene, Instance instance, entt::entity root = entt::null, const EntityHandles* node_handles = nullptr,
        const InstanceIds* ids = nullptr) const override;

private:
    ScenePrefab(std::string_view name, std::string&& json);

    std::string name_;
    std::string json_;
    // set while its file is loaded into an instance: instantiating it again then would place it inside itself
    mutable bool instantiating_ = false;
};
}  // namespace engine
