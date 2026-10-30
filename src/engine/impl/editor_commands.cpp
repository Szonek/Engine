#include "editor_commands.h"
#include "application.h"
#include "copy_name.h"
#include "logger.h"

#include <fmt/format.h>

#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace engine
{
void log_missing(std::string_view command)
{
    log::log(log::LogLevel::eError, fmt::format("Editor: the game object of an undo step ({}) isn't in the scene any more; it is skipped.\n", command));
}

void set_selection_markers(Scene& sc, entt::entity e, bool selected)
{
    if (e == entt::null || sc.has_component<engine::guizmo_component_t>(e) == selected)
    {
        return;
    }
    if (selected)
    {
        sc.add_component<engine::guizmo_component_t>(e);
        sc.add_component<engine::outline_component_t>(e);
    }
    else
    {
        sc.remove_component<engine::guizmo_component_t>(e);
        sc.remove_component<engine::outline_component_t>(e);
    }
}

CommandSetSelection::CommandSetSelection(Scene& sc, entt::entity prev_selected, entt::entity new_selected)
    : prev_(sc.make_ref(prev_selected))
    , new_(sc.make_ref(new_selected))
{
}

CommandSetSelection::CommandSetSelection(Scene& sc, IAssetSelection& assets, entt::entity prev_selected, entt::entity new_selected, std::string_view new_asset)
    : prev_(sc.make_ref(prev_selected))
    , new_(sc.make_ref(new_selected))
    , assets_(&assets)
    , asset_prev_(assets.get_selected_asset())
    , asset_new_(new_asset)
{
}

void CommandSetSelection::execute(Scene& sc)
{
    select(sc, prev_, new_, asset_new_);
}

void CommandSetSelection::undo(Scene& sc)
{
    select(sc, new_, prev_, asset_prev_);
}

void CommandSetSelection::select(Scene& sc, const GameObjectRef& deselected, const GameObjectRef& selected, const std::string& asset)
{
    set_selection_markers(sc, sc.resolve(deselected), false);
    set_selection_markers(sc, sc.resolve(selected), true);
    if (assets_ && assets_->get_selected_asset() != asset)
    {
        assets_->set_selected_asset(asset);
    }
}


CommandRenameEntity::CommandRenameEntity(Scene& sc, entt::entity e, std::string_view new_name)
    : ref_(sc.make_ref(e))
    , prev_name_(sc.get_component<engine_name_component_t>(e)->name)
    , new_name_(new_name.substr(0, ENGINE_ENTITY_NAME_MAX_LENGTH - 1))
{
}

void CommandRenameEntity::execute(Scene& sc)
{
    rename(sc, new_name_);
}

void CommandRenameEntity::undo(Scene& sc)
{
    rename(sc, prev_name_);
}

void CommandRenameEntity::rename(Scene& sc, const std::string& name)
{
    const auto e = sc.resolve(ref_);
    if (e == entt::null || !sc.has_component<engine_name_component_t>(e))
    {
        log_missing("rename");
        return;
    }
    auto nc = *sc.get_component<engine_name_component_t>(e);
    std::strcpy(nc.name, name.c_str());
    sc.update_component<engine_name_component_t>(e, nc);
}


void CommandAddEntity::execute(Scene& sc)
{
    const auto e = sc.create_new_entity();
    ref_ = sc.make_ref(e);
    auto nc = sc.add_component<engine_name_component_t>(e);
    const auto new_name = "Entity " + std::to_string(static_cast<std::uint32_t>(e));
    std::memcpy(nc->name, new_name.c_str(), std::min<std::size_t>(new_name.size(), ENGINE_ENTITY_NAME_MAX_LENGTH - 1));
    nc->name[std::min<std::size_t>(new_name.size(), ENGINE_ENTITY_NAME_MAX_LENGTH - 1)] = '\0';
}

void CommandAddEntity::undo(Scene& sc)
{
    const auto e = sc.resolve(ref_);
    if (e == entt::null)
    {
        log_missing("add a game object");
        return;
    }
    sc.destroy_entity(e);
}


CommandUpdateTypedComponent::CommandUpdateTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type, const void* new_value)
    : ref_(sc.make_ref(e))
    , type_(type)
    , prev_(type.size)
    , new_(type.size)
{
    sc.get_component(type_, e, prev_.get());
    std::memcpy(new_.get(), new_value, type_.size);
}

