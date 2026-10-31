#pragma once
#include "scene.h"
#include "editor_asset_browser.h"
#include "editor_save_point.h"

#include <algorithm>
#include <optional>
#include <vector>
#include <memory>
#include <string>

#include "components_internals/guizmo_component.h"
#include "components_internals/outline_component.h"

namespace engine
{

// An undo step. Commands refer to game objects with GameObjectRef (by persistent ID), never by handle, and get the scene
// when they run: they find their game objects again after Stop gave them new handles, or after undo brought a deleted
// one back. The editor drops the steps recorded while playing at Stop (CommandManager::end_play_session), so a step
// never writes play session values into the edit state, and all of them when a scene is released
// (CommandManager::clear), so a step never acts on another scene's game objects. A command whose game object isn't there (any more) changes
// nothing and logs it (log_missing).
class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void execute(Scene& sc) = 0;
    virtual void undo(Scene& sc) = 0;
    // Takes in `other`, the next step of the same continuous edit (a drag of a slider or the gizmo, typing in a
    // field), so the whole edit is one undo step, like Qt's QUndoCommand::mergeWith: true when this step now ends
    // where `other` does. `other` is executed but not kept. Only asked while the edit goes on (see
    // CommandManager::end_continuous_edit).
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
    // false for a step which changes nothing a scene file holds (the selection, debug draw): it doesn't make the scene
    // unsaved (SavePoint)
    virtual bool changes_scene() const
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
        // a step merged into the saved one would change the scene without leaving the save point
        if (!edit_ended_ && !undo_stack_.empty() && save_point_.can_merge() && undo_stack_.back()->merge_with(cmd.get()))
        {
            cmd->execute(sc);
            save_point_.on_merge(cmd->changes_scene());
            return;
        }
        cmd->execute(sc);
        if (!cmd->has_effect())
        {
            return;
        }
        save_point_.on_execute(cmd->changes_scene());
        undo_stack_.push_back(std::move(cmd));
        redo_stack_.clear();
        edit_ended_ = false;
        if (undo_stack_.size() > max_history_)
        {
            undo_stack_.erase(undo_stack_.begin());
            save_point_.on_drop_oldest();
        }
    }

    // No widget is being edited and the gizmo isn't dragged (the editor calls it every frame then): the next step
    // starts a new undo step instead of merging into the last one (ICommand::merge_with). A click is one step, a drag
    // from press to release is one step.
    void end_continuous_edit() { edit_ended_ = true; }

    void undo(Scene& sc)
    {
        edit_ended_ = true;
        if (undo_stack_.empty())
        {
            return;
        }
        auto cmd = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        cmd->undo(sc);
        redo_stack_.push_back(std::move(cmd));
        save_point_.on_undo();
    }

    void redo(Scene& sc)
    {
        edit_ended_ = true;
        if (redo_stack_.empty())
        {
            return;
        }
        auto cmd = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        cmd->execute(sc);
        undo_stack_.push_back(std::move(cmd));
        save_point_.on_redo();
    }

    // Whether the edit state has changes which aren't saved (see SavePoint); while playing, the edit state Stop gives
    // back: what a play session does is never saved.
    bool is_dirty() const { return edit_history_ ? edit_history_->save_point.is_dirty() : save_point_.is_dirty(); }
    // the scene was saved as it is now
    void mark_saved() { save_point_.mark_saved(); }
    // the scene changed without an undo step (an editor edit which has none yet): unsaved until the next save
    void mark_changed() { save_point_.mark_changed(); }

    // drops every step, the edit history kept for a play session too: when the scene they were recorded in was
    // released, they would find other game objects with the same persistent IDs in the next one. The scene is clean
    // then: it was just loaded (or is new).
    void clear()
    {
        undo_stack_.clear();
        redo_stack_.clear();
        edit_history_.reset();
        save_point_ = SavePoint();
    }

    // Play: the edit history is put aside and the play session records its own steps (Unreal's Play In Editor
    // transactions), so undo while playing never reaches into the edit state
    void begin_play_session()
    {
        edit_history_.emplace(History{ std::move(undo_stack_), std::move(redo_stack_), save_point_ });
        undo_stack_.clear();
        redo_stack_.clear();
        save_point_ = SavePoint();
    }

    // the steps recorded in this play session, keeping the edit history put aside (the scene they were recorded in was
    // released while playing)
    void drop_play_session_steps()
    {
        undo_stack_.clear();
        redo_stack_.clear();
        save_point_ = SavePoint();
    }

    // Stop: the play session's steps are dropped (they would write play session values into the edit state, which
    // Stop gave back) and the edit history comes back, its steps finding their game objects by persistent ID. A
    // session begun without begin_play_session (the one the application starts in, whose Stop keeps the scene as it
    // is) keeps its steps.
    void end_play_session()
    {
        if (!edit_history_)
        {
            return;
        }
        undo_stack_ = std::move(edit_history_->undo);
        redo_stack_ = std::move(edit_history_->redo);
        save_point_ = edit_history_->save_point;
        edit_history_.reset();
    }

