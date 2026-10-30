#include "components_internals/editor_object_internal_component.h"
#include "components_internals/joint_attachment_internal_component.h"
#include "serialization/builtin_types.h"
#include "serialization/game_types.h"
#include "serialization/scene_serializer.h"
#include "test_logger.h"

#include <engine.h>

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <rapidjson/document.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using engine::AssetKind;
using engine::EntityIdRegistry;

// asset IDs <-> handles without an application; nothing is ever loaded
class FakeAssets : public engine::IAssetResolver
{
public:
    void add(AssetKind kind, const std::string& id, std::uint32_t handle)
    {
        ids_[{ kind, handle }] = id;
        handles_[{ kind, id }] = handle;
    }

    std::string get_asset_id(AssetKind kind, std::uint32_t handle) const override
    {
        const auto it = ids_.find({ kind, handle });
        return it == ids_.end() ? std::string{} : it->second;
    }

    std::uint32_t resolve(AssetKind kind, std::string_view asset_id) override
    {
        const auto it = handles_.find({ kind, std::string(asset_id) });
        return it == handles_.end() ? ENGINE_INVALID_OBJECT_HANDLE : it->second;
    }

    // a data asset holding a value of `type`
    void add_data_asset(const std::string& id, std::uint32_t handle, const engine::TypeInfo* type)
    {
        add(AssetKind::eDataAsset, id, handle);
        data_types_[handle] = type;
    }

    const engine::TypeInfo* get_data_asset_type(std::uint32_t handle) const override
    {
        const auto it = data_types_.find(handle);
        return it == data_types_.end() ? nullptr : it->second;
    }

private:
    std::map<std::pair<AssetKind, std::uint32_t>, std::string> ids_;
    std::map<std::pair<AssetKind, std::string>, std::uint32_t> handles_;
    std::map<std::uint32_t, const engine::TypeInfo*> data_types_;
};

// what the scene's initializers do for these components (the defaults a missing field keeps)
void init_transform(entt::registry& registry, entt::entity entity)
{
    auto& t = registry.get<engine_transform_component_t>(entity);
    t = {};
    t.scale[0] = t.scale[1] = t.scale[2] = 1.0f;
    t.rotation[3] = 1.0f;
}

void init_light(entt::registry& registry, entt::entity entity)
{
    auto& l = registry.get<engine_light_component_t>(entity);
    l = {};
    l.type = ENGINE_LIGHT_TYPE_POINT;
    l.point.constant = 1.0f;
    l.point.linear = 0.09f;
    l.point.quadratic = 0.032f;
}

// like the scene: the joint attachment's joint name is the engine's string, owned by an internal component; an update
// carrying another string copies its text
void construct_joint_attachment(entt::registry& registry, entt::entity entity)
{
    auto& owned = registry.emplace<engine::joint_attachment_internal_component_t>(entity);
    registry.get<engine_joint_attachment_component_t>(entity).joint_name = owned.joint_name.get();
}

void update_joint_attachment(entt::registry& registry, entt::entity entity)
{
    auto& comp = registry.get<engine_joint_attachment_component_t>(entity);
    auto* owned = registry.get<engine::joint_attachment_internal_component_t>(entity).joint_name.get();
    if (comp.joint_name != owned)
    {
        if (comp.joint_name)
        {
            owned->str = comp.joint_name->str;
        }
        comp.joint_name = owned;
    }
}

// Prefab instances without an application: the model "models/arm.gltf" has a root node and the nodes "Upper" (with a
// box collider) and "Upper/Hand" (with a mesh and `hand_material`, and attached to the joint `hand_joint` of the root
// when it is set); each node gets a name and a transform, the others
// are children of the root, like a prefab's instance. Scene prefabs are the files in `scene_files`, loaded into their
// instances like ScenePrefab does.
class FakePrefabs : public engine::IPrefabInstances
{
public:
    static constexpr const char* ARM = "models/arm.gltf";

    FakePrefabs(entt::registry& registry, EntityIdRegistry& ids) : registry_(registry), ids_(ids) {}

    const engine::PrefabInstanceInfo* find_instance(entt::entity root) const override
    {
        const auto it = instances_.find(root);
        return it == instances_.end() ? nullptr : &it->second;
    }

    const engine::PrefabInstanceInfo* instantiate(std::string_view prefab_asset_id, entt::entity root, const engine::EntityHandles& node_handles,
        const engine::InstanceIds& ids) override
    {
        if (const auto file = scene_files.find(std::string(prefab_asset_id)); file != scene_files.end())
        {
            return instantiate_scene(file->first, file->second, root, node_handles, ids);
        }
        if (prefab_asset_id != ARM)
        {
            return nullptr;
        }
        engine::PrefabInstanceInfo info{ ARM, {} };
        for (const std::string path : { "", "Upper", "Upper/Hand" })
        {
            auto entity = root;
            if (!path.empty())
            {
                // like Scene::create_prefab_node_entity
                entity = ids.root_id == EntityIdRegistry::INVALID_ID ? registry_.create() : ids_.create_hashed(EntityIdRegistry::hashed_id(ids.root_id, ids.prefix + path), &node_handles);
                registry_.emplace<engine_parent_component_t>(entity, static_cast<engine_game_object_t>(root));
            }
            engine_name_component_t name{};
            std::strcpy(name.name, path.empty() ? "Arm" : path.substr(path.rfind('/') + 1).c_str());
            registry_.emplace_or_replace<engine_name_component_t>(entity, name);
            registry_.emplace_or_replace<engine_transform_component_t>(entity);
            if (path == "Upper")
            {
                registry_.emplace<engine_collider_component_t>(entity, upper_collider);
            }
            if (path == "Upper/Hand")
            {
                if (hand_has_mesh)
                {
                    registry_.emplace<engine_mesh_component_t>(entity, engine_mesh_component_t{ 7, false });
                }
                registry_.emplace<engine_material_component_t>(entity, hand_material);
                if (!hand_joint.empty())
                {
                    // like the model prefab: added, then its joint name set with engineStringSet
                    auto& attachment = registry_.emplace<engine_joint_attachment_component_t>(entity);
                    attachment.skin = static_cast<engine_game_object_t>(root);
                    attachment.joint_name->str = hand_joint;
                }
            }
            info.nodes.push_back({ path, entity, {} });
        }
        return add_instance(root, std::move(info));
    }

    // an instance whose root has the next persistent ID (and its prefab's nodes the hashed IDs of their paths)
    const engine::PrefabInstanceInfo* instantiate(std::string_view prefab_asset_id, entt::entity root)
    {
        return instantiate(prefab_asset_id, root, {}, engine::InstanceIds{ ids_.get_id(root), {} });
    }

    entt::entity node(entt::entity root, std::string_view path) const
    {
        for (const auto& n : instances_.at(root).nodes)
        {
            if (n.path == path)
            {
                return n.entity;
            }
        }
        return entt::null;
    }

    const engine::TypeRegistry* types = nullptr;
    engine::IAssetResolver* assets = nullptr;
    // scene prefabs: asset ID -> the file
    std::map<std::string, std::string> scene_files;
    // what the model gives its nodes (a test changes it to change the model)
    engine_material_component_t hand_material{ { 1.0f, 1.0f, 1.0f, 1.0f }, 3, 16.0f, ENGINE_INVALID_OBJECT_HANDLE };
    bool hand_has_mesh = true;
    std::string hand_joint;
    engine_collider_component_t upper_collider = [] {
        engine_collider_component_t collider{};
        collider.type = ENGINE_COLLIDER_TYPE_BOX;
        collider.collider.box = { { 0.5f, 1.0f, 0.5f } };
        return collider;
    }();

private:
    const engine::PrefabInstanceInfo* add_instance(entt::entity root, engine::PrefabInstanceInfo&& info)
    {
        engine::capture_prefab_components(*types, registry_, info);
        return &instances_.insert_or_assign(root, std::move(info)).first->second;
    }

    // like ScenePrefab::instantiate
    const engine::PrefabInstanceInfo* instantiate_scene(const std::string& name, const std::string& json, entt::entity root, const engine::EntityHandles& node_handles,
        const engine::InstanceIds& ids)
    {
        if (loading_.contains(name))
        {
            test_logger::messages().push_back(fmt::format("Scene prefab {} places itself.", name));
            return nullptr;
        }
        engine::SceneInstanceTarget target;
        target.root = root;
        target.ids = ids;
        target.node_handles = &node_handles;
        engine::SceneLoadOptions options;
        options.instance = &target;
        loading_.insert(name);
        const auto loaded = engine::load_scene(json, name, *types, registry_, ids_, *assets, *this, options);
        loading_.erase(name);
        if (!loaded)
        {
            return nullptr;
        }
        engine::PrefabInstanceInfo info{ name, { { "", root, {} } } };
        std::ranges::move(target.nodes, std::back_inserter(info.nodes));
        return add_instance(root, std::move(info));
    }

    entt::registry& registry_;
    EntityIdRegistry& ids_;
    std::map<entt::entity, engine::PrefabInstanceInfo> instances_;
    std::set<std::string> loading_;
};

// a registry set up like a scene's: entity 0 reserved, persistent IDs, the initializers above
struct World
{
    World()
    {
        // entity 0, like the scene's reserved one
        [[maybe_unused]] const auto reserved = registry.create();
        registry.on_construct<engine_transform_component_t>().connect<&init_transform>();
        registry.on_construct<engine_light_component_t>().connect<&init_light>();
        registry.on_construct<engine_joint_attachment_component_t>().connect<&construct_joint_attachment>();
        registry.on_update<engine_joint_attachment_component_t>().connect<&update_joint_attachment>();
        registry.on_destroy<engine_joint_attachment_component_t>().connect<&entt::registry::remove<engine::joint_attachment_internal_component_t>>();
    }

    entt::entity create(std::uint64_t id = EntityIdRegistry::INVALID_ID)
    {
        const auto e = registry.create();
        id == EntityIdRegistry::INVALID_ID ? ids.assign_next(e) : ids.assign(e, id);
        return e;
    }

    entt::entity find(std::uint64_t id) const { return ids.find(id); }

    // like the C API: added with its defaults (the initializers run), then updated
    template<typename T>
    void put(entt::entity e, const T& value)
    {
        registry.emplace<T>(e);
        registry.replace<T>(e, value);
    }

    template<typename T>
    const T& get(std::uint64_t id) const { return registry.get<T>(find(id)); }

    entt::registry registry;
    EntityIdRegistry ids{ registry };
    FakePrefabs prefabs{ registry, ids };
};

class SceneFileTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_logger::messages().clear();
        engine::register_builtin_types(types_);
        assets_.add(AssetKind::eGeometry, "models/crate.gltf#Cube", 7);
        assets_.add(AssetKind::eTexture, "models/crate.gltf#wood", 3);
        assets_.add(AssetKind::eTexture, "textures/stone.png", 4);
        assets_.add(AssetKind::eGeometry, "models/knight.glb#Body", 8);
        assets_.add(AssetKind::eSkin, "models/knight.glb#Rig", 2);
        assets_.add(AssetKind::eAnimationSet, "models/knight.glb#animations", 5);
        assets_.add(AssetKind::ePrefab, "rooms/guard_post.json", 6);
    }

    std::string save(World& world)
    {
        world.prefabs.types = &types_;
        world.prefabs.assets = &assets_;
        return engine::save_scene(types_, world.registry, world.ids, assets_, world.prefabs);
    }
    bool load(World& world, std::string_view json, const engine::SceneLoadOptions& options = {})
    {
        world.prefabs.types = &types_;
        world.prefabs.assets = &assets_;
        return engine::load_scene(json, "test.json", types_, world.registry, world.ids, assets_, world.prefabs, options);
    }
    // the game object and what is parented under it, saved the way the editor's undo of a delete does: the file and how
    // to load it back
    struct Snapshot
    {
        std::string file;
        engine::SceneLoadOptions restore;
    };
    Snapshot snapshot(World& world, entt::entity root)
    {
        world.prefabs.types = &types_;
        world.prefabs.assets = &assets_;
        std::vector<entt::entity> subtree{ root };
        for (std::size_t i = 0; i < subtree.size(); i++)
        {
            for (const auto [child, parent] : world.registry.view<const engine_parent_component_t>().each())
            {
                if (static_cast<entt::entity>(parent.parent) == subtree[i])
                {
                    subtree.push_back(child);
                }
            }
        }
        Snapshot result{ engine::save_game_objects(types_, world.registry, world.ids, assets_, world.prefabs, subtree), {} };
        for (const auto entity : subtree)
        {
            if (const auto id = world.ids.get_id(entity); id != EntityIdRegistry::INVALID_ID)
            {
                result.restore.handles[id] = entity;
            }
        }
        result.restore.find_references_outside_file = true;
        return result;
    }
    // an instance of FakePrefabs::ARM, its root with the next persistent ID
    entt::entity instantiate_arm(World& world)
    {
        world.prefabs.types = &types_;
        world.prefabs.assets = &assets_;
        const auto root = world.create();
        world.prefabs.instantiate(FakePrefabs::ARM, root);
        return root;
    }

    // a game object with every saved component, all values different from the defaults
    void build_full_scene(World& w)
    {
        const auto root = w.create();
        engine_name_component_t name{};
        std::strcpy(name.name, "crate");
        w.put<engine_name_component_t>(root, name);
        engine_folder_component_t folder{};
        std::strcpy(folder.path, "props/boxes");
        w.put<engine_folder_component_t>(root, folder);
        w.put<engine_transform_component_t>(root, engine_transform_component_t{ { 0.1f, -2.5f, 1e-7f }, { 2.0f, 2.0f, 0.5f }, { 0.0f, 0.70710677f, 0.0f, 0.70710677f } });
        w.put<engine_mesh_component_t>(root, engine_mesh_component_t{ 7, true });
        engine_material_component_t material{};
        material = { { 0.2f, 0.4f, 0.6f, 1.0f }, 3, 32.0f, 4 };
        w.put<engine_material_component_t>(root, material);
        w.put<engine_rigid_body_component_t>(root, engine_rigid_body_component_t{ ENGINE_RIGID_BODY_TYPE_DYNAMIC, 12.5f });
        engine_collider_component_t collider{};
        collider.type = ENGINE_COLLIDER_TYPE_BOX;
        collider.is_trigger = true;
        collider.disable = true;
        collider.collider.box = { { 1.0f, 2.0f, 3.0f } };
        collider.bounciness = 0.25f;
        collider.friction_static = 0.75f;
        w.put<engine_collider_component_t>(root, collider);

        const auto child = w.create();
        w.put<engine_transform_component_t>(child, engine_transform_component_t{ { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } });
        w.put<engine_parent_component_t>(child, engine_parent_component_t{ static_cast<engine_game_object_t>(root) });
        engine_light_component_t light{};
        light.type = ENGINE_LIGHT_TYPE_SPOT;
        light.intensity = { { 0.1f, 0.1f, 0.1f }, { 1.0f, 0.9f, 0.8f }, { 1.0f, 1.0f, 1.0f } };
        light.spot = { { 0.0f, -1.0f, 0.0f }, 12.5f, 17.5f, 1.0f, 0.09f, 0.032f };
        w.put<engine_light_component_t>(child, light);
        engine_collider_component_t compound{};
        compound.type = ENGINE_COLLIDER_TYPE_COMPOUND;
        compound.collider.compound.children[0].type = ENGINE_COLLIDER_TYPE_SPHERE;
        compound.collider.compound.children[0].transform[1] = 0.5f;
        compound.collider.compound.children[0].rotation_quaternion[3] = 1.0f;
        compound.collider.compound.children[0].collider.sphere.radius = 0.3f;
        w.put<engine_collider_component_t>(child, compound);

        const auto level = w.create();
        w.put<engine_nav_surface_component_t>(level, engine_nav_surface_component_t{ 0.4f, 1.8f, 40.0f, 0.3f, 0.2f, 0.1f });
        w.put<engine_nav_modifier_component_t>(level, engine_nav_modifier_component_t{ ENGINE_NAV_MODIFIER_TYPE_NOT_WALKABLE });
        w.put<engine_character_controller_component_t>(level, engine_character_controller_component_t{ 0.3f, 1.7f, 0.25f, 50.0f });

        const auto camera = w.create();
        engine_camera_component_t cam{};
        cam.enabled = true;
        cam.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
        cam.type_union.perspective_fov = 60.0f;
        cam.target[2] = -1.0f;
        cam.viewport_rect = { 0.0f, 0.0f, 1.0f, 0.5f };
        cam.direction.up[1] = 1.0f;
        cam.pitch = -30.0f;
        cam.clip_plane_near = 0.1f;
        cam.clip_plane_far = 250.0f;
        w.put<engine_camera_component_t>(camera, cam);

        // a character: its skin, deforming its body, played by its animation controller, and a sword in its hand
        const auto character = w.create();
        w.put<engine_skin_component_t>(character, engine_skin_component_t{ 2 });
        w.put<engine_skinned_mesh_component_t>(character, engine_skinned_mesh_component_t{ 8, static_cast<engine_game_object_t>(character), true });
        engine_animation_controller_component_t animation{};
        animation.animations = 5;
        animation.layers_count = 2;
        animation.layers[0] = { "base", ENGINE_ANIMATION_LAYER_MODE_OVERRIDE, 1.0f, true, 0, {} };
        animation.layers[1] = { "upper_body", ENGINE_ANIMATION_LAYER_MODE_ADDITIVE, 0.5f, false, 2, {} };
        animation.layers[1].joint_weights[0] = { "spine", 1.0f };
        animation.layers[1].joint_weights[1] = { "neck", 0.25f };
        w.put<engine_animation_controller_component_t>(character, animation);

        const auto sword = w.create();
        w.put<engine_parent_component_t>(sword, engine_parent_component_t{ static_cast<engine_game_object_t>(character) });
        // the caller's string: its text is copied into the engine's
        engine_string_t hand{ "hand.r" };
        w.put<engine_joint_attachment_component_t>(sword, engine_joint_attachment_component_t{ static_cast<engine_game_object_t>(character), &hand });
    }

    engine::TypeRegistry types_;
    FakeAssets assets_;
};

std::size_t count_entities(const entt::registry& registry)
{
    std::size_t count = 0;
    for ([[maybe_unused]] const auto e : registry.view<entt::entity>())
    {
        count++;
    }
    return count;
}

template<typename T>
bool same_bytes(const T& a, const T& b)
{
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}

// -- round trips ---------------------------------------------------------------------------------------------------

TEST_F(SceneFileTest, SaveLoadSaveGivesTheSameFile)
{
    World original;
    build_full_scene(original);
    const auto file = save(original);

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(SceneFileTest, LoadedComponentsHaveTheSavedValues)
{
    World original;
    build_full_scene(original);
    World loaded;
    ASSERT_TRUE(load(loaded, save(original)));

    // whole components without unions or padding compare byte for byte; floats come back exactly (0.1f, 1e-7f)
    EXPECT_TRUE(same_bytes(loaded.get<engine_transform_component_t>(1), original.get<engine_transform_component_t>(1)));
    EXPECT_TRUE(same_bytes(loaded.get<engine_name_component_t>(1), original.get<engine_name_component_t>(1)));
    EXPECT_TRUE(same_bytes(loaded.get<engine_folder_component_t>(1), original.get<engine_folder_component_t>(1)));
    EXPECT_TRUE(same_bytes(loaded.get<engine_nav_surface_component_t>(3), original.get<engine_nav_surface_component_t>(3)));
    EXPECT_TRUE(same_bytes(loaded.get<engine_character_controller_component_t>(3), original.get<engine_character_controller_component_t>(3)));
    EXPECT_EQ(loaded.get<engine_nav_modifier_component_t>(3).type, ENGINE_NAV_MODIFIER_TYPE_NOT_WALKABLE);

    const auto& mesh = loaded.get<engine_mesh_component_t>(1);
    EXPECT_EQ(mesh.geometry, 7u);
    EXPECT_TRUE(mesh.disable);
    const auto& material = loaded.get<engine_material_component_t>(1);
    EXPECT_TRUE(same_bytes(material, original.get<engine_material_component_t>(1)));
    const auto& body = loaded.get<engine_rigid_body_component_t>(1);
    EXPECT_EQ(body.type, ENGINE_RIGID_BODY_TYPE_DYNAMIC);
    EXPECT_EQ(body.mass, 12.5f);

    const auto& box = loaded.get<engine_collider_component_t>(1);
    EXPECT_EQ(box.type, ENGINE_COLLIDER_TYPE_BOX);
    EXPECT_TRUE(box.is_trigger);
    EXPECT_TRUE(box.disable);
    EXPECT_TRUE(same_bytes(box.collider.box, original.get<engine_collider_component_t>(1).collider.box));
    EXPECT_EQ(box.bounciness, 0.25f);
    const auto& compound = loaded.get<engine_collider_component_t>(2).collider.compound.children[0];
    EXPECT_EQ(compound.type, ENGINE_COLLIDER_TYPE_SPHERE);
    EXPECT_EQ(compound.transform[1], 0.5f);
    EXPECT_EQ(compound.collider.sphere.radius, 0.3f);

    const auto& light = loaded.get<engine_light_component_t>(2);
    EXPECT_EQ(light.type, ENGINE_LIGHT_TYPE_SPOT);
    EXPECT_TRUE(same_bytes(light.intensity, original.get<engine_light_component_t>(2).intensity));
    EXPECT_TRUE(same_bytes(light.spot, original.get<engine_light_component_t>(2).spot));

    const auto& camera = loaded.get<engine_camera_component_t>(4);
    const auto& saved_camera = original.get<engine_camera_component_t>(4);
    EXPECT_TRUE(camera.enabled);
    EXPECT_EQ(camera.type, ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE);
    EXPECT_EQ(camera.type_union.perspective_fov, 60.0f);
    EXPECT_TRUE(same_bytes(camera.target, saved_camera.target));
    EXPECT_TRUE(same_bytes(camera.viewport_rect, saved_camera.viewport_rect));
    EXPECT_TRUE(same_bytes(camera.direction, saved_camera.direction));
    EXPECT_EQ(camera.pitch, -30.0f);
    EXPECT_EQ(camera.clip_plane_far, 250.0f);

    // the reference points at the loaded game object with the saved ID
    EXPECT_EQ(loaded.get<engine_parent_component_t>(2).parent, static_cast<engine_game_object_t>(loaded.find(1)));

    const auto character = static_cast<engine_game_object_t>(loaded.find(5));
    EXPECT_EQ(loaded.get<engine_skin_component_t>(5).skin, 2u);
    const auto& skinned_mesh = loaded.get<engine_skinned_mesh_component_t>(5);
    EXPECT_EQ(skinned_mesh.geometry, 8u);
    EXPECT_EQ(skinned_mesh.skin, character);
    EXPECT_TRUE(skinned_mesh.disable);
    const auto& animation = loaded.get<engine_animation_controller_component_t>(5);
    // the layers have padding and bytes after the names' terminators: compared field by field
    EXPECT_TRUE(engine::equal_values(*types_.find("animation_controller"), reinterpret_cast<const std::byte*>(&animation),
        reinterpret_cast<const std::byte*>(&original.get<engine_animation_controller_component_t>(5))));
    EXPECT_EQ(animation.layers_count, 2u);
    EXPECT_EQ(animation.layers[1].mode, ENGINE_ANIMATION_LAYER_MODE_ADDITIVE);
    EXPECT_STREQ(animation.layers[1].joint_weights[1].joint_name, "neck");
    EXPECT_EQ(animation.layers[1].joint_weights[1].weight, 0.25f);
    const auto& attachment = loaded.get<engine_joint_attachment_component_t>(6);
    EXPECT_EQ(attachment.skin, character);
    // the loaded game object's own string, which the engine owns
    EXPECT_EQ(attachment.joint_name, loaded.registry.get<engine::joint_attachment_internal_component_t>(loaded.find(6)).joint_name.get());
    EXPECT_EQ(attachment.joint_name->str, "hand.r");
}

TEST_F(SceneFileTest, FileIsReadable)
{
    World w;
    build_full_scene(w);
    const auto file = save(w);
    EXPECT_NE(file.find(R"("version": 1)"), std::string::npos);
    EXPECT_NE(file.find(R"("position": [0.1, -2.5, 1e-07])"), std::string::npos) << file;
    EXPECT_NE(file.find(R"("geometry": "models/crate.gltf#Cube")"), std::string::npos);
    EXPECT_NE(file.find(R"("specular_texture": "textures/stone.png")"), std::string::npos);
    EXPECT_NE(file.find(R"("parent": 1)"), std::string::npos);
    // a union writes its active case under the tag's value, and only that one
    EXPECT_NE(file.find(R"("type": "box")"), std::string::npos);
    EXPECT_NE(file.find(R"("box": {)"), std::string::npos);
    EXPECT_EQ(file.find(R"("sphere_collider")"), std::string::npos);
    EXPECT_NE(file.find(R"("spot": {)"), std::string::npos);
    EXPECT_EQ(file.find(R"("point": {)"), std::string::npos);
    // skins and animation sets by asset ID, the engine's string as its text
    EXPECT_NE(file.find(R"("skin": "models/knight.glb#Rig")"), std::string::npos);
    EXPECT_NE(file.find(R"("animations": "models/knight.glb#animations")"), std::string::npos);
    EXPECT_NE(file.find(R"("joint_name": "hand.r")"), std::string::npos);
    EXPECT_NE(file.find(R"("mode": "additive")"), std::string::npos);
}

TEST_F(SceneFileTest, GameObjectsAreSortedByPersistentId)
{
    World w;
    w.registry.emplace<engine_transform_component_t>(w.create(9));
    w.registry.emplace<engine_transform_component_t>(w.create(4));
    const auto file = save(w);
    EXPECT_LT(file.find(R"("id": 4)"), file.find(R"("id": 9)"));
}

TEST_F(SceneFileTest, TypesListsTheVersionsOfTheSavedComponents)
{
    World w;
    w.registry.emplace<engine_transform_component_t>(w.create());
    const auto file = save(w);
    EXPECT_NE(file.find(R"("types": {
    "transform": 1
  })"), std::string::npos) << file;
}

TEST_F(SceneFileTest, TransientGameObjectsAreNotSaved)
{
    World w;
    const auto saved = w.create();
    const auto transient = w.registry.create();
    w.registry.emplace<engine_transform_component_t>(transient);
    w.put<engine_parent_component_t>(saved, engine_parent_component_t{ static_cast<engine_game_object_t>(transient) });

    const auto file = save(w);
    EXPECT_EQ(file.find(R"("transform")"), std::string::npos);
    EXPECT_NE(file.find(R"("parent": null)"), std::string::npos);
    EXPECT_TRUE(test_logger::logged("isn't saved (transient or destroyed)"));
}

TEST_F(SceneFileTest, NonFiniteFloatIsSavedAsNull)
{
    World w;
    w.put<engine_rigid_body_component_t>(w.create(), engine_rigid_body_component_t{ ENGINE_RIGID_BODY_TYPE_DYNAMIC, std::numeric_limits<float>::infinity() });
    EXPECT_NE(save(w).find(R"("mass": null)"), std::string::npos);
    EXPECT_TRUE(test_logger::logged("not a finite number"));
}

// -- loading what the file doesn't say ------------------------------------------------------------------------------

TEST_F(SceneFileTest, MissingFieldKeepsTheDefault)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": { "transform": { "position": [1, 2, 3] } } } ] })"));
    const auto& t = w.get<engine_transform_component_t>(1);
    EXPECT_EQ(t.position[2], 3.0f);
    EXPECT_EQ(t.scale[0], 1.0f);
    EXPECT_EQ(t.rotation[3], 1.0f);
    EXPECT_TRUE(test_logger::messages().empty());
}

TEST_F(SceneFileTest, ChangedUnionCaseStartsZeroed)
{
    World w;
    // the default light is a point light (constant 1): a spot light must not read the point light's bytes
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": { "light": { "type": "spot", "spot": { "cut_off": 20 } } } } ] })"));
    const auto& light = w.get<engine_light_component_t>(1);
    EXPECT_EQ(light.type, ENGINE_LIGHT_TYPE_SPOT);
    EXPECT_EQ(light.spot.cut_off, 20.0f);
    EXPECT_EQ(light.spot.direction[0], 0.0f);
    EXPECT_EQ(light.spot.constant, 0.0f);
}

TEST_F(SceneFileTest, UnknownComponentsAndFieldsAreIgnored)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": {
        "teleporter": { "target": 2 },
        "transform": { "position": [1, 2, 3], "skew": 4 } } } ] })"));
    EXPECT_EQ(w.get<engine_transform_component_t>(1).position[0], 1.0f);
    EXPECT_TRUE(test_logger::logged("unknown component teleporter"));
    EXPECT_TRUE(test_logger::logged("unknown field \"skew\""));
}