void CommandUpdateTypedComponent::execute(Scene& sc)
{
    apply(sc, new_);
}

void CommandUpdateTypedComponent::undo(Scene& sc)
{
    apply(sc, prev_);
}

void CommandUpdateTypedComponent::apply(Scene& sc, const ValueBuffer& value)
{
    if (const auto e = sc.resolve(ref_); e != entt::null && sc.has_component(type_, e))
    {
        sc.update_component(type_, e, value.get());
    }
    else
    {
        log_missing(type_.name);
    }
}

std::unique_ptr<ICommand> make_update_typed_component_command(Scene& sc, entt::entity e, const TypeInfo& type, const void* new_value)
{
    std::vector<std::unique_ptr<ICommand>> commands;
    commands.push_back(std::make_unique<CommandUpdateTypedComponent>(sc, e, type, new_value));
    for (const auto required : required_components(type, static_cast<const std::byte*>(new_value)))
    {
        const auto* required_type = sc.get_application()->get_type_registry().get(required);
        if (required_type && !sc.has_component(*required_type, e))
        {
            commands.push_back(std::make_unique<CommandAddTypedComponent>(sc, e, *required_type));
        }
    }
    if (commands.size() == 1)
    {
        return std::move(commands.front());
    }
    return std::make_unique<CommandSequence>(std::move(commands));
}

CommandAddTypedComponent::CommandAddTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type)
    : ref_(sc.make_ref(e))
    , type_(type)
{
}

void CommandAddTypedComponent::execute(Scene& sc)
{
    const auto e = sc.resolve(ref_);
    if (e == entt::null)
    {
        log_missing(type_.name);
        return;
    }
    if (!sc.has_component(type_, e))
    {
        sc.add_component(type_, e);
    }
}

void CommandAddTypedComponent::undo(Scene& sc)
{
    const auto e = sc.resolve(ref_);
    if (e == entt::null)
    {
        log_missing(type_.name);
        return;
    }
    sc.remove_component(type_, e);
}

CommandRemoveTypedComponent::CommandRemoveTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type)
    : ref_(sc.make_ref(e))
    , type_(type)
    , value_(type.size)
{
    sc.get_component(type_, e, value_.get());
}

void CommandRemoveTypedComponent::execute(Scene& sc)
{
    const auto e = sc.resolve(ref_);
    if (e == entt::null)
    {
        log_missing(type_.name);
        return;
    }
    sc.remove_component(type_, e);
}

void CommandRemoveTypedComponent::undo(Scene& sc)
{
    const auto e = sc.resolve(ref_);
    if (e == entt::null)
    {
        log_missing(type_.name);
        return;
    }
    if (!sc.has_component(type_, e))
    {
        sc.add_component(type_, e);
        sc.update_component(type_, e, value_.get());
    }
}

CommandDestroyEntity::CommandDestroyEntity(Scene& sc, entt::entity e)
    : ref_(sc.make_ref(e))
{
    assert(e != entt::null);
}

