#include "editor_commands.h"

#include <cassert>
#include <cstring>
#include <string>
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

