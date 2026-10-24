#include "editor_commands.h"
#include "copy_name.h"
#include "logger.h"

#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace engine
{
CommandSetSelectedEntity::CommandSetSelectedEntity(Scene& sc, entt::entity prev_selected, entt::entity new_selected)
    : sc_(sc)
    , e_prev_(prev_selected)
    , e_new_(new_selected)
{
}

void CommandSetSelectedEntity::execute()
{
    if (e_prev_ != entt::null)
    {
        sc_.remove_component<engine::guizmo_component_t>(e_prev_);
        sc_.remove_component<engine::outline_component_t>(e_prev_);
    }
    if (e_new_ != entt::null)
    {
        sc_.add_component<engine::guizmo_component_t>(e_new_);
        sc_.add_component<engine::outline_component_t>(e_new_);
    }
}

void CommandSetSelectedEntity::undo()
{
    if (e_new_ != entt::null)
    {
        sc_.remove_component<engine::guizmo_component_t>(e_new_);
        sc_.remove_component<engine::outline_component_t>(e_new_);
    }
    if (e_prev_ != entt::null)
    {
        sc_.add_component<engine::guizmo_component_t>(e_prev_);
        sc_.add_component<engine::outline_component_t>(e_prev_);
    }
}


CommandRenameEntity::CommandRenameEntity(Scene& sc, entt::entity e, std::string_view new_name)
    : scene_(sc)
    , e_(e)
{
    new_name_.resize(ENGINE_ENTITY_NAME_MAX_LENGTH);
    new_name_ = new_name;
    auto nc = scene_.get_component<engine_name_component_t>(e_);
    prev_name_ = nc->name;
}

void CommandRenameEntity::execute()
{
    auto nc = scene_.get_component<engine_name_component_t>(e_);
    std::strcpy(nc->name, new_name_.c_str());
    scene_.update_component<engine_name_component_t>(e_, *nc);
}

void CommandRenameEntity::undo()
{
    auto nc = scene_.get_component<engine_name_component_t>(e_);
    std::strcpy(nc->name, prev_name_.c_str());
    scene_.update_component<engine_name_component_t>(e_, *nc);
}


CommandAddEntity::CommandAddEntity(Scene& sc)
    : scene_(sc)
{
}

void CommandAddEntity::execute()
{
    assert(e_ == entt::null);
    e_ = scene_.create_new_entity();
    auto nc = scene_.add_component<engine_name_component_t>(e_);
    const auto new_name = "Entity " + std::to_string(static_cast<std::uint32_t>(e_));
    std::memcpy(nc->name, new_name.c_str(), std::min<std::size_t>(new_name.size(), ENGINE_ENTITY_NAME_MAX_LENGTH - 1));
    nc->name[std::min<std::size_t>(new_name.size(), ENGINE_ENTITY_NAME_MAX_LENGTH - 1)] = '\0';
}

void CommandAddEntity::undo()
{
    assert(e_ != entt::null);
    scene_.destroy_entity(e_);
    e_ = entt::null;
}


CommandUpdateTypedComponent::CommandUpdateTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type, const void* new_value)
    : sc_(sc)
    , e_(e)
    , type_(type)
    , prev_(type.size)
    , new_(type.size)
{
    sc_.get_component(type_, e_, prev_.get());
    std::memcpy(new_.get(), new_value, type_.size);
}

void CommandUpdateTypedComponent::execute()
{
    apply(new_);
}

void CommandUpdateTypedComponent::undo()
{
    apply(prev_);
}

void CommandUpdateTypedComponent::apply(const ValueBuffer& value)
{
    if (sc_.is_valid_entity(e_) && sc_.has_component(type_, e_))
    {
        sc_.update_component(type_, e_, value.get());
    }
}

CommandAddTypedComponent::CommandAddTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type)
    : sc_(sc)
    , e_(e)
    , type_(type)
{
}

void CommandAddTypedComponent::execute()
{
    if (sc_.is_valid_entity(e_) && !sc_.has_component(type_, e_))
    {
        sc_.add_component(type_, e_);
    }
}

void CommandAddTypedComponent::undo()
{
    if (sc_.is_valid_entity(e_))
    {
        sc_.remove_component(type_, e_);
    }
}

CommandRemoveTypedComponent::CommandRemoveTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type)
    : sc_(sc)
    , e_(e)
    , type_(type)
    , value_(type.size)
{
    sc_.get_component(type_, e_, value_.get());
}

void CommandRemoveTypedComponent::execute()
{
    if (sc_.is_valid_entity(e_))
    {
        sc_.remove_component(type_, e_);
    }
}

void CommandRemoveTypedComponent::undo()
{
    if (sc_.is_valid_entity(e_) && !sc_.has_component(type_, e_))
    {
        sc_.add_component(type_, e_);
        sc_.update_component(type_, e_, value_.get());
    }
}

CommandDestroyEntity::CommandDestroyEntity(Scene& sc, entt::entity e)
    : scene_(sc)
    , e_(e)
{
    assert(e_ != entt::null);
}

bool CommandDestroyEntity::can_destroy(const Scene& sc, entt::entity e)
{
    return !EntityIdRegistry::is_hashed_id(sc.get_persistent_id(e));
}