namespace
{
entt::entity parent_of(const Scene& scene, entt::entity e)
{
    return scene.has_component<engine_parent_component_t>(e) ? static_cast<entt::entity>(scene.get_component<engine_parent_component_t>(e)->parent) : entt::entity{ entt::null };
}

// Whether every game object a prefab instance created which is in the subtree of `e` has its instance's root in it too
// (above it, up to `e`). One which was moved out of its instance (parented under a game object outside of it) would be
// saved and loaded back apart from its instance, as a game object of its own with the ID of the instance's node.
bool holds_whole_instances(const Scene& sc, entt::entity e)
{
    const auto is_node_of = [&](entt::entity root, entt::entity node) {
        const auto* instance = sc.prefabs().find_instance(root);
        return instance && std::ranges::any_of(instance->nodes, [node](const PrefabInstanceNode& n) { return n.entity == node; });
    };
    for (const auto entity : sc.get_subtree(e))
    {
        if (!EntityIdRegistry::is_hashed_id(sc.get_persistent_id(entity)))
        {
            continue;
        }
        auto ancestor = entity;
        do
        {
            ancestor = parent_of(sc, ancestor);
        } while (ancestor != entt::null && ancestor != e && !is_node_of(ancestor, entity));
        if (ancestor == entt::null || !is_node_of(ancestor, entity))
        {
            return false;
        }
    }
    return true;
}

// Gives `e` a name none of its siblings (the game objects with its parent, or without one) has, see copy_name; a name
// no sibling has stays.
void make_name_unique_among_siblings(Scene& scene, entt::entity e)
{
    if (!scene.has_component<engine_name_component_t>(e))
    {
        return;
    }
    const auto parent = parent_of(scene, e);
    std::vector<std::string> sibling_names;
    const auto add_sibling_name = [&](entt::entity sibling)
        {
            if (sibling != e && scene.has_component<engine_name_component_t>(sibling))
            {
                sibling_names.emplace_back(scene.get_component<engine_name_component_t>(sibling)->name);
            }
        };
    if (parent != entt::null)
    {
        std::ranges::for_each(scene.get_children(parent), add_sibling_name);
    }
    else
    {
        // the game objects without a parent have no list of their own
        for (const auto sibling : scene.get_all_entities())
        {
            if (parent_of(scene, sibling) == entt::null)
            {
                add_sibling_name(sibling);
            }
        }
    }
    auto name = *scene.get_component<engine_name_component_t>(e);
    if (std::ranges::find(sibling_names, name.name) == sibling_names.end())
    {
        return;
    }
    const auto new_name = copy_name(name.name, sibling_names, ENGINE_ENTITY_NAME_MAX_LENGTH);
    std::memcpy(name.name, new_name.c_str(), new_name.size() + 1);
    scene.update_component<engine_name_component_t>(e, name);
}
}  // namespace

bool CommandDestroyEntity::can_destroy(const Scene& sc, entt::entity e)
{
    return !EntityIdRegistry::is_hashed_id(sc.get_persistent_id(e)) && holds_whole_instances(sc, e);
}

void CommandDestroyEntity::execute(Scene& sc)
{
    saved_.clear();
    restore_ = {};
    parent_ = {};
    const auto e = sc.resolve(ref_);
    if (e == entt::null)
    {
        log_missing("delete");
        return;
    }
    if (!can_destroy(sc, e))
    {
        return;
    }
    const auto subtree = sc.get_subtree(e);
    for (const auto entity : subtree)
    {
        if (const auto id = sc.get_persistent_id(entity); id != EntityIdRegistry::INVALID_ID)
        {
            restore_.handles[id] = entity;
        }
    }
    // the parent (and anything else outside of what is destroyed) is referenced by persistent ID
    restore_.find_references_outside_file = true;
    if (const auto parent = parent_of(sc, e); parent != entt::null)
    {
        parent_ = sc.make_ref(parent);
        const auto siblings = sc.get_children(parent);
        child_index_ = static_cast<std::size_t>(std::find(siblings.begin(), siblings.end(), e) - siblings.begin());
    }
    saved_ = sc.save(subtree);
    sc.destroy_entity(e);
}

void CommandDestroyEntity::undo(Scene& sc)
{
    if (saved_.empty())
    {
        return;
    }
    sc.load(saved_, "undo of a destroy", restore_);
    if (const auto parent = sc.resolve(parent_); parent != entt::null)
    {
        sc.set_child_index(parent, sc.resolve(ref_), child_index_);
    }
}

CommandDuplicateEntity::CommandDuplicateEntity(Scene& sc, entt::entity e, entt::entity selected)
    : ref_(sc.make_ref(e))
    , selected_(sc.make_ref(selected))
{
}

bool CommandDuplicateEntity::can_duplicate(const Scene& sc, entt::entity e)
{
    const auto id = sc.get_persistent_id(e);
    return sc.is_valid_entity(e) && id != EntityIdRegistry::INVALID_ID && !EntityIdRegistry::is_hashed_id(id) && holds_whole_instances(sc, e);
}

void CommandDuplicateEntity::execute(Scene& sc)
{
    if (!destroy_copy_)
    {
        duplicate(sc);
    }
    else
    {
        destroy_copy_->undo(sc);
    }
    if (select_copy_)
    {
        select_copy_->execute(sc);
    }
}

void CommandDuplicateEntity::undo(Scene& sc)
{
    if (select_copy_)
    {
        select_copy_->undo(sc);
    }
    if (destroy_copy_)
    {
        destroy_copy_->execute(sc);
    }
}