TEST_F(SceneFileTest, InactiveUnionCaseIsIgnored)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": {
        "collider": { "type": "box", "box": { "size": [1, 1, 1] }, "sphere": { "radius": 2 } } } } ] })"));
    EXPECT_EQ(w.get<engine_collider_component_t>(1).collider.box.size[0], 1.0f);
    EXPECT_TRUE(test_logger::logged("unknown field \"sphere\""));
}

TEST_F(SceneFileTest, WrongValuesKeepTheDefault)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": {
        "rigid_body": { "type": "floating", "mass": "heavy" },
        "mesh": { "disable": 1 },
        "transform": { "position": [1, 2] } } } ] })"));
    const auto& body = w.get<engine_rigid_body_component_t>(1);
    EXPECT_EQ(body.type, ENGINE_RIGID_BODY_TYPE_DYNAMIC);
    EXPECT_EQ(body.mass, 0.0f);
    EXPECT_FALSE(w.get<engine_mesh_component_t>(1).disable);
    // the elements which fit are read
    EXPECT_EQ(w.get<engine_transform_component_t>(1).position[1], 2.0f);
    EXPECT_TRUE(test_logger::logged("unknown value \"floating\""));
    EXPECT_TRUE(test_logger::logged("rigid_body.mass is not a number"));
    EXPECT_TRUE(test_logger::logged("mesh.disable is not true or false"));
    EXPECT_TRUE(test_logger::logged("has 2 elements instead of 3"));
}

TEST_F(SceneFileTest, TooLongStringIsCut)
{
    World w;
    const std::string long_name(200, 'x');
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": { "name": { "name": ")" + long_name + R"(" } } } ] })"));
    EXPECT_EQ(std::string(w.get<engine_name_component_t>(1).name), std::string(127, 'x'));
    EXPECT_TRUE(test_logger::logged("longer than 127 characters"));
}

// -- references ----------------------------------------------------------------------------------------------------

TEST_F(SceneFileTest, ReferenceToALaterGameObjectResolves)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [
        { "id": 1, "components": { "parent": { "parent": 2 } } },
        { "id": 2, "components": {} } ] })"));
    EXPECT_EQ(w.get<engine_parent_component_t>(1).parent, static_cast<engine_game_object_t>(w.find(2)));
}

TEST_F(SceneFileTest, ReferenceToAGameObjectNotInTheFileIsNone)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": { "parent": { "parent": 42 } } } ] })"));
    EXPECT_EQ(w.get<engine_parent_component_t>(1).parent, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_TRUE(test_logger::logged("game object 1: parent.parent references the game object 42 which isn't in the file"));
}

TEST_F(SceneFileTest, UsedIdGetsANewOneAndReferencesFollowIt)
{
    World w;
    const auto existing = w.create(1);
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [
        { "id": 1, "components": { "name": { "name": "root" } } },
        { "id": 2, "components": { "parent": { "parent": 1 } } } ] })"));
    EXPECT_TRUE(test_logger::logged("the persistent ID 1 is used already"));
    // the loaded child's parent is the loaded root, not the game object which had the ID already
    const auto child_parent = static_cast<entt::entity>(w.get<engine_parent_component_t>(2).parent);
    EXPECT_NE(child_parent, existing);
    EXPECT_STREQ(w.registry.get<engine_name_component_t>(child_parent).name, "root");
    EXPECT_GT(w.ids.get_id(child_parent), 2u);
}

// -- some game objects (the editor's undo of a destroy) --------------------------------------------------------------

// destroys the game object and everything parented under it, as the scene does
void destroy_with_children(World& w, entt::entity entity)
{
    std::vector<entt::entity> children;
    for (const auto [child, parent] : w.registry.view<const engine_parent_component_t>().each())
    {
        if (static_cast<entt::entity>(parent.parent) == entity)
        {
            children.push_back(child);
        }
    }
    for (const auto child : children)
    {
        destroy_with_children(w, child);
    }
    w.registry.destroy(entity);
}

TEST_F(SceneFileTest, OnlyTheGivenGameObjectsAreSaved)
{
    World original;
    build_full_scene(original);
    const auto root = original.find(1);
    const auto grandchild = original.create();
    original.put<engine_parent_component_t>(grandchild, engine_parent_component_t{ static_cast<engine_game_object_t>(original.find(2)) });
    const auto transient = original.registry.create();
    original.put<engine_parent_component_t>(transient, engine_parent_component_t{ static_cast<engine_game_object_t>(root) });
    const auto file = snapshot(original, root).file;

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    // the root, its child and grandchild; not the other game objects of the scene, nor the transient child
    EXPECT_EQ(count_entities(loaded.registry), 4u);
    EXPECT_NE(loaded.find(1), entt::entity{ entt::null });
    EXPECT_NE(loaded.find(2), entt::entity{ entt::null });
    EXPECT_NE(loaded.find(original.ids.get_id(grandchild)), entt::entity{ entt::null });
    EXPECT_STREQ(loaded.get<engine_name_component_t>(1).name, "crate");
}

