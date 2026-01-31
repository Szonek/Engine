#pragma once
#include "scene.h"

#include <vector>
#include <memory>

namespace engine
{

class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual bool merge_with(const ICommand* other)
    {
        return false;
    }
};

class CommandManager
{
public:
    void execute_command(std::unique_ptr<ICommand> cmd)
    {
        if (!undo_stack_.empty())
        {
            const auto last = undo_stack_.back().get();
            if (last->merge_with(cmd.get()))
            {
                // dont push new cmd, merged with last
                return;
            }
        }
        cmd->execute();
        undo_stack_.push_back(std::move(cmd));
        redo_stack_.clear();
        if (undo_stack_.size() > max_history_)
        {
            undo_stack_.erase(undo_stack_.begin());
        }
    }

    void undo()
    {
        if (undo_stack_.empty())
        {
            return;
        }
        auto cmd = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        cmd->undo();
        redo_stack_.push_back(std::move(cmd));
    }

    void redo()
    {
        if (redo_stack_.empty())
        {
            return;
        }
        auto cmd = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        cmd->execute();
        undo_stack_.push_back(std::move(cmd));
    }

private:
    std::vector<std::unique_ptr<ICommand>> undo_stack_;
    std::vector<std::unique_ptr<ICommand>> redo_stack_;
    const std::size_t max_history_ = 100;
};

class CommandRenameEntity : public ICommand
{
public:
    CommandRenameEntity(Scene& sc, entt::entity e, std::string_view new_name)
        : scene_(sc)
        , e_(e)
    {
        new_name_.resize(ENGINE_ENTITY_NAME_MAX_LENGTH);
        new_name_ = new_name;
        auto nc = scene_.get_component<engine_name_component_t>(e_);
        prev_name_ = nc->name;
    }

    void execute() override
    {
        auto nc = scene_.get_component<engine_name_component_t>(e_);
        std::strcpy(nc->name, new_name_.c_str());
        scene_.update_component<engine_name_component_t>(e_, *nc);

    }

    void undo() override
    {
        auto nc = scene_.get_component<engine_name_component_t>(e_);
        std::strcpy(nc->name, prev_name_.c_str());
        scene_.update_component<engine_name_component_t>(e_, *nc);
    }

private:
    Scene& scene_;
    const entt::entity e_ = entt::null;
    std::string prev_name_;
    std::string new_name_;
};

class CommandAddEntity : public ICommand
{
public:
    CommandAddEntity(Scene& sc)
        : scene_(sc)
    {
    }

    void execute() override
    {
        assert(e_ == entt::null);
        e_ = scene_.create_new_entity();
        auto nc = scene_.add_component<engine_name_component_t>(e_);
        const auto new_name = "Entity " + std::to_string(static_cast<std::uint32_t>(e_));
        std::memcpy(nc->name, new_name.c_str(), new_name.size());
    }

    void undo() override
    {
        assert(e_ != entt::null);
        scene_.destroy_entity(e_);
        e_ = entt::null;
    }

private:
    Scene& scene_;
    entt::entity e_ = entt::null;
};

namespace
{
inline void delete_entity_hierarchy(Scene& sc, entt::entity entity)
{
    if (sc.has_component<engine_children_component_t>(entity))
    {
        const auto children = sc.get_component<engine_children_component_t>(entity);
        for (int c = 0; c < ENGINE_MAX_CHILDREN; c++)
        {
            const auto child = static_cast<entt::entity>(children->child[c]);
            if (sc.is_valid_entity(child))
            {
                delete_entity_hierarchy(sc, child);
            }
        }
    }
    sc.destroy_entity(entity);

}
}


class CommandDestroyEntity : public ICommand
{
public:
    CommandDestroyEntity(Scene& sc, entt::entity e)
        : scene_(sc)
        , e_(e)
    {
        assert(e_ != entt::null);
    }

    void execute() override
    {
        delete_entity_hierarchy(scene_, e_);
    }

    void undo() override
    {
        const auto new_entt  = scene_.create_new_entity(e_);
        assert(new_entt == e_);
    }

private:
    Scene& scene_;
    const entt::entity e_;
};

class CommandSetPhysicsDebugDraw: public ICommand
{
public:
    CommandSetPhysicsDebugDraw(Scene& sc, bool flag)
        : scene_(sc)
        , flag_(flag)
    {
    }

    void execute() override
    {
        scene_.enable_physics_debug_draw(flag_);
    }

    void undo() override
    {
        scene_.enable_physics_debug_draw(!flag_);
    }

private:
    Scene& scene_;
    bool flag_;
};

}  // namespace engine
