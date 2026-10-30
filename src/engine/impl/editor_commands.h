#pragma once
#include "scene.h"
#include "editor_asset_browser.h"

#include <vector>
#include <memory>
#include <string>

#include "components_internals/guizmo_component.h"
#include "components_internals/outline_component.h"

namespace engine
{

// An undo step. Commands refer to game objects with GameObjectRef (by persistent ID), never by handle, and get the scene
// when they run: they find their game objects again after Stop loaded the scene saved at Play back with new handles
// (also into another scene object, when the game replaced its scene while playing). A command whose game object isn't
// there (any more) changes nothing and logs it (log_missing).
class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void execute(Scene& sc) = 0;
    virtual void undo(Scene& sc) = 0;
    virtual bool merge_with(const ICommand* /*other*/)
    {
        return false;
    }
    // false when the first execute() changed nothing (e.g. a copy which couldn't be made): the command isn't kept, so
    // undo doesn't take an empty step and redo doesn't try it again
    virtual bool has_effect() const
    {
        return true;
    }
};

// logs that the game object an undo step changes isn't in the scene (any more)
void log_missing(std::string_view command);

class CommandManager
{
public:
    void execute_command(Scene& sc, std::unique_ptr<ICommand> cmd)
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
        cmd->execute(sc);
        if (!cmd->has_effect())
        {
            return;
        }
        undo_stack_.push_back(std::move(cmd));
        redo_stack_.clear();
        if (undo_stack_.size() > max_history_)
        {
            undo_stack_.erase(undo_stack_.begin());
        }
    }

    void undo(Scene& sc)
    {
        if (undo_stack_.empty())
        {
            return;
        }
        auto cmd = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        cmd->undo(sc);
        redo_stack_.push_back(std::move(cmd));
    }

    void redo(Scene& sc)
    {
        if (redo_stack_.empty())
        {
            return;
        }
        auto cmd = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        cmd->execute(sc);
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
        : ref_(sc.make_ref(e))
        , prev_(*sc.get_component<TComp>(e))
        , new_(new_comp)
    {
    }

    void execute(Scene& sc) override
    {
        apply(sc, new_);
    }

    void undo(Scene& sc) override
    {
        apply(sc, prev_);
    }

private:
    void apply(Scene& sc, const TComp& value)
    {
        const auto e = sc.resolve(ref_);
        if (e == entt::null || !sc.has_component<TComp>(e))
        {
            log_missing("component update");
            return;
        }
        sc.update_component<TComp>(e, value);
    }

    const GameObjectRef ref_;
    const TComp prev_;
    const TComp new_;
};

// Like CommandUpdateComponent, for a component known only by its type description (game-defined components).
class CommandUpdateTypedComponent : public ICommand
{
public:
    CommandUpdateTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type, const void* new_value);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    // nothing happens when the component was removed meanwhile
    void apply(Scene& sc, const ValueBuffer& value);

    const GameObjectRef ref_;
    const TypeInfo& type_;
    ValueBuffer prev_;
    ValueBuffer new_;
};

// Adds a component known only by its type description, with its default value; undo removes it.
class CommandAddTypedComponent : public ICommand
{
public:
    CommandAddTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    const GameObjectRef ref_;
    const TypeInfo& type_;
};

// Removes a component known only by its type description; undo adds it back with the value it had.
class CommandRemoveTypedComponent : public ICommand
{
public:
    CommandRemoveTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    const GameObjectRef ref_;
    const TypeInfo& type_;
    ValueBuffer value_;
};

// Changes the selection, one undo step like in Blender and Unity: the game object (entt::null for none) and, given the
// asset browser's selection, the file, so undo and redo bring back a file selected in place of a game object too.
class CommandSetSelection : public ICommand
{
public:
    // selects the game object, leaving the selected file alone (none is selected while a game object is)
    CommandSetSelection(Scene& sc, entt::entity prev_selected, entt::entity new_selected);
    // selects the game object and the file (an empty path for none); the file selected before is the one selected now
    CommandSetSelection(Scene& sc, IAssetSelection& assets, entt::entity prev_selected, entt::entity new_selected, std::string_view new_asset);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    void select(Scene& sc, const GameObjectRef& deselected, const GameObjectRef& selected, const std::string& asset);

    const GameObjectRef prev_;
    const GameObjectRef new_;
    IAssetSelection* assets_ = nullptr;
    std::string asset_prev_;
    std::string asset_new_;
};

class CommandRenameEntity : public ICommand
{
public:
    CommandRenameEntity(Scene& sc, entt::entity e, std::string_view new_name);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    void rename(Scene& sc, const std::string& name);

    const GameObjectRef ref_;
    std::string prev_name_;
    std::string new_name_;
};