TEST_F(SceneFileTest, RestoredGameObjectsKeepTheirHandlesAndReferencesOutsideTheFile)
{
    World w;
    const auto parent = w.create();
    const auto child = w.create();
    w.put<engine_parent_component_t>(child, engine_parent_component_t{ static_cast<engine_game_object_t>(parent) });
    engine_name_component_t name{};
    std::strcpy(name.name, "child");
    w.put<engine_name_component_t>(child, name);
    const auto grandchild = w.create();
    w.put<engine_parent_component_t>(grandchild, engine_parent_component_t{ static_cast<engine_game_object_t>(child) });
    const auto scene = save(w);
    const auto child_id = w.ids.get_id(child);
    const auto grandchild_id = w.ids.get_id(grandchild);

    const auto saved = snapshot(w, child);
    destroy_with_children(w, child);
    ASSERT_FALSE(w.registry.valid(child));

    ASSERT_TRUE(load(w, saved.file, saved.restore));
    EXPECT_EQ(w.find(child_id), child);
    EXPECT_EQ(w.find(grandchild_id), grandchild);
    EXPECT_EQ(w.registry.get<engine_parent_component_t>(child).parent, static_cast<engine_game_object_t>(parent));
    EXPECT_EQ(w.registry.get<engine_parent_component_t>(grandchild).parent, static_cast<engine_game_object_t>(child));
    EXPECT_STREQ(w.registry.get<engine_name_component_t>(child).name, "child");
    EXPECT_EQ(save(w), scene);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(SceneFileTest, GameObjectWhoseHandleIsTakenDoesntTakeAnotherOnesHandle)
{
    World w;
    const auto parent = w.create();
    const auto child = w.create();
    w.put<engine_parent_component_t>(child, engine_parent_component_t{ static_cast<engine_game_object_t>(parent) });
    const auto grandchild = w.create();
    w.put<engine_parent_component_t>(grandchild, engine_parent_component_t{ static_cast<engine_game_object_t>(child) });
    const auto child_id = w.ids.get_id(child);
    const auto grandchild_id = w.ids.get_id(grandchild);

    const auto saved = snapshot(w, child);
    destroy_with_children(w, child);
    // another game object takes the child's slot (destroyed last, so the next one created reuses it)
    const auto other = w.registry.create();
    ASSERT_EQ(entt::to_entity(other), entt::to_entity(child));

    ASSERT_TRUE(load(w, saved.file, saved.restore));
    // the grandchild still gets its own handle back, the child a new one, and the references follow
    EXPECT_EQ(w.find(grandchild_id), grandchild);
    const auto new_child = w.find(child_id);
    ASSERT_NE(new_child, entt::entity{ entt::null });
    EXPECT_NE(new_child, child);
    EXPECT_NE(new_child, other);
    EXPECT_EQ(w.registry.get<engine_parent_component_t>(grandchild).parent, static_cast<engine_game_object_t>(new_child));
    EXPECT_EQ(w.registry.get<engine_parent_component_t>(new_child).parent, static_cast<engine_game_object_t>(parent));
}

TEST_F(SceneFileTest, RestoredInstanceKeepsTheHandlesOfItsNodesAndItsAddedComponents)
{
    World w;
    const auto parent = w.create();
    const auto root = instantiate_arm(w);
    w.put<engine_parent_component_t>(root, engine_parent_component_t{ static_cast<engine_game_object_t>(parent) });
    const auto upper = w.prefabs.node(root, "Upper");
    const auto hand = w.prefabs.node(root, "Upper/Hand");
    engine_light_component_t light{};
    light.type = ENGINE_LIGHT_TYPE_POINT;
    w.put<engine_light_component_t>(upper, light);
    const auto scene = save(w);
    const auto root_id = w.ids.get_id(root);

    const auto saved = snapshot(w, root);
    destroy_with_children(w, root);

    ASSERT_TRUE(load(w, saved.file, saved.restore));
    EXPECT_EQ(w.find(root_id), root);
    EXPECT_EQ(w.prefabs.node(root, "Upper"), upper);
    EXPECT_EQ(w.prefabs.node(root, "Upper/Hand"), hand);
    EXPECT_TRUE(w.registry.all_of<engine_light_component_t>(upper));
    EXPECT_EQ(save(w), scene);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

// the editor's duplicate: the saved subtree loaded again as a copy
engine::SceneLoadOptions copy_options(engine::EntityHandles& loaded)
{
    engine::SceneLoadOptions options;
    options.new_ids = true;
    options.find_references_outside_file = true;
    options.loaded = &loaded;
    return options;
}

TEST_F(SceneFileTest, CopyGetsNewIdsAndItsReferencesFindTheCopies)
{
    World w;
    const auto parent = w.create();
    const auto child = w.create();
    w.put<engine_parent_component_t>(child, engine_parent_component_t{ static_cast<engine_game_object_t>(parent) });
    engine_name_component_t name{};
    std::strcpy(name.name, "child");
    w.put<engine_name_component_t>(child, name);
    const auto grandchild = w.create();
    w.put<engine_parent_component_t>(grandchild, engine_parent_component_t{ static_cast<engine_game_object_t>(child) });
    const auto child_id = w.ids.get_id(child);
    const auto grandchild_id = w.ids.get_id(grandchild);

    engine::EntityHandles loaded;
    ASSERT_TRUE(load(w, snapshot(w, child).file, copy_options(loaded)));
    const auto copy = loaded.at(child_id);
    const auto grandchild_copy = loaded.at(grandchild_id);
    EXPECT_NE(copy, child);
    EXPECT_NE(grandchild_copy, grandchild);
    EXPECT_NE(w.ids.get_id(copy), child_id);
    EXPECT_NE(w.ids.get_id(grandchild_copy), grandchild_id);
    // the original is untouched
    EXPECT_EQ(w.find(child_id), child);
    EXPECT_EQ(w.find(grandchild_id), grandchild);
    // the reference inside the copy finds the copy, the one outside of it the same game object as the original's
    EXPECT_EQ(w.registry.get<engine_parent_component_t>(grandchild_copy).parent, static_cast<engine_game_object_t>(copy));
    EXPECT_EQ(w.registry.get<engine_parent_component_t>(copy).parent, static_cast<engine_game_object_t>(parent));
    EXPECT_STREQ(w.registry.get<engine_name_component_t>(copy).name, "child");
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(SceneFileTest, CopyOfAnInstanceIsAnInstanceWithItsAddedComponents)
{
    World w;
    const auto root = instantiate_arm(w);
    engine_light_component_t light{};
    light.type = ENGINE_LIGHT_TYPE_POINT;
    w.put<engine_light_component_t>(w.prefabs.node(root, "Upper"), light);
    const auto root_id = w.ids.get_id(root);

    engine::EntityHandles loaded;
    ASSERT_TRUE(load(w, snapshot(w, root).file, copy_options(loaded)));
    const auto copy = loaded.at(root_id);
    ASSERT_NE(w.prefabs.find_instance(copy), nullptr);
    const auto upper = w.prefabs.node(copy, "Upper");
    // the copy's nodes have the hashed IDs of its own root ID, and the file's references to the nodes find them
    EXPECT_EQ(w.ids.get_id(upper), EntityIdRegistry::hashed_id(w.ids.get_id(copy), "Upper"));
    EXPECT_EQ(loaded.at(EntityIdRegistry::hashed_id(root_id, "Upper")), upper);
    EXPECT_TRUE(w.registry.all_of<engine_light_component_t>(upper));
    EXPECT_NE(upper, w.prefabs.node(root, "Upper"));
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

// -- prefab instances ------------------------------------------------------------------------------------------------

TEST_F(SceneFileTest, InstanceIsSavedAsItsPrefabAndWhatWasAddedToIt)
{
    World w;
    const auto root = instantiate_arm(w);
    w.registry.replace<engine_transform_component_t>(root, engine_transform_component_t{ { 3.0f, 0.0f, -1.5f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } });
    engine_collider_component_t collider{};
    collider.type = ENGINE_COLLIDER_TYPE_SPHERE;
    collider.collider.sphere.radius = 0.2f;
    w.put<engine_collider_component_t>(w.prefabs.node(root, "Upper/Hand"), collider);

    const auto file = save(w);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
    rapidjson::Document doc;
    doc.Parse(file.c_str());
    ASSERT_TRUE(doc.IsObject());
    // one game object: the root; the prefab creates the others
    ASSERT_EQ(doc["entities"].Size(), 1u);
    const auto& saved = doc["entities"][0];
    EXPECT_STREQ(saved["prefab"].GetString(), FakePrefabs::ARM);
    // the root's name and transform, which are always saved whole, under the root's node path ""; its mesh and the
    // rest come from the prefab
    EXPECT_FALSE(saved.HasMember("components"));
    const auto& nodes = saved["nodes"];
    ASSERT_TRUE(nodes.HasMember(""));
    EXPECT_EQ(nodes[""].MemberCount(), 1u);
    EXPECT_TRUE(nodes[""]["components"].HasMember("name"));
    EXPECT_TRUE(nodes[""]["components"].HasMember("transform"));
    EXPECT_EQ(nodes[""]["components"].MemberCount(), 2u);
    // the added collider whole
    ASSERT_TRUE(nodes.HasMember("Upper/Hand"));
    EXPECT_EQ(nodes["Upper/Hand"].MemberCount(), 1u);
    ASSERT_TRUE(nodes["Upper/Hand"].HasMember("components"));
    EXPECT_TRUE(nodes["Upper/Hand"]["components"]["collider"].HasMember("is_trigger"));
    EXPECT_EQ(nodes["Upper/Hand"]["components"].MemberCount(), 1u);
    EXPECT_FALSE(nodes.HasMember("Upper"));
}

TEST_F(SceneFileTest, InstanceRoundTripsWithItsAddedComponents)
{
    World original;
    const auto root = instantiate_arm(original);
    original.registry.replace<engine_transform_component_t>(root, engine_transform_component_t{ { 3.0f, 0.0f, -1.5f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } });
    engine_light_component_t light{};
    light.type = ENGINE_LIGHT_TYPE_POINT;
    light.point.constant = 2.0f;
    original.put<engine_light_component_t>(original.prefabs.node(root, "Upper"), light);
    const auto file = save(original);

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();

    const auto loaded_root = loaded.find(original.ids.get_id(root));
    ASSERT_NE(loaded_root, entt::entity{ entt::null });
    EXPECT_FLOAT_EQ(loaded.registry.get<engine_transform_component_t>(loaded_root).position[0], 3.0f);
    const auto upper = loaded.prefabs.node(loaded_root, "Upper");
    EXPECT_TRUE(loaded.registry.all_of<engine_light_component_t>(upper));
    // the game objects of the instance have the hashed IDs of their node paths
    EXPECT_EQ(loaded.ids.get_id(upper), EntityIdRegistry::hashed_id(original.ids.get_id(root), "Upper"));
}

TEST_F(SceneFileTest, ReferenceIntoAnInstanceResolvesWhateverWasCreatedBefore)
{
    World original;
    const auto root = instantiate_arm(original);
    const auto holder = original.create();
    engine_name_component_t holder_name{};
    std::strcpy(holder_name.name, "holder");
    original.put<engine_name_component_t>(holder, holder_name);
    original.put<engine_parent_component_t>(holder, engine_parent_component_t{ static_cast<engine_game_object_t>(original.prefabs.node(root, "Upper/Hand")) });
    const auto file = save(original);

    // the scene the file is loaded into has game objects already, which have the IDs of the file's game objects
    World loaded;
    for (int i = 0; i < 5; i++)
    {
        loaded.create();
    }
    ASSERT_TRUE(load(loaded, file));
    auto loaded_holder = entt::entity{ entt::null };
    for (const auto [entity, name] : loaded.registry.view<const engine_name_component_t>().each())
    {
        if (std::string_view(name.name) == "holder")
        {
            loaded_holder = entity;
        }
    }
    ASSERT_NE(loaded_holder, entt::entity{ entt::null });
    // the hand of the loaded instance
    const auto hand = static_cast<entt::entity>(loaded.registry.get<engine_parent_component_t>(loaded_holder).parent);
    ASSERT_TRUE(loaded.registry.valid(hand));
    EXPECT_STREQ(loaded.registry.get<engine_name_component_t>(hand).name, "Hand");
    const auto loaded_root = static_cast<entt::entity>(loaded.registry.get<engine_parent_component_t>(hand).parent);
    EXPECT_EQ(loaded.prefabs.node(loaded_root, "Upper/Hand"), hand);
}

TEST_F(SceneFileTest, ReferenceToANodeTheModelDoesntHaveIsNone)
{
    World w;
    const auto gone = EntityIdRegistry::hashed_id(1, "Upper/Gone");
    ASSERT_TRUE(load(w, fmt::format(R"({{ "version": 1, "entities": [
        {{ "id": 1, "prefab": "models/arm.gltf", "nodes": {{ "Upper/Gone": {{ "components": {{ "name": {{ "name": "x" }} }} }} }} }},
        {{ "id": 2, "components": {{ "parent": {{ "parent": {} }} }} }} ] }})", gone)));
    EXPECT_EQ(w.get<engine_parent_component_t>(2).parent, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_TRUE(test_logger::logged("its prefab models/arm.gltf has no node Upper/Gone, what the file has for it is ignored"));
    EXPECT_TRUE(test_logger::logged(fmt::format("references the game object {} which isn't in the file", gone)));
    // the rest of the instance is there
    EXPECT_NE(w.prefabs.node(w.find(1), "Upper/Hand"), entt::entity{ entt::null });
}

TEST_F(SceneFileTest, InstanceWhosePrefabCantBeFoundLoadsItsRoot)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [
        { "id": 1, "prefab": "models/missing.gltf", "nodes": {
            "": { "components": { "transform": { "position": [1, 2, 3] } }, "overrides": { "mesh": { "disable": true } } },
            "Upper": { "components": { "name": { "name": "x" } } } } } ] })"));
    EXPECT_TRUE(test_logger::logged("its prefab models/missing.gltf can't be instantiated"));
    // what is saved whole of the root is loaded, an override of what the prefab gives it can't be
    EXPECT_FLOAT_EQ(w.get<engine_transform_component_t>(1).position[2], 3.0f);
    EXPECT_FALSE(w.registry.all_of<engine_mesh_component_t>(w.find(1)));
    EXPECT_TRUE(test_logger::logged("the root has an override of mesh, which its prefab models/missing.gltf doesn't give it (any more), the override is ignored"));
    EXPECT_EQ(count_entities(w.registry), 2u);  // the reserved entity and the root
}

TEST_F(SceneFileTest, ComponentRemovedFromWhatThePrefabCreatedIsSavedAndRemovedOnLoad)
{
    World original;
    const auto root = instantiate_arm(original);
    original.registry.remove<engine_mesh_component_t>(original.prefabs.node(root, "Upper/Hand"));

    const auto file = save(original);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
    rapidjson::Document doc;
    doc.Parse(file.c_str());
    const auto& hand = doc["entities"][0]["nodes"]["Upper/Hand"];
    EXPECT_EQ(hand.MemberCount(), 1u);
    ASSERT_EQ(hand["removed"].Size(), 1u);
    EXPECT_STREQ(hand["removed"][0].GetString(), "mesh");

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    const auto loaded_hand = loaded.prefabs.node(loaded.find(original.ids.get_id(root)), "Upper/Hand");
    EXPECT_FALSE(loaded.registry.all_of<engine_mesh_component_t>(loaded_hand));
    EXPECT_TRUE(loaded.registry.all_of<engine_material_component_t>(loaded_hand));
    EXPECT_EQ(save(loaded), file);

    // nothing to remove when the model doesn't give it any more
    World changed;
    changed.prefabs.hand_has_mesh = false;
    ASSERT_TRUE(load(changed, file));
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(SceneFileTest, GameObjectRemovedFromWhatThePrefabCreatedIsLoggedAndNotSaved)
{
    World w;
    const auto root = instantiate_arm(w);
    const auto hand = w.prefabs.node(root, "Upper/Hand");
    w.registry.destroy(hand);

    save(w);
    EXPECT_TRUE(test_logger::logged(fmt::format("game object {} is an instance of models/arm.gltf; a game object removed from what its prefab "
        "created isn't saved: node Upper/Hand was destroyed", w.ids.get_id(root))));
}

TEST_F(SceneFileTest, ComponentsOfAnInstanceOutsideItsNodesAreIgnored)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [
        { "id": 1, "prefab": "models/arm.gltf", "components": { "light": { "type": "point" } }, "nodes": {
            "Upper": { "overrides": [], "parts": {}, "removed": ["no_such_component"] } } } ] })"));
    EXPECT_FALSE(w.registry.all_of<engine_light_component_t>(w.find(1)));
    EXPECT_TRUE(test_logger::logged("a prefab instance has its components under its nodes"));
    EXPECT_TRUE(test_logger::logged("the overrides of node Upper are not an object, they are ignored"));
    EXPECT_TRUE(test_logger::logged("node Upper has an unknown member \"parts\""));
    EXPECT_TRUE(test_logger::logged("node Upper removes something which isn't a known component"));
    // the instance is there
    EXPECT_TRUE(w.registry.all_of<engine_collider_component_t>(w.prefabs.node(w.find(1), "Upper")));
}

// -- prefab overrides ------------------------------------------------------------------------------------------------

// a moved child, a hidden child mesh and a recoloured material
void override_arm(World& w, entt::entity root)
{
    const auto upper = w.prefabs.node(root, "Upper");
    auto transform = w.registry.get<engine_transform_component_t>(upper);
    transform.position[1] = 0.75f;
    w.registry.replace<engine_transform_component_t>(upper, transform);
    const auto hand = w.prefabs.node(root, "Upper/Hand");
    auto mesh = w.registry.get<engine_mesh_component_t>(hand);
    mesh.disable = true;
    w.registry.replace<engine_mesh_component_t>(hand, mesh);
    auto material = w.registry.get<engine_material_component_t>(hand);
    material.diffuse_color[0] = 0.5f;
    w.registry.replace<engine_material_component_t>(hand, material);
}