private:
    struct History
    {
        std::vector<std::unique_ptr<ICommand>> undo;
        std::vector<std::unique_ptr<ICommand>> redo;
        SavePoint save_point;
    };

    std::vector<std::unique_ptr<ICommand>> undo_stack_;
    std::vector<std::unique_ptr<ICommand>> redo_stack_;
    // the edit history while a play session runs
    std::optional<History> edit_history_;
    // where the history (the edit history, or the play session's while playing) stands relative to the last save
    SavePoint save_point_;
    // the last step's edit is over: nothing merges into it
    bool edit_ended_ = true;
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

    // the same component of the same game object, changed again by the same edit: one step from before the edit to
    // after it
    bool merge_with(const ICommand* other) override
    {
        const auto* next = dynamic_cast<const CommandUpdateComponent<TComp>*>(other);
        if (!next || next->ref_ != ref_)
        {
            return false;
        }
        new_ = next->new_;
        return true;
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
    TComp new_;
};

// Like CommandUpdateComponent, for a component known only by its type description (game-defined components).
class CommandUpdateTypedComponent : public ICommand
{
public:
    CommandUpdateTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type, const void* new_value);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;
    // like CommandUpdateComponent's
    bool merge_with(const ICommand* other) override;

private:
    // nothing happens when the component was removed meanwhile
    void apply(Scene& sc, const ValueBuffer& value);

    const GameObjectRef ref_;
    const TypeInfo& type_;
    ValueBuffer prev_;
    ValueBuffer new_;
};

// Several steps as one undo step: executed in order, undone in reverse (e.g. a script set with the components it requires).
class CommandSequence : public ICommand
{
public:
    explicit CommandSequence(std::vector<std::unique_ptr<ICommand>> commands) : commands_(std::move(commands)) {}

    void execute(Scene& sc) override
    {
        for (auto& command : commands_)
        {
            command->execute(sc);
        }
    }

    void undo(Scene& sc) override
    {
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it)
        {
            (*it)->undo(sc);
        }
    }

    bool changes_scene() const override
    {
        return std::ranges::any_of(commands_, [](const auto& command) { return command->changes_scene(); });
    }

private:
    std::vector<std::unique_ptr<ICommand>> commands_;
};

// Changes a component known only by its type description and adds the components its new value requires which the game
// object doesn't have (engine::required_components: a script's config when the script component names it, like Unity's
// RequireComponent), with their defaults, in one undo step.
std::unique_ptr<ICommand> make_update_typed_component_command(Scene& sc, entt::entity e, const TypeInfo& type, const void* new_value);

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

// Adds or removes the selection markers (gizmo, outline) of a game object, entt::null for none. They are internal, so a
// game object loaded back (Stop, undo of a delete) has none.
void set_selection_markers(Scene& sc, entt::entity e, bool selected);

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
    // the selection isn't saved
    bool changes_scene() const override { return false; }

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

// An empty game object, with only a name; with `position`, also a transform there (created in the scene view)
class CommandAddEntity : public ICommand
{
public:
    CommandAddEntity() = default;
    explicit CommandAddEntity(const glm::vec3& position) : position_(position) {}

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    GameObjectRef ref_;
    std::optional<glm::vec3> position_;
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
    bool changes_scene() const override { return false; }

private:
    bool flag_;
};

}  // namespace engine
