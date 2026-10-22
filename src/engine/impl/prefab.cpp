#include "prefab.h"

#include "application.h"
#include "asset_id.h"
#include "logger.h"
#include "scene.h"
#include "components_internals/prefab_instance_component.h"

#include <engine.h>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <utility>

namespace
{
using engine::log::LogLevel;

// the reserved entity which stands for "no game object" (ENGINE_INVALID_GAME_OBJECT_ID)
constexpr entt::entity NO_ENTITY{ ENGINE_INVALID_GAME_OBJECT_ID };

// Components are added first and then updated with their values: adding a component initializes it to its defaults
// and updating it is what the scene's observers react to (as it is for a game's own calls).
template<typename T>
void set_component(engine::Scene& scene, entt::entity entity, const T& value)
{
    scene.add_component<T>(entity);
    scene.update_component<T>(entity, value);
}

void copy_vec3(float (&dst)[3], const glm::vec3& src)
{
    dst[0] = src.x;
    dst[1] = src.y;
    dst[2] = src.z;
}
}  // namespace

std::unique_ptr<engine::Prefab> engine::Prefab::create(Application& app, ModelDesc&& model)
{
    std::unique_ptr<Prefab> prefab(new Prefab(app, std::move(model)));
    if (!prefab->create_assets())
    {
        return nullptr;
    }
    return prefab;
}

engine::Prefab::Prefab(Application& app, ModelDesc&& model)
    : app_(app)
    , model_(std::move(model))
{
}

engine::Prefab::~Prefab()
{
    if (animation_set_ != ENGINE_INVALID_OBJECT_HANDLE)
    {
        app_.release_animation_set(animation_set_);
    }
    if (skin_ != ENGINE_INVALID_OBJECT_HANDLE)
    {
        app_.release_skin(skin_);
    }
    for (const auto texture : textures_)
    {
        app_.release_texture(texture);
    }
    for (const auto geometry : geometries_)
    {
        app_.release_geometry(geometry);
    }
}

bool engine::Prefab::create_assets()
{
    for (const auto& geometry_desc : model_.geometries)
    {
        const auto geometry = app_.add_geometry(geometry_desc.vertex_laytout, geometry_desc.vertex_count, geometry_desc.vertex_data, geometry_desc.indicies, geometry_desc.name);
        if (geometry == ENGINE_INVALID_OBJECT_HANDLE)
        {
            log::log(LogLevel::eError, fmt::format("Prefab {}: failed creating geometry {}.\n", get_name(), geometry_desc.name));
            return false;
        }
        geometries_.push_back(geometry);
    }
    if (!create_textures())
    {
        return false;
    }
    create_materials();
    return create_skin() && create_animation_set();
}

bool engine::Prefab::create_textures()
{
    for (const auto& texture_desc : model_.textures)
    {
        // an image in its own file may be used already (by another model or the game): the prefab only uses it too
        if (const auto existing = app_.get_texture(texture_desc.name); existing != ENGINE_INVALID_OBJECT_HANDLE && app_.acquire_texture(existing))
        {
            textures_.push_back(existing);
            continue;
        }
        const auto texture = app_.add_texture(texture_desc);
        if (texture == ENGINE_INVALID_OBJECT_HANDLE)
        {
            log::log(LogLevel::eError, fmt::format("Prefab {}: failed creating texture {}.\n", get_name(), texture_desc.name));
            return false;
        }
        textures_.push_back(texture);
    }
    return true;
}

void engine::Prefab::create_materials()
{
    // value-initialized: a texture a material doesn't use is index 0, the application's default texture
    materials_ = std::vector<engine_material_component_t>(model_.materials.size());
    for (std::size_t i = 0; i < materials_.size(); i++)
    {
        const auto& material_desc = model_.materials[i];
        auto& material = materials_[i];
        material.diffuse_color[0] = material_desc.diffuse_factor.r;
        material.diffuse_color[1] = material_desc.diffuse_factor.g;
        material.diffuse_color[2] = material_desc.diffuse_factor.b;
        material.diffuse_color[3] = material_desc.diffuse_factor.a;
        if (material_desc.diffuse_texture != INVALID_VALUE)
        {
            material.diffuse_texture = textures_.at(material_desc.diffuse_texture);
        }
        material.shininess = 32;
    }
}