TEST_F(SceneFileTest, ChangesToWhatThePrefabCreatedAreSavedAsTheChangedFields)
{
    World w;
    const auto root = instantiate_arm(w);
    override_arm(w, root);

    const auto file = save(w);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
    rapidjson::Document doc;
    doc.Parse<rapidjson::kParseNumbersAsStringsFlag>(file.c_str());
    ASSERT_TRUE(doc.IsObject());
    const auto& nodes = doc["entities"][0]["nodes"];
    // only the fields which differ from the prefab's, an array (the position, the colour) as one field
    ASSERT_TRUE(nodes.HasMember("Upper"));
    EXPECT_EQ(nodes["Upper"].MemberCount(), 1u);
    const auto& upper = nodes["Upper"]["overrides"];
    EXPECT_EQ(upper.MemberCount(), 1u);
    ASSERT_EQ(upper["transform"].MemberCount(), 1u);
    EXPECT_STREQ(upper["transform"]["position"][1].GetString(), "0.75");
    EXPECT_EQ(nodes["Upper/Hand"].MemberCount(), 1u);
    const auto& hand = nodes["Upper/Hand"]["overrides"];
    EXPECT_EQ(hand.MemberCount(), 2u);
    ASSERT_EQ(hand["mesh"].MemberCount(), 1u);
    EXPECT_TRUE(hand["mesh"]["disable"].GetBool());
    ASSERT_EQ(hand["material"].MemberCount(), 1u);
    EXPECT_EQ(hand["material"]["diffuse_color"].Size(), 4u);
}

TEST_F(SceneFileTest, OverridesRoundTrip)
{
    World original;
    const auto root = instantiate_arm(original);
    override_arm(original, root);
    const auto file = save(original);

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
    const auto loaded_root = loaded.find(original.ids.get_id(root));
    const auto upper = loaded.prefabs.node(loaded_root, "Upper");
    const auto hand = loaded.prefabs.node(loaded_root, "Upper/Hand");
    EXPECT_TRUE(same_bytes(loaded.registry.get<engine_transform_component_t>(upper), original.registry.get<engine_transform_component_t>(original.prefabs.node(root, "Upper"))));
    EXPECT_TRUE(loaded.registry.get<engine_mesh_component_t>(hand).disable);
    EXPECT_EQ(loaded.registry.get<engine_mesh_component_t>(hand).geometry, 7u);
    const auto& material = loaded.registry.get<engine_material_component_t>(hand);
    EXPECT_FLOAT_EQ(material.diffuse_color[0], 0.5f);
    EXPECT_FLOAT_EQ(material.diffuse_color[1], 1.0f);
    EXPECT_EQ(material.diffuse_texture, 3u);
}

TEST_F(SceneFileTest, FieldsWhichArentOverriddenFollowThePrefab)
{
    World original;
    const auto root = instantiate_arm(original);
    override_arm(original, root);
    const auto file = save(original);

    // the model changed since the file was saved
    World loaded;
    loaded.prefabs.hand_material.shininess = 64.0f;
    loaded.prefabs.hand_material.diffuse_color[0] = 0.0f;
    ASSERT_TRUE(load(loaded, file));
    const auto hand = loaded.prefabs.node(loaded.find(original.ids.get_id(root)), "Upper/Hand");
    const auto& material = loaded.registry.get<engine_material_component_t>(hand);
    // the overridden colour stays, the shininess is the model's new one
    EXPECT_FLOAT_EQ(material.diffuse_color[0], 0.5f);
    EXPECT_FLOAT_EQ(material.shininess, 64.0f);
}

TEST_F(SceneFileTest, FieldSetBackToThePrefabsValueIsNotSaved)
{
    World w;
    const auto root = instantiate_arm(w);
    const auto hand = w.prefabs.node(root, "Upper/Hand");
    auto material = w.registry.get<engine_material_component_t>(hand);
    material.shininess = 2.0f;
    w.registry.replace<engine_material_component_t>(hand, material);
    material.shininess = 16.0f;
    w.registry.replace<engine_material_component_t>(hand, material);

    rapidjson::Document doc;
    doc.Parse(save(w).c_str());
    EXPECT_FALSE(doc["entities"][0]["nodes"].HasMember("Upper/Hand"));
}

// a box collider made taller on the arm's Upper
void override_upper_collider(World& w, entt::entity root)
{
    const auto upper = w.prefabs.node(root, "Upper");
    auto collider = w.registry.get<engine_collider_component_t>(upper);
    collider.collider.box.size[1] = 2.0f;
    w.registry.replace<engine_collider_component_t>(upper, collider);
}

TEST_F(SceneFileTest, OverrideInsideAUnionCaseSavesTheWholeUnion)
{
    World original;
    const auto root = instantiate_arm(original);
    override_upper_collider(original, root);
    const auto file = save(original);

    rapidjson::Document doc;
    doc.Parse(file.c_str());
    const auto& saved = doc["entities"][0]["nodes"]["Upper"]["overrides"]["collider"];
    // the union is one value: its tag and its whole case, but nothing else of the collider
    EXPECT_STREQ(saved["type"].GetString(), "box");
    ASSERT_TRUE(saved.HasMember("box"));
    EXPECT_EQ(saved["box"]["size"].Size(), 3u);
    EXPECT_EQ(saved.MemberCount(), 2u);

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    const auto& box = loaded.registry.get<engine_collider_component_t>(loaded.prefabs.node(loaded.find(original.ids.get_id(root)), "Upper"));
    EXPECT_EQ(box.type, ENGINE_COLLIDER_TYPE_BOX);
    EXPECT_FLOAT_EQ(box.collider.box.size[0], 0.5f);
    EXPECT_FLOAT_EQ(box.collider.box.size[1], 2.0f);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(SceneFileTest, UnionOverrideStaysWhenThePrefabChangesItsCase)
{
    World original;
    const auto root = instantiate_arm(original);
    override_upper_collider(original, root);
    const auto file = save(original);

    // the model's Upper has a sphere collider now
    World loaded;
    loaded.prefabs.upper_collider = {};
    loaded.prefabs.upper_collider.type = ENGINE_COLLIDER_TYPE_SPHERE;
    loaded.prefabs.upper_collider.collider.sphere.radius = 0.4f;
    loaded.prefabs.upper_collider.bounciness = 0.75f;
    ASSERT_TRUE(load(loaded, file));
    // the override wins: the box it was, the rest of the collider is the model's
    const auto& collider = loaded.registry.get<engine_collider_component_t>(loaded.prefabs.node(loaded.find(original.ids.get_id(root)), "Upper"));
    EXPECT_EQ(collider.type, ENGINE_COLLIDER_TYPE_BOX);
    EXPECT_FLOAT_EQ(collider.collider.box.size[0], 0.5f);
    EXPECT_FLOAT_EQ(collider.collider.box.size[1], 2.0f);
    EXPECT_FLOAT_EQ(collider.collider.box.size[2], 0.5f);
    EXPECT_FLOAT_EQ(collider.bounciness, 0.75f);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(SceneFileTest, OverrideOfAComponentThePrefabDoesntGiveAnyMoreIsIgnored)
{
    World original;
    const auto root = instantiate_arm(original);
    override_arm(original, root);
    const auto file = save(original);

    // the model's hand has no mesh any more
    World loaded;
    loaded.prefabs.hand_has_mesh = false;
    ASSERT_TRUE(load(loaded, file));
    const auto hand = loaded.prefabs.node(loaded.find(original.ids.get_id(root)), "Upper/Hand");
    // no mesh made of defaults with the overridden field
    EXPECT_FALSE(loaded.registry.all_of<engine_mesh_component_t>(hand));
    EXPECT_TRUE(test_logger::logged("node Upper/Hand has an override of mesh, which its prefab models/arm.gltf doesn't give it (any more), the "
        "override is ignored"));
    // the other overrides are loaded
    EXPECT_FLOAT_EQ(loaded.registry.get<engine_material_component_t>(hand).diffuse_color[0], 0.5f);
}

TEST_F(SceneFileTest, AddedComponentWhichThePrefabGivesSinceIsReadWhole)
{
    World original;
    original.prefabs.hand_has_mesh = false;
    const auto root = instantiate_arm(original);
    original.put<engine_mesh_component_t>(original.prefabs.node(root, "Upper/Hand"), engine_mesh_component_t{ 7, true });
    const auto file = save(original);

    // the model's hand has a mesh now
    World loaded;
    ASSERT_TRUE(load(loaded, file));
    const auto loaded_root = loaded.find(original.ids.get_id(root));
    EXPECT_TRUE(loaded.registry.get<engine_mesh_component_t>(loaded.prefabs.node(loaded_root, "Upper/Hand")).disable);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
    // and it is an override of the model's from now on
    rapidjson::Document doc;
    const auto resaved = save(loaded);
    doc.Parse(resaved.c_str());
    const auto& hand = doc["entities"][0]["nodes"]["Upper/Hand"];
    EXPECT_FALSE(hand.HasMember("components"));
    EXPECT_EQ(hand["overrides"]["mesh"].MemberCount(), 1u);
}

TEST_F(SceneFileTest, OverriddenUnionTagSavesTheWholeCase)
{
    World original;
    const auto root = instantiate_arm(original);
    const auto upper = original.prefabs.node(root, "Upper");
    auto collider = original.registry.get<engine_collider_component_t>(upper);
    collider.type = ENGINE_COLLIDER_TYPE_SPHERE;
    collider.collider = {};
    collider.collider.sphere.radius = 0.3f;
    original.registry.replace<engine_collider_component_t>(upper, collider);
    const auto file = save(original);

    rapidjson::Document doc;
    doc.Parse(file.c_str());
    const auto& saved = doc["entities"][0]["nodes"]["Upper"]["overrides"]["collider"];
    EXPECT_STREQ(saved["type"].GetString(), "sphere");
    EXPECT_TRUE(saved["sphere"].HasMember("radius"));
    EXPECT_FALSE(saved.HasMember("box"));

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    const auto& sphere = loaded.registry.get<engine_collider_component_t>(loaded.prefabs.node(loaded.find(original.ids.get_id(root)), "Upper"));
    EXPECT_EQ(sphere.type, ENGINE_COLLIDER_TYPE_SPHERE);
    // the case starts zeroed, not with the bytes of the prefab's box
    EXPECT_FLOAT_EQ(sphere.collider.sphere.radius, 0.3f);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(SceneFileTest, OverridesSurviveUndoOfADeleteAndACopy)
{
    World w;
    const auto root = instantiate_arm(w);
    override_arm(w, root);
    w.registry.remove<engine_collider_component_t>(w.prefabs.node(root, "Upper"));
    const auto scene = save(w);

    const auto saved = snapshot(w, root);
    destroy_with_children(w, root);
    ASSERT_TRUE(load(w, saved.file, saved.restore));
    EXPECT_EQ(save(w), scene);

    engine::EntityHandles loaded;
    ASSERT_TRUE(load(w, snapshot(w, root).file, copy_options(loaded)));
    const auto copy = loaded.at(w.ids.get_id(root));
    EXPECT_TRUE(w.registry.get<engine_mesh_component_t>(w.prefabs.node(copy, "Upper/Hand")).disable);
    EXPECT_FLOAT_EQ(w.registry.get<engine_material_component_t>(w.prefabs.node(copy, "Upper/Hand")).diffuse_color[0], 0.5f);
    EXPECT_FLOAT_EQ(w.registry.get<engine_transform_component_t>(w.prefabs.node(copy, "Upper")).position[1], 0.75f);
    EXPECT_FALSE(w.registry.all_of<engine_collider_component_t>(w.prefabs.node(copy, "Upper")));
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(SceneFileTest, RenamedInstanceRootKeepsItsName)
{
    World original;
    const auto root = instantiate_arm(original);
    engine_name_component_t name{};
    std::strcpy(name.name, "left arm");
    original.registry.replace<engine_name_component_t>(root, name);
    const auto file = save(original);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    EXPECT_STREQ(loaded.get<engine_name_component_t>(original.ids.get_id(root)).name, "left arm");
}

TEST_F(SceneFileTest, JointNameChangedOnAnInstanceIsSavedAsAnOverride)
{
    World original;
    original.prefabs.hand_joint = "wrist";
    const auto root = instantiate_arm(original);
    // engineStringSet changes the engine's string in place, without an update: what the prefab gave is a copy
    const auto hand = original.prefabs.node(root, "Upper/Hand");
    original.registry.get<engine_joint_attachment_component_t>(hand).joint_name->str = "finger";

    const auto file = save(original);
    rapidjson::Document doc;
    doc.Parse(file.c_str());
    ASSERT_TRUE(doc.IsObject());
    const auto& overrides = doc["entities"][0]["nodes"]["Upper/Hand"]["overrides"];
    ASSERT_EQ(overrides.MemberCount(), 1u);
    ASSERT_EQ(overrides["joint_attachment"].MemberCount(), 1u);
    EXPECT_STREQ(overrides["joint_attachment"]["joint_name"].GetString(), "finger");

    World loaded;
    loaded.prefabs.hand_joint = "wrist";
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    const auto loaded_hand = loaded.prefabs.node(loaded.find(original.ids.get_id(root)), "Upper/Hand");
    EXPECT_EQ(loaded.registry.get<engine_joint_attachment_component_t>(loaded_hand).joint_name->str, "finger");
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(SceneFileTest, UnchangedJointNameIsNotSaved)
{
    World w;
    w.prefabs.hand_joint = "wrist";
    instantiate_arm(w);
    rapidjson::Document doc;
    doc.Parse(save(w).c_str());
    EXPECT_FALSE(doc["entities"][0]["nodes"].HasMember("Upper/Hand"));
}

TEST_F(SceneFileTest, UnchangedInstanceSavesWithoutWarnings)
{
    World w;
    const auto root = instantiate_arm(w);
    // a value written again unchanged, from a struct with other padding bytes, is no change
    auto mesh = w.registry.get<engine_mesh_component_t>(w.prefabs.node(root, "Upper/Hand"));
    w.registry.replace<engine_mesh_component_t>(w.prefabs.node(root, "Upper/Hand"), mesh);
    save(w);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

// -- assets ----------------------------------------------------------------------------------------------------------

TEST_F(SceneFileTest, AssetsAreResolvedByTheirId)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": {
        "mesh": { "geometry": "models/crate.gltf#Cube" },
        "material": { "diffuse_texture": "models/crate.gltf#wood", "specular_texture": null } } } ] })"));
    EXPECT_EQ(w.get<engine_mesh_component_t>(1).geometry, 7u);
    EXPECT_EQ(w.get<engine_material_component_t>(1).diffuse_texture, 3u);
    EXPECT_EQ(w.get<engine_material_component_t>(1).specular_texture, ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(SceneFileTest, AssetWhichCantBeFoundIsNone)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": { "mesh": { "geometry": "models/missing.gltf#Cube" } } } ] })"));
    EXPECT_EQ(w.get<engine_mesh_component_t>(1).geometry, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_TRUE(test_logger::logged("uses the asset \"models/missing.gltf#Cube\" which can't be found"));
}

TEST_F(SceneFileTest, ReleasedAssetIsSavedAsNull)
{
    World w;
    w.put<engine_mesh_component_t>(w.create(), engine_mesh_component_t{ 99, false });
    EXPECT_NE(save(w).find(R"("geometry": null)"), std::string::npos);
    EXPECT_TRUE(test_logger::logged("uses an asset which doesn't exist any more"));
}

// -- files which can't be used -------------------------------------------------------------------------------------

class UnusableSceneFileTest : public SceneFileTest, public ::testing::WithParamInterface<std::pair<const char*, const char*>>
{
};

TEST_P(UnusableSceneFileTest, LoadFailsAndChangesNothing)
{
    World w;
    const auto entities_before = count_entities(w.registry);
    EXPECT_FALSE(load(w, GetParam().first));
    EXPECT_EQ(count_entities(w.registry), entities_before);
    EXPECT_TRUE(test_logger::logged(GetParam().second));
}

INSTANTIATE_TEST_SUITE_P(Files, UnusableSceneFileTest, ::testing::Values(
    std::make_pair(R"({ "version": 1, "entities": [ { "id": 1, )", "not valid JSON"),
    std::make_pair(R"({ "entities": [] })", "it has no version"),
    std::make_pair(R"({ "version": 2, "entities": [] })", "version 2 is not supported"),
    std::make_pair(R"({ "version": 1 })", "it has no entity list")));

TEST_F(SceneFileTest, GameObjectsWithoutAValidIdAreSkipped)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [
        { "components": {} },
        { "id": 0 },
        { "id": 5, "components": { "name": { "name": "first" } } },
        { "id": 5, "components": { "name": { "name": "second" } } } ] })"));
    EXPECT_STREQ(w.get<engine_name_component_t>(5).name, "first");
    // entity 0 and the one loaded
    EXPECT_EQ(count_entities(w.registry), 2u);
    EXPECT_TRUE(test_logger::logged("a game object has no valid persistent ID"));
    EXPECT_TRUE(test_logger::logged("the persistent ID 5 is used twice"));
}