void CommandDestroyEntity::execute()
{
    saved_.clear();
    restore_ = {};
    parent_ = entt::null;
    if (!scene_.is_valid_entity(e_) || !can_destroy(scene_, e_))
    {
        return;
    }
    const auto subtree = scene_.get_subtree(e_);
    for (const auto entity : subtree)
    {
        if (const auto id = scene_.get_persistent_id(entity); id != EntityIdRegistry::INVALID_ID)
        {
            restore_.handles[id] = entity;
        }
    }
    // the parent (and anything else outside of what is destroyed) is referenced by persistent ID
    restore_.find_references_outside_file = true;
    if (scene_.has_component<engine_parent_component_t>(e_))
    {
        parent_ = static_cast<entt::entity>(scene_.get_component<engine_parent_component_t>(e_)->parent);
        const auto siblings = scene_.get_children(parent_);
        child_index_ = static_cast<std::size_t>(std::find(siblings.begin(), siblings.end(), e_) - siblings.begin());
    }
    saved_ = scene_.save(subtree);
    scene_.destroy_entity(e_);
}

void CommandDestroyEntity::undo()
{
    if (saved_.empty())
    {
        return;
    }
    scene_.load(saved_, "undo of a destroy", restore_);
    if (parent_ != entt::null)
    {
        scene_.set_child_index(parent_, e_, child_index_);
    }
}

namespace
{
entt::entity parent_of(const Scene& scene, entt::entity e)
{
    return scene.has_component<engine_parent_component_t>(e) ? static_cast<entt::entity>(scene.get_component<engine_parent_component_t>(e)->parent) : entt::entity{ entt::null };
}
}  // namespace

CommandDuplicateEntity::CommandDuplicateEntity(Scene& sc, entt::entity e, entt::entity selected)
    : scene_(sc)
    , e_(e)
    , selected_(selected)
{
}

bool CommandDuplicateEntity::can_duplicate(const Scene& sc, entt::entity e)
{
    const auto id = sc.get_persistent_id(e);
    return sc.is_valid_entity(e) && id != EntityIdRegistry::INVALID_ID && !EntityIdRegistry::is_hashed_id(id);
}

void CommandDuplicateEntity::execute()
{
    if (!destroy_copy_)
    {
        duplicate();
    }
    else
    {
        destroy_copy_->undo();
    }
    if (select_copy_)
    {
        select_copy_->execute();
    }
}

void CommandDuplicateEntity::undo()
{
    if (select_copy_)
    {
        select_copy_->undo();
    }
    if (destroy_copy_)
    {
        destroy_copy_->execute();
    }
}

void CommandDuplicateEntity::duplicate()
{
    if (!can_duplicate(scene_, e_))
    {
        log::log(log::LogLevel::eError, "Editor: a runtime (transient) object or a part of a prefab instance can't be duplicated.\n");
        return;
    }
    EntityHandles loaded;
    SceneLoadOptions copy;
    copy.new_ids = true;
    // the parent (and anything else outside of what is copied) stays what it is
    copy.find_references_outside_file = true;
    copy.loaded = &loaded;
    if (!scene_.load(scene_.save(scene_.get_subtree(e_)), "duplicate", copy))
    {
        return;
    }
    const auto it = loaded.find(scene_.get_persistent_id(e_));
    if (it == loaded.end())
    {
        return;
    }
    copy_ = it->second;

    // the name, unique among the siblings, and the place right after the original
    const auto parent = parent_of(scene_, e_);
    std::vector<std::string> sibling_names;
    const auto add_sibling_name = [&](entt::entity sibling)
        {
            if (sibling != copy_ && scene_.has_component<engine_name_component_t>(sibling))
            {
                sibling_names.emplace_back(scene_.get_component<engine_name_component_t>(sibling)->name);
            }
        };
    if (parent != entt::null)
    {
        std::ranges::for_each(scene_.get_children(parent), add_sibling_name);
    }
    else
    {
        // the game objects without a parent have no list of their own
        for (const auto sibling : scene_.get_all_entities())
        {
            if (parent_of(scene_, sibling) == entt::null)
            {
                add_sibling_name(sibling);
            }
        }
    }
    if (scene_.has_component<engine_name_component_t>(copy_))
    {
        auto name = *scene_.get_component<engine_name_component_t>(copy_);
        const auto new_name = copy_name(name.name, sibling_names, ENGINE_ENTITY_NAME_MAX_LENGTH);
        std::memcpy(name.name, new_name.c_str(), new_name.size() + 1);
        scene_.update_component<engine_name_component_t>(copy_, name);
    }
    if (parent != entt::null)
    {
        const auto siblings = scene_.get_children(parent);
        const auto index = static_cast<std::size_t>(std::find(siblings.begin(), siblings.end(), e_) - siblings.begin());
        scene_.set_child_index(parent, copy_, index + 1);
    }

    // a scene has one nav surface for now: the nav mesh build keeps using the original's
    const auto copy_subtree = scene_.get_subtree(copy_);
    if (std::ranges::any_of(copy_subtree, [this](entt::entity e) { return scene_.has_component<engine_nav_surface_component_t>(e); }))
    {
        log::log(log::LogLevel::eError, "Editor: the copy has a nav surface, so the scene has more than one now; the nav mesh is built from the one "
            "with the lowest persistent ID (the original's).\n");
    }

    destroy_copy_ = std::make_unique<CommandDestroyEntity>(scene_, copy_);
    select_copy_ = std::make_unique<CommandSetSelectedEntity>(scene_, selected_, copy_);
}


CommandSetPhysicsDebugDraw::CommandSetPhysicsDebugDraw(Scene& sc, bool flag)
    : scene_(sc)
    , flag_(flag)
{
}

void CommandSetPhysicsDebugDraw::execute()
{
    scene_.physics().enable_debug_draw(flag_);
}

void CommandSetPhysicsDebugDraw::undo()
{
    scene_.physics().enable_debug_draw(!flag_);
}

} // namespace engine