class CommandAddEntity : public ICommand
{
public:
    CommandAddEntity() = default;

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    GameObjectRef ref_;
};

// Destroys the game object and everything parented under it. They are saved first and loaded back on undo, with
// their persistent IDs and handles (so references to them find them again) and at the same place among their parent's
// children. Transient game objects aren't saved, so they don't come back, and a game object removed from what a prefab
// created on its instance can't be saved yet, so it comes back (logged); a game object of a prefab instance (but its
// root) isn't deleted (see can_destroy).
class CommandDestroyEntity : public ICommand
{
public:
    CommandDestroyEntity(Scene& sc, entt::entity e);

    // false for a game object its prefab instance created: a game object removed from an instance can't be saved yet,
    // and undo couldn't bring back what the prefab gave it which scene files don't describe yet (skin, joint attachment);
    // also false when one is parented under `e` without its instance's root (moved out of its instance by code: the
    // editor doesn't move them), which would come back apart from its instance
    static bool can_destroy(const Scene& sc, entt::entity e);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    const GameObjectRef ref_;
    std::string saved_;
    SceneLoadOptions restore_;
    GameObjectRef parent_;
    std::size_t child_index_ = 0;
};

// Copies the game object and everything parented under it, like Duplicate in Unity, Godot and Unreal: the copy gets
// new persistent IDs, the same parent, the place right after the original among its parent's children and the name
// "<name> (1)" (the first number no sibling uses), and becomes the selection. References between the copied game
// objects point at the copies, references to the rest of the scene stay. A prefab instance is copied as an instance
// of its prefab with what was added to it, changed in it (prefab overrides) and removed from it (a game object removed
// from what the prefab created can't be saved yet, so the copy has it, logged). Undo destroys the copy (and gives the selection back), redo brings it back with the same IDs and
// handles; one undo step.
class CommandDuplicateEntity : public ICommand
{
public:
    // `selected`: the selection before the copy, given back on undo
    CommandDuplicateEntity(Scene& sc, entt::entity e, entt::entity selected);

    // false for a transient game object (runtime objects aren't scene content) and a game object its prefab instance
    // created (a copy out of an instance would need what scene files don't describe yet: skin, joint attachment), or
    // which holds one without its instance's root (see can_destroy); duplicating it anyway logs why and copies nothing
    static bool can_duplicate(const Scene& sc, entt::entity e);

    // the copy in `sc`, entt::null when nothing was copied
    entt::entity get_copy(const Scene& sc) const { return sc.resolve(copy_); }

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;
    // a copy which couldn't be made leaves no undo step
    bool has_effect() const override { return !copy_.is_none(); }

private:
    void duplicate(Scene& sc);

    const GameObjectRef ref_;
    const GameObjectRef selected_;
    GameObjectRef copy_;
    // undo destroys the copy the way a delete does, redo is that delete's undo
    std::unique_ptr<CommandDestroyEntity> destroy_copy_;
    std::unique_ptr<CommandSetSelection> select_copy_;
};

// Places a prefab (a model or a scene file, by its asset ID) as a persistent instance, like dragging an asset into
// Unity's or Godot's scene view: its root is moved to `position` (keeping the rotation and scale the prefab gave it),
// gets a name no other game object without a parent has ("Rogue (1)" when there is a "Rogue", like a duplicate) and becomes
// the selection (deselecting the file selected in the asset browser). The scene holds the prefab (SceneAssets) as long as
// it lives. Undo destroys the instance (and gives the selection back, a file too), redo brings it back with the same IDs
// and handles; one undo step.
class CommandPlacePrefab : public ICommand
{
public:
    // `selected`: the game object selected before the placement, given back on undo with the file selected in `assets`
    CommandPlacePrefab(Scene& sc, IAssetSelection& assets, std::string_view prefab_asset_id, const glm::vec3& position, entt::entity selected);

    // the instance's root in `sc`, entt::null when nothing was placed
    entt::entity get_instance(const Scene& sc) const { return sc.resolve(root_); }

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;
    // a prefab which couldn't be loaded or instantiated leaves no undo step
    bool has_effect() const override { return !root_.is_none(); }

private:
    void place(Scene& sc);

    IAssetSelection& assets_;
    const std::string asset_id_;
    const glm::vec3 position_;
    const GameObjectRef selected_;
    GameObjectRef root_;
    // undo destroys the instance the way a delete does, redo is that delete's undo
    std::unique_ptr<CommandDestroyEntity> destroy_instance_;
    std::unique_ptr<CommandSetSelection> select_instance_;
};

class CommandSetPhysicsDebugDraw: public ICommand
{
public:
    explicit CommandSetPhysicsDebugDraw(bool flag);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    bool flag_;
};

}  // namespace engine