TEST_F(SceneFileTest, ComponentSavedByANewerEngineIsSkipped)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "types": { "transform": 2 }, "entities": [ { "id": 1, "components": {
        "transform": { "position": [1, 2, 3] }, "name": { "name": "kept" } } } ] })"));
    EXPECT_FALSE(w.registry.all_of<engine_transform_component_t>(w.find(1)));
    EXPECT_STREQ(w.get<engine_name_component_t>(1).name, "kept");
    EXPECT_TRUE(test_logger::logged("component transform was saved in version 2"));
}
// -- game-defined components ----------------------------------------------------------------------------------------

enum Stance { STANCE_IDLE = 0, STANCE_GUARD = 2 };

struct Ring
{
    float radius;
};

// a component the game registers through the C API
struct PatrolConfig
{
    float speed;
    char label[12];
    Stance stance;
    engine_game_object_t target;
    engine_geometry_t marker;
    engine_texture2d_t icon;
    engine_prefab_t post;
    float points[4];
    Ring ring;
    bool loop;
};

class GameComponentFileTest : public SceneFileTest
{
protected:
    void SetUp() override
    {
        SceneFileTest::SetUp();
        register_patrol_config(types_);
    }

    static engine_field_desc_t field(const char* name, engine_field_kind_t kind, std::size_t offset, std::uint32_t count = 0)
    {
        engine_field_desc_t f{};
        f.name = name;
        f.kind = kind;
        f.offset = static_cast<std::uint32_t>(offset);
        f.count = count;
        return f;
    }

    static void register_patrol_config(engine::TypeRegistry& types)
    {
        const engine_field_desc_t ring_fields[] = { field("radius", ENGINE_FIELD_KIND_FLOAT, offsetof(Ring, radius)) };
        engine_type_desc_t ring{ "ring", 1, sizeof(Ring), alignof(Ring), nullptr, ring_fields, 1 };
        ASSERT_NE(engine::register_game_type(types, ring, false), nullptr);

        static const engine_enum_value_desc_t stances[] = { { "idle", STANCE_IDLE }, { "guard", STANCE_GUARD } };
        auto stance = field("stance", ENGINE_FIELD_KIND_ENUM, offsetof(PatrolConfig, stance));
        stance.enum_values = stances;
        stance.enum_values_count = 2;
        auto ring_field = field("ring", ENGINE_FIELD_KIND_STRUCT, offsetof(PatrolConfig, ring));
        ring_field.type_name = "ring";
        const engine_field_desc_t fields[] = {
            field("speed", ENGINE_FIELD_KIND_FLOAT, offsetof(PatrolConfig, speed)),
            field("label", ENGINE_FIELD_KIND_STRING, offsetof(PatrolConfig, label), sizeof(PatrolConfig::label)),
            stance,
            field("target", ENGINE_FIELD_KIND_GAME_OBJECT, offsetof(PatrolConfig, target)),
            field("marker", ENGINE_FIELD_KIND_GEOMETRY, offsetof(PatrolConfig, marker)),
            field("icon", ENGINE_FIELD_KIND_TEXTURE, offsetof(PatrolConfig, icon)),
            field("post", ENGINE_FIELD_KIND_PREFAB, offsetof(PatrolConfig, post)),
            field("points", ENGINE_FIELD_KIND_FLOAT, offsetof(PatrolConfig, points), 4),
            ring_field,
            field("loop", ENGINE_FIELD_KIND_BOOL, offsetof(PatrolConfig, loop)),
        };
        PatrolConfig defaults{};
        defaults.speed = 3.0f;
        std::strcpy(defaults.label, "patrol");
        defaults.stance = STANCE_GUARD;
        defaults.marker = ENGINE_INVALID_OBJECT_HANDLE;
        defaults.icon = ENGINE_INVALID_OBJECT_HANDLE;
        defaults.post = ENGINE_INVALID_OBJECT_HANDLE;
        defaults.ring.radius = 1.5f;
        defaults.loop = true;
        engine_type_desc_t desc{ "patrol_config", 1, sizeof(PatrolConfig), alignof(PatrolConfig), &defaults, fields, 10 };
        ASSERT_NE(engine::register_game_type(types, desc, true), nullptr);
    }

    const engine::TypeInfo& patrol_config() const { return *types_.find("patrol_config"); }

    PatrolConfig get_patrol(const World& w, std::uint64_t id) const
    {
        PatrolConfig value{};
        patrol_config().storage->get(w.registry, w.find(id), &value);
        return value;
    }

    // the full scene, and a patrol config on the crate (1) targeting its child (2)
    void build_scene_with_patrol(World& w)
    {
        build_full_scene(w);
        PatrolConfig patrol{};
        patrol.speed = 0.1f;
        std::strcpy(patrol.label, "north gate");
        patrol.stance = STANCE_IDLE;
        patrol.target = static_cast<engine_game_object_t>(w.find(2));
        patrol.marker = 7;
        patrol.icon = 4;
        patrol.post = 6;
        patrol.points[0] = -1.0f;
        patrol.points[3] = 1e-7f;
        patrol.ring.radius = 4.0f;
        patrol.loop = false;
        patrol_config().storage->add(w.registry, w.find(1));
        patrol_config().storage->update(w.registry, w.find(1), &patrol);
    }
};

TEST_F(GameComponentFileTest, SaveLoadSaveGivesTheSameFile)
{
    World original;
    build_scene_with_patrol(original);
    const auto file = save(original);

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(GameComponentFileTest, GameComponentOnAGameObjectOfAnInstanceSurvivesSaveAndLoad)
{
    World original;
    const auto root = instantiate_arm(original);
    PatrolConfig patrol{};
    patrol.speed = 3.5f;
    patrol.target = static_cast<engine_game_object_t>(root);
    const auto hand = original.prefabs.node(root, "Upper/Hand");
    patrol_config().storage->add(original.registry, hand);
    patrol_config().storage->update(original.registry, hand, &patrol);
    const auto file = save(original);

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    const auto loaded_hand = loaded.find(original.ids.get_id(hand));
    ASSERT_NE(loaded_hand, entt::entity{ entt::null });
    PatrolConfig value{};
    patrol_config().storage->get(loaded.registry, loaded_hand, &value);
    EXPECT_FLOAT_EQ(value.speed, 3.5f);
    EXPECT_EQ(value.target, static_cast<engine_game_object_t>(loaded.find(original.ids.get_id(root))));
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(GameComponentFileTest, LoadedComponentHasTheSavedValues)
{
    World original;
    build_scene_with_patrol(original);
    World loaded;
    ASSERT_TRUE(load(loaded, save(original)));

    const auto patrol = get_patrol(loaded, 1);
    const auto expected = get_patrol(original, 1);
    EXPECT_EQ(patrol.speed, 0.1f);
    EXPECT_STREQ(patrol.label, "north gate");
    EXPECT_EQ(patrol.stance, STANCE_IDLE);
    // the reference points at the loaded game object with the saved ID
    EXPECT_EQ(patrol.target, static_cast<engine_game_object_t>(loaded.find(2)));
    EXPECT_EQ(patrol.marker, 7u);
    EXPECT_EQ(patrol.icon, 4u);
    EXPECT_EQ(patrol.post, 6u);
    EXPECT_TRUE(same_bytes(patrol.points, expected.points));
    EXPECT_EQ(patrol.ring.radius, 4.0f);
    EXPECT_FALSE(patrol.loop);
}

TEST_F(GameComponentFileTest, FileIsReadable)
{
    World w;
    build_scene_with_patrol(w);
    const auto file = save(w);
    EXPECT_NE(file.find(R"("patrol_config": 1)"), std::string::npos) << file;
    EXPECT_NE(file.find(R"("label": "north gate")"), std::string::npos);
    EXPECT_NE(file.find(R"("stance": "idle")"), std::string::npos);
    EXPECT_NE(file.find(R"("target": 2)"), std::string::npos);
    EXPECT_NE(file.find(R"("marker": "models/crate.gltf#Cube")"), std::string::npos);
    EXPECT_NE(file.find(R"("post": "rooms/guard_post.json")"), std::string::npos);
    EXPECT_NE(file.find(R"("points": [-1, 0, 0, 1e-07])"), std::string::npos);
    EXPECT_NE(file.find(R"("ring": {)"), std::string::npos);
    // after the engine's components: they are loaded first
    EXPECT_LT(file.find(R"("collider": {)"), file.find(R"("patrol_config": {)"));
}

TEST_F(GameComponentFileTest, MissingFieldsGetTheTypesDefaultValue)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": {
        "patrol_config": { "speed": 0.5, "ring": {} } } } ] })"));
    const auto patrol = get_patrol(w, 1);
    EXPECT_EQ(patrol.speed, 0.5f);
    EXPECT_STREQ(patrol.label, "patrol");
    EXPECT_EQ(patrol.stance, STANCE_GUARD);
    EXPECT_EQ(patrol.marker, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(patrol.post, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(patrol.ring.radius, 1.5f);
    EXPECT_TRUE(patrol.loop);
    EXPECT_TRUE(test_logger::messages().empty());
}

TEST_F(GameComponentFileTest, WrongValuesKeepTheDefault)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": {
        "patrol_config": { "stance": "asleep", "label": "a label longer than eleven", "target": 99, "unknown": 1 } } } ] })"));
    const auto patrol = get_patrol(w, 1);
    EXPECT_EQ(patrol.stance, STANCE_GUARD);
    EXPECT_STREQ(patrol.label, "a label lon");
    EXPECT_EQ(patrol.target, ENGINE_INVALID_GAME_OBJECT_ID);
    EXPECT_TRUE(test_logger::logged("patrol_config.stance has an unknown value \"asleep\""));
    EXPECT_TRUE(test_logger::logged("patrol_config.label is longer than 11 characters"));
    EXPECT_TRUE(test_logger::logged("patrol_config.target references the game object 99 which isn't in the file"));
    EXPECT_TRUE(test_logger::logged("unknown field \"unknown\""));
}