bool engine::Prefab::create_skin()
{
    if (model_.skins.empty())
    {
        return true;
    }
    if (model_.skins.size() > 1)
    {
        log::log(LogLevel::eError, fmt::format("Prefab {}: only models with a single skin are supported.\n", get_name()));
        return false;
    }

    // the skeleton's root is the joint which comes last in the model
    const auto& skin_desc = model_.skins.front();
    const ModelNodeDesc* root = nullptr;
    for (const auto& [joint_name, inverse_bind_matrix] : skin_desc.inverse_bind_matrix_map)
    {
        const auto* joint = find_node(joint_name);
        if (joint && (!root || joint->index > root->index))
        {
            root = joint;
        }
    }
    if (!root)
    {
        log::log(LogLevel::eError, fmt::format("Prefab {}: none of the joints of skin {} is a node of the model.\n", get_name(), skin_desc.asset_id));
        return false;
    }
    skin_ = app_.add_skin(skin_desc, *root);
    return skin_ != ENGINE_INVALID_OBJECT_HANDLE;
}

bool engine::Prefab::create_animation_set()
{
    // the model's animations, compiled once for its skin; every instance's controller plays them
    if (skin_ == ENGINE_INVALID_OBJECT_HANDLE || model_.animations.empty())
    {
        return true;
    }
    const auto* skin = app_.get_skin(skin_);
    auto library = std::make_shared<AnimationLibrary>(*skin);
    for (const auto& animation : model_.animations)
    {
        if (!library->add_animation(animation, *skin))
        {
            log::log(LogLevel::eError, fmt::format("Prefab {}: failed compiling animation {}.\n", get_name(), animation.name));
            return false;
        }
    }
    animation_set_ = app_.add_animation_set(make_asset_id(model_.source_name, "animations", "animations", 0), std::move(library));
    return animation_set_ != ENGINE_INVALID_OBJECT_HANDLE;
}

const engine::ModelNodeDesc* engine::Prefab::find_node(const std::string& name) const
{
    const auto it = std::find_if(model_.nodes.begin(), model_.nodes.end(), [&name](const auto& node) { return node->name == name; });
    return it != model_.nodes.end() ? it->get() : nullptr;
}

