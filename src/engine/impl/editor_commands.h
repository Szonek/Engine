#pragma once
#include "scene.h"

#include <vector>
#include <memory>

#include "components_internals/guizmo_component.h"
#include "components_internals/outline_component.h"

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

template<typename TComp>
class CommandUpdateComponent : public ICommand
{
public:
    CommandUpdateComponent(Scene& sc, entt::entity e, TComp& new_comp)
        : sc_(sc)
        , e_(e)
        , new_(new_comp)
        , prev_(*sc.get_component<TComp>(e_))
    {
    }

    void execute() override
    {
        sc_.update_component<TComp>(e_, new_);
    }

    void undo() override
    {
        sc_.update_component<TComp>(e_, prev_);
    }

private:
    Scene& sc_;
    entt::entity e_;
    const TComp prev_;
    const TComp new_;
};

class CommandSetSelectedEntity : public ICommand
{
public:
    CommandSetSelectedEntity(Scene& sc, entt::entity prev_selected, entt::entity new_selected);

    void execute() override;
    void undo() override;

private:
    Scene& sc_;
    entt::entity e_prev_;
    entt::entity e_new_;
};

class CommandRenameEntity : public ICommand
{
public:
    CommandRenameEntity(Scene& sc, entt::entity e, std::string_view new_name);

    void execute() override;
    void undo() override;

private:
    Scene& scene_;
    const entt::entity e_ = entt::null;
    std::string prev_name_;
    std::string new_name_;
};

class CommandAddEntity : public ICommand
{
public:
    CommandAddEntity(Scene& sc);

    void execute() override;
    void undo() override;

private:
    Scene& scene_;
    entt::entity e_ = entt::null;
};

class CommandDestroyEntity : public ICommand
{
public:
    CommandDestroyEntity(Scene& sc, entt::entity e);

    void execute() override;
    void undo() override;

private:
    Scene& scene_;
    const entt::entity e_;
};

class CommandSetPhysicsDebugDraw: public ICommand
{
public:
    CommandSetPhysicsDebugDraw(Scene& sc, bool flag);

    void execute() override;
    void undo() override;

private:
    Scene& scene_;
    bool flag_;
};

}  // namespace engine