TEST_F(SceneFileTest, GameComponentWhichIsNotRegisteredIsIgnored)
{
    // the game didn't register it (e.g. a file from another game, or the type was renamed)
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "types": { "patrol_config": 1 }, "entities": [ { "id": 1, "components": {
        "patrol_config": { "speed": 0.5 }, "name": { "name": "kept" } } } ] })"));
    EXPECT_STREQ(w.get<engine_name_component_t>(1).name, "kept");
    EXPECT_TRUE(test_logger::logged("unknown component patrol_config"));
}

TEST_F(GameComponentFileTest, GameComponentSavedInANewerVersionIsSkipped)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "types": { "patrol_config": 2 }, "entities": [ { "id": 1, "components": {
        "patrol_config": { "speed": 0.5 } } } ] })"));
    EXPECT_FALSE(patrol_config().storage->has(w.registry, w.find(1)));
    EXPECT_TRUE(test_logger::logged("component patrol_config was saved in version 2"));
}

// -- data asset references -------------------------------------------------------------------------------------------

struct Wave
{
    float speed;
};

struct SpawnerConfig
{
    engine_data_asset_t wave;
};

class DataAssetReferenceTest : public GameComponentFileTest
{
protected:
    void SetUp() override
    {
        GameComponentFileTest::SetUp();
        const engine_field_desc_t wave_fields[] = { field("speed", ENGINE_FIELD_KIND_FLOAT, offsetof(Wave, speed)) };
        const engine_type_desc_t wave{ "wave", 1, sizeof(Wave), alignof(Wave), nullptr, wave_fields, 1 };
        wave_ = engine::register_game_type(types_, wave, false);
        ASSERT_NE(wave_, nullptr);
        auto wave_field = field("wave", ENGINE_FIELD_KIND_DATA_ASSET, offsetof(SpawnerConfig, wave));
        wave_field.type_name = "wave";
        const engine_type_desc_t spawner{ "spawner_config", 1, sizeof(SpawnerConfig), alignof(SpawnerConfig), nullptr, &wave_field, 1 };
        ASSERT_NE(engine::register_game_type(types_, spawner, true), nullptr);
        assets_.add_data_asset("waves/fast.json", 11, wave_);
        // a data asset of another type
        assets_.add_data_asset("waves/ring.json", 12, types_.find("ring"));
    }

    const engine::TypeInfo& spawner_config() const { return *types_.find("spawner_config"); }

    const engine::TypeInfo* wave_ = nullptr;
};

TEST_F(DataAssetReferenceTest, ReferenceIsSavedAsTheAssetIdAndLoadedBack)
{
    World original;
    const auto entity = original.create();
    const SpawnerConfig spawner{ 11 };
    spawner_config().storage->add(original.registry, entity);
    spawner_config().storage->update(original.registry, entity, &spawner);
    const auto file = save(original);
    EXPECT_NE(file.find(R"("wave": "waves/fast.json")"), std::string::npos) << file;

    World loaded;
    ASSERT_TRUE(load(loaded, file));
    SpawnerConfig value{};
    spawner_config().storage->get(loaded.registry, loaded.find(original.ids.get_id(entity)), &value);
    EXPECT_EQ(value.wave, 11u);
    EXPECT_EQ(save(loaded), file);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(DataAssetReferenceTest, DataAssetOfAnotherTypeIsNone)
{
    World w;
    ASSERT_TRUE(load(w, R"({ "version": 1, "entities": [ { "id": 1, "components": { "spawner_config": { "wave": "waves/ring.json" } } } ] })"));
    SpawnerConfig value{ 99 };
    spawner_config().storage->get(w.registry, w.find(1), &value);
    EXPECT_EQ(value.wave, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_TRUE(test_logger::logged("holds a ring, not a wave"));
}

TEST_F(DataAssetReferenceTest, FieldNeedsAStructTypeWithoutGameObjects)
{
    // patrol_config is a component, and it references a game object
    auto field_desc = field("wave", ENGINE_FIELD_KIND_DATA_ASSET, offsetof(SpawnerConfig, wave));
    field_desc.type_name = "patrol_config";
    const engine_type_desc_t component{ "bad_spawner", 1, sizeof(SpawnerConfig), alignof(SpawnerConfig), nullptr, &field_desc, 1 };
    EXPECT_EQ(engine::register_game_type(types_, component, true), nullptr);

    struct Targets
    {
        engine_game_object_t target;
    };
    const engine_field_desc_t target = field("target", ENGINE_FIELD_KIND_GAME_OBJECT, offsetof(Targets, target));
    const engine_type_desc_t targets{ "targets", 1, sizeof(Targets), alignof(Targets), nullptr, &target, 1 };
    ASSERT_NE(engine::register_game_type(types_, targets, false), nullptr);
    field_desc.type_name = "targets";
    EXPECT_EQ(engine::register_game_type(types_, component, true), nullptr);
    EXPECT_TRUE(test_logger::logged("without game object references"));
}

// -- scene prefabs ---------------------------------------------------------------------------------------------------

// A room: a table, a cup on it (a reference inside the file) and an arm (a model's instance) with a changed collider.
constexpr const char* ROOM = "rooms/room.json";
std::string room_file(float table_x = 1.0f)
{
    return fmt::format(R"({{ "version": 1, "entities": [
        {{ "id": 1, "components": {{ "name": {{ "name": "table" }}, "transform": {{ "position": [{}, 0, 0] }} }} }},
        {{ "id": 2, "components": {{ "name": {{ "name": "cup" }}, "parent": {{ "parent": 1 }}, "transform": {{ "position": [0, 0.9, 0] }} }} }},
        {{ "id": 3, "prefab": "models/arm.gltf", "nodes": {{ "": {{ "components": {{ "name": {{ "name": "arm" }} }} }},
            "Upper": {{ "overrides": {{ "collider": {{ "is_trigger": true }} }} }} }} }} ] }})", table_x);
}

class ScenePrefabTest : public SceneFileTest
{
protected:
    // an instance of the room, its root (with the next persistent ID) at `x`
    entt::entity place_room(World& w, float x)
    {
        w.prefabs.types = &types_;
        w.prefabs.assets = &assets_;
        w.prefabs.scene_files[ROOM] = room_file();
        const auto root = w.create();
        w.put<engine_transform_component_t>(root, engine_transform_component_t{ { x, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } });
        EXPECT_NE(w.prefabs.instantiate(ROOM, root), nullptr);
        return root;
    }

    static entt::entity parent(const World& w, entt::entity e)
    {
        return static_cast<entt::entity>(w.registry.get<engine_parent_component_t>(e).parent);
    }
};