void CommandDuplicateEntity::duplicate(Scene& sc)
{
    const auto e = sc.resolve(ref_);
    if (e == entt::null)
    {
        log_missing("duplicate");
        return;
    }
    if (!can_duplicate(sc, e))
    {
        log::log(log::LogLevel::eError, "Editor: a runtime (transient) object, a part of a prefab instance or a game object holding a part of one without its "
            "instance can't be duplicated.\n");
        return;
    }
    EntityHandles loaded;
    SceneLoadOptions copy;
    copy.new_ids = true;
    // the parent (and anything else outside of what is copied) stays what it is
    copy.find_references_outside_file = true;
    copy.loaded = &loaded;
    if (!sc.load(sc.save(sc.get_subtree(e)), "duplicate", copy))
    {
        return;
    }
    const auto it = loaded.find(sc.get_persistent_id(e));
    if (it == loaded.end())
    {
        return;
    }
    const auto copy_entity = it->second;
    copy_ = sc.make_ref(copy_entity);

    // the name, unique among the siblings, and the place right after the original
    make_name_unique_among_siblings(sc, copy_entity);
    if (const auto parent = parent_of(sc, e); parent != entt::null)
    {
        const auto siblings = sc.get_children(parent);
        const auto index = static_cast<std::size_t>(std::find(siblings.begin(), siblings.end(), e) - siblings.begin());
        sc.set_child_index(parent, copy_entity, index + 1);
    }

    // a scene has one nav surface for now: the nav mesh build keeps using the original's
    const auto copy_subtree = sc.get_subtree(copy_entity);
    if (std::ranges::any_of(copy_subtree, [&sc](entt::entity c) { return sc.has_component<engine_nav_surface_component_t>(c); }))
    {
        log::log(log::LogLevel::eError, "Editor: the copy has a nav surface, so the scene has more than one now; the nav mesh is built from the one "
            "with the lowest persistent ID (the original's).\n");
    }

    destroy_copy_ = std::make_unique<CommandDestroyEntity>(sc, copy_entity);
    select_copy_ = std::make_unique<CommandSetSelection>(sc, sc.resolve(selected_), copy_entity);
}

CommandPlacePrefab::CommandPlacePrefab(Scene& sc, IAssetSelection& assets, std::string_view prefab_asset_id, const glm::vec3& position, entt::entity selected)
    : assets_(assets)
    , asset_id_(prefab_asset_id)
    , position_(position)
    , selected_(sc.make_ref(selected))
{
}

void CommandPlacePrefab::execute(Scene& sc)
{
    if (!destroy_instance_)
    {
        place(sc);
    }
    else
    {
        destroy_instance_->undo(sc);
    }
    if (select_instance_)
    {
        select_instance_->execute(sc);
    }
}

void CommandPlacePrefab::undo(Scene& sc)
{
    if (select_instance_)
    {
        select_instance_->undo(sc);
    }
    if (destroy_instance_)
    {
        destroy_instance_->execute(sc);
    }
}

void CommandPlacePrefab::place(Scene& sc)
{
    // logs why when it can't be loaded
    const auto prefab = sc.assets().resolve_prefab(asset_id_);
    if (prefab == ENGINE_INVALID_OBJECT_HANDLE)
    {
        return;
    }
    const auto root = sc.prefabs().instantiate_prefab(prefab, Prefab::Instance::ePersistent);
    if (root == entt::null)
    {
        return;
    }
    root_ = sc.make_ref(root);
    if (!sc.has_component<engine_transform_component_t>(root))
    {
        sc.add_component<engine_transform_component_t>(root);
    }
    auto transform = *sc.get_component<engine_transform_component_t>(root);
    std::memcpy(transform.position, &position_[0], sizeof(transform.position));
    sc.update_component<engine_transform_component_t>(root, transform);
    make_name_unique_among_siblings(sc, root);

    destroy_instance_ = std::make_unique<CommandDestroyEntity>(sc, root);
    // the file selected before (the one dragged in, often) comes back with undo
    select_instance_ = std::make_unique<CommandSetSelection>(sc, assets_, sc.resolve(selected_), root, "");
}


CommandSetPhysicsDebugDraw::CommandSetPhysicsDebugDraw(bool flag)
    : flag_(flag)
{
}

void CommandSetPhysicsDebugDraw::execute(Scene& sc)
{
    sc.physics().enable_debug_draw(flag_);
}

void CommandSetPhysicsDebugDraw::undo(Scene& sc)
{
    sc.physics().enable_debug_draw(!flag_);
}

} // namespace engine