entt::entity engine::Prefab::instantiate(Scene& scene, Instance instance) const
{
    const SkinDesc* skin_desc = model_.skins.empty() ? nullptr : &model_.skins.front();
    // the game object owning this instance's skin (and its pose); skinned meshes and joint attachments point at it
    auto skin_owner = NO_ENTITY;
    std::vector<entt::entity> skinned_meshes;
    auto root = NO_ENTITY;

    std::map<std::int32_t, entt::entity> node_index_to_entity;
    for (const auto& node : model_.nodes)
    {
        const auto entity = instance == Instance::eTransient ? scene.create_transient_entity() : scene.create_new_entity();
        node_index_to_entity[node->index] = entity;

        engine_name_component_t name{};
        std::strncpy(name.name, node->name.c_str(), std::size(name.name) - 1);
        set_component(scene, entity, name);

        // the node with the same name as the skin owns the skin and the animation controller
        if (skin_desc && skin_desc->name == node->name)
        {
            engine_skin_component_t skin{};
            skin.skin = skin_;
            set_component(scene, entity, skin);
            skin_owner = entity;

            // the engine creates the controller of this instance
            if (animation_set_ != ENGINE_INVALID_OBJECT_HANDLE)
            {
                auto controller = *scene.add_component<engine_animation_controller_component_t>(entity);
                controller.animations = animation_set_;
                scene.update_component(entity, controller);
            }
        }

        engine_transform_component_t transform{};
        copy_vec3(transform.position, node->translation);
        transform.rotation[0] = node->rotation.x;
        transform.rotation[1] = node->rotation.y;
        transform.rotation[2] = node->rotation.z;
        transform.rotation[3] = node->rotation.w;
        copy_vec3(transform.scale, node->scale);
        set_component(scene, entity, transform);

        if (node->mesh != INVALID_VALUE)
        {
            if (node->skin != INVALID_VALUE)
            {
                // its skin owner is set once every node exists
                engine_skinned_mesh_component_t mesh{};
                mesh.geometry = geometries_.at(node->mesh);
                set_component(scene, entity, mesh);
                skinned_meshes.push_back(entity);
            }
            else
            {
                engine_mesh_component_t mesh{};
                mesh.geometry = geometries_.at(node->mesh);
                set_component(scene, entity, mesh);
            }
        }

        if (node->material != INVALID_VALUE)
        {
            set_component(scene, entity, materials_.at(node->material));
        }

        if (!node->parent)
        {
            root = entity;
        }
    }
    if (root == NO_ENTITY)
    {
        log::log(LogLevel::eError, fmt::format("Prefab {}: the model has no root node.\n", get_name()));
        for (const auto& [index, entity] : node_index_to_entity)
        {
            scene.destroy_entity(entity);
        }
        return root;
    }

    for (const auto entity : skinned_meshes)
    {
        auto mesh = *scene.get_component<engine_skinned_mesh_component_t>(entity);
        mesh.skin = static_cast<engine_game_object_t>(skin_owner);
        scene.update_component(entity, mesh);
    }

    // joints are not needed as game objects, but the objects which parent was a joint follow it
    if (skin_desc)
    {
        for (const auto& [joint_name, inverse_bind_matrix] : skin_desc->inverse_bind_matrix_map)
        {
            const auto* joint = find_node(joint_name);
            if (!joint)
            {
                continue;
            }
            scene.destroy_entity(node_index_to_entity.at(joint->index));
            node_index_to_entity.erase(joint->index);

            for (const auto* child : joint->children)
            {
                const auto child_entity = node_index_to_entity.find(child->index);
                if (child->mesh == INVALID_VALUE || child_entity == node_index_to_entity.end())
                {
                    continue;
                }
                auto attachment = *scene.add_component<engine_joint_attachment_component_t>(child_entity->second);
                attachment.skin = static_cast<engine_game_object_t>(skin_owner);
                engineStringSet(attachment.joint_name, joint_name.c_str());
                scene.update_component(child_entity->second, attachment);
            }
        }
    }

    // all objects are children of the root
    for (const auto& [index, entity] : node_index_to_entity)
    {
        if (entity != root)
        {
            engine_parent_component_t parent{};
            parent.parent = static_cast<engine_game_object_t>(root);
            set_component(scene, entity, parent);
        }
    }
    return root;
}

engine::prefab_instance_internal_component_t::prefab_instance_internal_component_t(Application& app, std::uint32_t prefab)
    : app_(&app)
    , prefab_(prefab)
{
    const auto acquired = app_->acquire_prefab(prefab_);
    assert(acquired && "the prefab of an instance must be valid");
    if (!acquired)
    {
        app_ = nullptr;
    }
}

engine::prefab_instance_internal_component_t::prefab_instance_internal_component_t(prefab_instance_internal_component_t&& rhs) noexcept
    : app_(std::exchange(rhs.app_, nullptr))
    , prefab_(rhs.prefab_)
{
}

engine::prefab_instance_internal_component_t& engine::prefab_instance_internal_component_t::operator=(prefab_instance_internal_component_t&& rhs) noexcept
{
    if (this != &rhs)
    {
        release();
        app_ = std::exchange(rhs.app_, nullptr);
        prefab_ = rhs.prefab_;
    }
    return *this;
}

engine::prefab_instance_internal_component_t::~prefab_instance_internal_component_t()
{
    release();
}

void engine::prefab_instance_internal_component_t::release()
{
    if (app_)
    {
        std::exchange(app_, nullptr)->release_prefab(prefab_);
    }
}