TEST_F(ScenePrefabTest, EachInstanceHasItsOwnGameObjects)
{
    World w;
    const auto a = place_room(w, 0.0f);
    const auto b = place_room(w, 10.0f);
    for (const auto root : { a, b })
    {
        const auto table = w.prefabs.node(root, "1");
        const auto cup = w.prefabs.node(root, "2");
        const auto arm = w.prefabs.node(root, "3");
        ASSERT_NE(table, entt::entity{ entt::null });
        // the file's game objects without a parent are under the root, the references inside the file point into the
        // same instance
        EXPECT_EQ(parent(w, table), root);
        EXPECT_EQ(parent(w, arm), root);
        EXPECT_EQ(parent(w, cup), table);
        EXPECT_FLOAT_EQ(w.registry.get<engine_transform_component_t>(table).position[0], 1.0f);
        // the model's instance in the file is reached by the node path of its root there
        const auto upper = w.prefabs.node(root, "3/Upper");
        ASSERT_NE(upper, entt::entity{ entt::null });
        EXPECT_EQ(parent(w, upper), arm);
        EXPECT_TRUE(w.registry.get<engine_collider_component_t>(upper).is_trigger);
        // every game object has the hashed ID of its node path, however deep
        const auto root_id = w.ids.get_id(root);
        EXPECT_EQ(w.ids.get_id(table), EntityIdRegistry::hashed_id(root_id, "1"));
        EXPECT_EQ(w.ids.get_id(upper), EntityIdRegistry::hashed_id(root_id, "3/Upper"));
        EXPECT_EQ(w.ids.get_id(w.prefabs.node(root, "3/Upper/Hand")), EntityIdRegistry::hashed_id(root_id, "3/Upper/Hand"));
    }
    EXPECT_NE(w.prefabs.node(a, "2"), w.prefabs.node(b, "2"));
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(ScenePrefabTest, InstanceIsSavedAsItsRootAndItsOverrides)
{
    World w;
    const auto root = place_room(w, 3.0f);
    auto cup_transform = w.registry.get<engine_transform_component_t>(w.prefabs.node(root, "2"));
    cup_transform.position[1] = 0.8f;
    w.registry.replace<engine_transform_component_t>(w.prefabs.node(root, "2"), cup_transform);
    auto hand_material = w.registry.get<engine_material_component_t>(w.prefabs.node(root, "3/Upper/Hand"));
    hand_material.shininess = 2.0f;
    w.registry.replace<engine_material_component_t>(w.prefabs.node(root, "3/Upper/Hand"), hand_material);

    const auto file = save(w);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
    rapidjson::Document doc;
    doc.Parse(file.c_str());
    ASSERT_EQ(doc["entities"].Size(), 1u);
    const auto& saved = doc["entities"][0];
    EXPECT_STREQ(saved["prefab"].GetString(), ROOM);
    const auto& nodes = saved["nodes"];
    // the root's placement, and only what changed inside the instance, by node path
    EXPECT_EQ(nodes.MemberCount(), 3u) << file;
    EXPECT_TRUE(nodes[""]["components"].HasMember("transform"));
    EXPECT_EQ(nodes["2"]["overrides"]["transform"].MemberCount(), 1u);
    EXPECT_TRUE(nodes["2"]["overrides"]["transform"].HasMember("position"));
    EXPECT_EQ(nodes["3/Upper/Hand"]["overrides"]["material"].MemberCount(), 1u);
}

TEST_F(ScenePrefabTest, InstanceRoundTripsWhateverWasCreatedBefore)
{
    World original;
    place_room(original, 0.0f);
    const auto b = place_room(original, 10.0f);
    auto cup_transform = original.registry.get<engine_transform_component_t>(original.prefabs.node(b, "2"));
    cup_transform.position[1] = 0.8f;
    original.registry.replace<engine_transform_component_t>(original.prefabs.node(b, "2"), cup_transform);
    // a game object of the level references a game object inside the second room's arm
    const auto holder = original.create();
    original.put<engine_parent_component_t>(holder, engine_parent_component_t{ static_cast<engine_game_object_t>(original.prefabs.node(b, "3/Upper")) });
    const auto file = save(original);

    World loaded;
    loaded.prefabs.scene_files[ROOM] = room_file();
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
    const auto loaded_b = loaded.find(original.ids.get_id(b));
    ASSERT_NE(loaded_b, entt::entity{ entt::null });
    EXPECT_FLOAT_EQ(loaded.registry.get<engine_transform_component_t>(loaded.prefabs.node(loaded_b, "2")).position[1], 0.8f);
    EXPECT_EQ(parent(loaded, loaded.find(original.ids.get_id(holder))), loaded.prefabs.node(loaded_b, "3/Upper"));

    // the scene the file is loaded into has game objects already, which have the IDs of the file's game objects: the
    // reference into the instance still finds its game object
    World crowded;
    crowded.prefabs.scene_files[ROOM] = room_file();
    for (int i = 0; i < 5; i++)
    {
        crowded.create();
    }
    ASSERT_TRUE(load(crowded, file));
    auto crowded_holder = entt::entity{ entt::null };
    for (const auto entity : crowded.registry.view<const engine_parent_component_t>(entt::exclude<engine_name_component_t>))
    {
        crowded_holder = entity;
    }
    ASSERT_NE(crowded_holder, entt::entity{ entt::null });
    const auto upper = parent(crowded, crowded_holder);
    ASSERT_TRUE(crowded.registry.valid(upper));
    EXPECT_STREQ(crowded.registry.get<engine_name_component_t>(upper).name, "Upper");
    const auto crowded_b = parent(crowded, parent(crowded, upper));
    EXPECT_EQ(crowded.prefabs.node(crowded_b, "3/Upper"), upper);
    EXPECT_FLOAT_EQ(crowded.registry.get<engine_transform_component_t>(crowded_b).position[0], 10.0f);
}

TEST_F(ScenePrefabTest, ChangeToTheFileShowsInEveryInstanceWhereNothingOverridesIt)
{
    World original;
    const auto a = place_room(original, 0.0f);
    const auto b = place_room(original, 10.0f);
    auto table_transform = original.registry.get<engine_transform_component_t>(original.prefabs.node(a, "1"));
    table_transform.scale[1] = 2.0f;
    original.registry.replace<engine_transform_component_t>(original.prefabs.node(a, "1"), table_transform);
    const auto file = save(original);

    World loaded;
    loaded.prefabs.scene_files[ROOM] = room_file(5.0f);
    ASSERT_TRUE(load(loaded, file));
    const auto loaded_a = loaded.find(original.ids.get_id(a));
    const auto loaded_b = loaded.find(original.ids.get_id(b));
    const auto& table_a = loaded.registry.get<engine_transform_component_t>(loaded.prefabs.node(loaded_a, "1"));
    const auto& table_b = loaded.registry.get<engine_transform_component_t>(loaded.prefabs.node(loaded_b, "1"));
    EXPECT_FLOAT_EQ(table_a.position[0], 5.0f);
    EXPECT_FLOAT_EQ(table_a.scale[1], 2.0f);
    EXPECT_FLOAT_EQ(table_b.position[0], 5.0f);
    EXPECT_FLOAT_EQ(table_b.scale[1], 1.0f);
}

TEST_F(ScenePrefabTest, UndoOfADeletedInstanceRestoresItsHandles)
{
    World w;
    const auto root = place_room(w, 0.0f);
    std::map<std::string, entt::entity> nodes;
    for (const auto& node : w.prefabs.find_instance(root)->nodes)
    {
        nodes[node.path] = node.entity;
    }
    const auto scene = save(w);

    const auto saved = snapshot(w, root);
    destroy_with_children(w, root);
    ASSERT_TRUE(load(w, saved.file, saved.restore));
    for (const auto& [path, entity] : nodes)
    {
        EXPECT_EQ(w.prefabs.node(root, path), entity) << path;
    }
    EXPECT_EQ(save(w), scene);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(ScenePrefabTest, CopyOfAnInstanceHasTheIdsOfItsOwnRoot)
{
    World w;
    const auto root = place_room(w, 0.0f);
    const auto root_id = w.ids.get_id(root);

    engine::EntityHandles loaded;
    ASSERT_TRUE(load(w, snapshot(w, root).file, copy_options(loaded)));
    const auto copy = loaded.at(root_id);
    const auto upper = w.prefabs.node(copy, "3/Upper");
    EXPECT_EQ(w.ids.get_id(upper), EntityIdRegistry::hashed_id(w.ids.get_id(copy), "3/Upper"));
    EXPECT_EQ(loaded.at(EntityIdRegistry::hashed_id(root_id, "3/Upper")), upper);
    EXPECT_NE(upper, w.prefabs.node(root, "3/Upper"));
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(ScenePrefabTest, TransientInstanceHasNoIds)
{
    World w;
    w.prefabs.types = &types_;
    w.prefabs.assets = &assets_;
    w.prefabs.scene_files[ROOM] = room_file();
    const auto root = w.registry.create();
    ASSERT_NE(w.prefabs.instantiate(ROOM, root, {}, engine::InstanceIds{}), nullptr);
    for (const auto& node : w.prefabs.find_instance(root)->nodes)
    {
        EXPECT_EQ(w.ids.get_id(node.entity), EntityIdRegistry::INVALID_ID) << node.path;
    }
    EXPECT_EQ(parent(w, w.prefabs.node(root, "2")), w.prefabs.node(root, "1"));
    EXPECT_EQ(parent(w, w.prefabs.node(root, "3/Upper")), w.prefabs.node(root, "3"));
}

TEST_F(ScenePrefabTest, FileWhichPlacesItselfIsNotPlacedInsideItself)
{
    World w;
    w.prefabs.types = &types_;
    w.prefabs.assets = &assets_;
    w.prefabs.scene_files["rooms/loop.json"] = R"({ "version": 1, "entities": [ { "id": 1, "prefab": "rooms/loop.json" } ] })";
    const auto root = w.create();
    ASSERT_NE(w.prefabs.instantiate("rooms/loop.json", root), nullptr);
    EXPECT_TRUE(test_logger::logged("Scene prefab rooms/loop.json places itself"));
    EXPECT_TRUE(test_logger::logged("its prefab rooms/loop.json can't be instantiated"));
    // the placing game object inside it is there, without an instance
    const auto inner = w.prefabs.node(root, "1");
    ASSERT_NE(inner, entt::entity{ entt::null });
    EXPECT_EQ(w.prefabs.find_instance(inner), nullptr);
}

TEST_F(ScenePrefabTest, ScenePrefabInsideAScenePrefabHasTheIdsOfItsNodePaths)
{
    // a house placing the room
    constexpr const char* HOUSE = "houses/house.json";
    const auto house_file = fmt::format(R"({{ "version": 1, "entities": [ {{ "id": 4, "prefab": "{}", "nodes": {{
        "": {{ "components": {{ "name": {{ "name": "room" }} }} }}, "3/Upper/Hand": {{ "overrides": {{ "material": {{ "shininess": 4.0 }} }} }} }} }} ] }})", ROOM);
    World original;
    original.prefabs.types = &types_;
    original.prefabs.assets = &assets_;
    original.prefabs.scene_files[ROOM] = room_file();
    original.prefabs.scene_files[HOUSE] = house_file;
    const auto root = original.create();
    ASSERT_NE(original.prefabs.instantiate(HOUSE, root), nullptr);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();

    const auto room = original.prefabs.node(root, "4");
    ASSERT_NE(room, entt::entity{ entt::null });
    EXPECT_EQ(parent(original, room), root);
    EXPECT_EQ(parent(original, original.prefabs.node(root, "4/1")), room);
    // the room's override and the house's override both reach the arm's nodes
    const auto upper = original.prefabs.node(root, "4/3/Upper");
    ASSERT_NE(upper, entt::entity{ entt::null });
    EXPECT_TRUE(original.registry.get<engine_collider_component_t>(upper).is_trigger);
    const auto hand = original.prefabs.node(root, "4/3/Upper/Hand");
    ASSERT_NE(hand, entt::entity{ entt::null });
    EXPECT_FLOAT_EQ(original.registry.get<engine_material_component_t>(hand).shininess, 4.0f);
    const auto root_id = original.ids.get_id(root);
    EXPECT_EQ(original.ids.get_id(room), EntityIdRegistry::hashed_id(root_id, "4"));
    EXPECT_EQ(original.ids.get_id(original.prefabs.node(root, "4/2")), EntityIdRegistry::hashed_id(root_id, "4/2"));
    EXPECT_EQ(original.ids.get_id(upper), EntityIdRegistry::hashed_id(root_id, "4/3/Upper"));

    // an override of the level reaches into the room inside the house, and round trips
    auto hand_material = original.registry.get<engine_material_component_t>(hand);
    hand_material.shininess = 2.0f;
    original.registry.replace<engine_material_component_t>(hand, hand_material);
    const auto file = save(original);
    rapidjson::Document doc;
    doc.Parse(file.c_str());
    ASSERT_EQ(doc["entities"].Size(), 1u);
    EXPECT_TRUE(doc["entities"][0]["nodes"].HasMember("4/3/Upper/Hand")) << file;

    World loaded;
    loaded.prefabs.scene_files[ROOM] = room_file();
    loaded.prefabs.scene_files[HOUSE] = house_file;
    ASSERT_TRUE(load(loaded, file));
    EXPECT_EQ(save(loaded), file);
    const auto loaded_root = loaded.find(root_id);
    ASSERT_NE(loaded_root, entt::entity{ entt::null });
    EXPECT_FLOAT_EQ(loaded.registry.get<engine_material_component_t>(loaded.prefabs.node(loaded_root, "4/3/Upper/Hand")).shininess, 2.0f);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(ScenePrefabTest, GameObjectWhoseParentIsNotInTheFileStaysUnderTheRoot)
{
    World w;
    w.prefabs.types = &types_;
    w.prefabs.assets = &assets_;
    w.prefabs.scene_files["rooms/partial.json"] = R"({ "version": 1, "entities": [
        { "id": 1, "components": { "name": { "name": "lamp" }, "parent": { "parent": 99 } } } ] })";
    const auto root = w.create();
    ASSERT_NE(w.prefabs.instantiate("rooms/partial.json", root), nullptr);
    EXPECT_TRUE(test_logger::logged("references the game object 99 which isn't in the file"));
    EXPECT_EQ(parent(w, w.prefabs.node(root, "1")), root);
}

// -- play mode: clearing the scene and loading back what was saved at Play ------------------------------------------

class PlayModeRestoreTest : public ScenePrefabTest
{
protected:
    // what Stop does with the scene saved at Play (Scene::replace)
    bool restore(World& w, std::string_view snapshot)
    {
        engine::clear_game_objects(w.registry, w.ids);
        return load(w, snapshot);
    }

    // a level: every kind of component, a model's instance with overrides, a scene prefab, and a game object to reparent
    // and one to destroy while playing
    void build_level(World& w)
    {
        build_full_scene(w);
        override_arm(w, instantiate_arm(w));
        place_room(w, 5.0f);
        engine_name_component_t name{};
        std::strcpy(name.name, "moved");
        w.put<engine_name_component_t>(w.create(MOVED), name);
        std::strcpy(name.name, "destroyed");
        w.put<engine_name_component_t>(w.create(DESTROYED), name);
    }

    static constexpr std::uint64_t MOVED = 100;
    static constexpr std::uint64_t DESTROYED = 101;
};

TEST_F(PlayModeRestoreTest, RestoreGivesBackTheSceneSavedAtPlay)
{
    World w;
    build_level(w);
    const auto snapshot = save(w);
    std::map<std::uint64_t, entt::entity> before;
    for (const auto e : w.registry.view<entt::entity>())
    {
        if (const auto id = w.ids.get_id(e); id != EntityIdRegistry::INVALID_ID)
        {
            before[id] = e;
        }
    }

    // playing: a game object reparented, one destroyed, a runtime object spawned, a value changed
    const auto moved = w.find(MOVED);
    w.put<engine_parent_component_t>(moved, engine_parent_component_t{ static_cast<engine_game_object_t>(w.find(1)) });
    w.registry.destroy(w.find(DESTROYED));
    const auto spawned = w.registry.create();
    w.put<engine_transform_component_t>(spawned, engine_transform_component_t{});
    auto transform = w.get<engine_transform_component_t>(1);
    transform.position[0] = 42.0f;
    w.registry.replace<engine_transform_component_t>(w.find(1), transform);
    ASSERT_NE(save(w), snapshot);

    ASSERT_TRUE(restore(w, snapshot));
    // the same bytes, so saving after Stop gives what saving before Play gave
    EXPECT_EQ(save(w), snapshot);
    // every game object is back with its persistent ID, under a new handle: a handle kept from the play session finds nothing
    for (const auto& [id, old_handle] : before)
    {
        const auto restored = w.find(id);
        ASSERT_NE(restored, entt::entity{ entt::null }) << id;
        EXPECT_NE(restored, old_handle) << id;
        EXPECT_FALSE(w.registry.valid(old_handle)) << id;
    }
    EXPECT_FALSE(w.registry.all_of<engine_parent_component_t>(w.find(MOVED)));
    EXPECT_FALSE(w.registry.valid(spawned));
    EXPECT_EQ(count_entities(w.registry), before.size() + 1);  // and the reserved entity 0
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(PlayModeRestoreTest, ClearKeepsTheEditorsGameObjectsAndTheReservedEntity)
{
    World w;
    build_level(w);
    const auto editor_camera = w.registry.create();
    w.registry.emplace<engine::editor_object_internal_component_t>(editor_camera);
    w.put<engine_camera_component_t>(editor_camera, engine_camera_component_t{});
    const auto runtime_object = w.registry.create();

    engine::clear_game_objects(w.registry, w.ids);
    EXPECT_TRUE(w.registry.valid(editor_camera));
    EXPECT_TRUE(w.registry.all_of<engine_camera_component_t>(editor_camera));
    EXPECT_TRUE(w.registry.valid(entt::entity{ ENGINE_INVALID_GAME_OBJECT_ID }));
    EXPECT_FALSE(w.registry.valid(runtime_object));
    EXPECT_EQ(count_entities(w.registry), 2u);
    EXPECT_EQ(w.find(1), entt::entity{ entt::null });
    // the scene saves as an empty one
    World empty;
    EXPECT_EQ(save(w), save(empty));
}

TEST_F(PlayModeRestoreTest, IdsAfterARestoreAreTheOnesBeforePlay)
{
    World w;
    build_level(w);
    const auto snapshot = save(w);
    const auto next_before_play = w.ids.get_id(w.create());
    w.registry.destroy(w.find(next_before_play));

    // playing creates game objects with counter IDs (a level reload by the game, say)
    for (int i = 0; i < 5; i++)
    {
        w.create();
    }
    ASSERT_TRUE(restore(w, snapshot));
    // the counter starts again: the game object created next gets the ID it would have got before Play
    EXPECT_EQ(w.ids.get_id(w.create()), next_before_play);

    // and a scene built into a cleared one gets the IDs of a new scene
    engine::clear_game_objects(w.registry, w.ids);
    EXPECT_EQ(w.ids.get_id(w.create()), 1u);
}
}  // namespace
