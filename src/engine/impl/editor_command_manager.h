#pragma once
#include "editor_save_point.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine
{
class Scene;

// An undo step. Commands refer to game objects with GameObjectRef (by persistent ID), never by handle, and get the scene
// when they run: they find their game objects again after Stop gave them new handles, or after undo brought a deleted
// one back. The editor drops the steps recorded while playing at Stop (CommandManager::end_play_session), so a step
// never writes play session values into the edit state, and all of them when a scene is released (CommandManager::clear),
// so a step never acts on another scene's game objects; the steps changing an asset stay (see changes_asset). A command whose
// game object isn't there (any more) changes nothing and logs it (log_missing). GL-free (the scene is only passed on),
// so the history is unit tested.
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
    // the asset ID of the data asset the step changes, empty for a step of the scene. A data asset is a file, not
    // scene content, so its steps are kept at Stop (like Unity's ScriptableObject edits) and when the scene is
    // released, and dropped when its file is read again (CommandManager::drop_data_asset_steps).
    virtual std::string_view get_data_asset() const
    {
        return {};
    }
    // the name of the terrain the step changes, empty for any other step: its steps are dropped when its heights are
    // replaced (CommandManager::drop_terrain_steps)
    virtual std::string_view get_terrain() const
    {
        return {};
    }
    // Whether the step changes an asset (a file) instead of the scene: a data asset, a terrain. Such a step is kept at
    // Stop and when the scene is released, like the asset's changes are.
    virtual bool changes_asset() const
    {
        return !get_data_asset().empty() || !get_terrain().empty();
    }
};

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
        push(std::move(cmd));
    }

    // No widget is being edited and the gizmo isn't dragged (the editor calls it every frame then): the next step
    // starts a new undo step instead of merging into the last one (ICommand::merge_with). A click is one step, a drag
    // from press to release is one step.
    void end_continuous_edit() { edit_ended_ = true; }

    // whether there is a step to undo
    bool can_undo() const { return !undo_stack_.empty(); }

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

    // drops every step of the scene, the edit history kept for a play session too: when the scene they were recorded
    // in was released, they would find other game objects with the same persistent IDs in the next one. The scene is
    // clean then: it was just loaded (or is new). The done data asset steps stay, the edit history's first.
    void clear()
    {
        std::vector<std::unique_ptr<ICommand>> kept;
        if (edit_history_)
        {
            take_data_asset_steps(edit_history_->undo, kept);
        }
        take_data_asset_steps(undo_stack_, kept);
        undo_stack_.clear();
        redo_stack_.clear();
        edit_history_.reset();
        save_point_ = SavePoint();
        add_kept_steps(std::move(kept));
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
    // released while playing) and the session's done data asset steps
    void drop_play_session_steps()
    {
        std::vector<std::unique_ptr<ICommand>> kept;
        take_data_asset_steps(undo_stack_, kept);
        undo_stack_.clear();
        redo_stack_.clear();
        save_point_ = SavePoint();
        add_kept_steps(std::move(kept));
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
        // the data asset edits stay (they are files, not the scene Stop gives back), and so do their done steps,
        // after the edit history's done ones (its undone ones can still be redone)
        std::vector<std::unique_ptr<ICommand>> kept;
        take_data_asset_steps(undo_stack_, kept);
        undo_stack_ = std::move(edit_history_->undo);
        redo_stack_ = std::move(edit_history_->redo);
        save_point_ = edit_history_->save_point;
        edit_history_.reset();
        add_kept_steps(std::move(kept));
    }

    // drops the steps changing the data asset (ICommand::get_data_asset), the edit history's too: its file was read
    // again, and an undo would bring back an older value over the file's change
    void drop_data_asset_steps(std::string_view asset_id)
    {
        drop_steps([asset_id](const std::unique_ptr<ICommand>& cmd) { return cmd->get_data_asset() == asset_id; });
    }

    // drops the steps changing the terrain (ICommand::get_terrain), the edit history's too: its heights were replaced
    // (a heightmap imported again), and an undo would write heights from before over the new ones
    void drop_terrain_steps(std::string_view terrain)
    {
        drop_steps([terrain](const std::unique_ptr<ICommand>& cmd) { return cmd->get_terrain() == terrain; });
    }

private:
    // a new step, done
    void push(std::unique_ptr<ICommand> cmd)
    {
        save_point_.on_execute(cmd->changes_scene());
        undo_stack_.push_back(std::move(cmd));
        redo_stack_.clear();
        edit_ended_ = false;
        drop_oldest_over_limit();
    }

    void drop_oldest_over_limit()
    {
        while (undo_stack_.size() > max_history_)
        {
            undo_stack_.erase(undo_stack_.begin());
            save_point_.on_drop_oldest();
        }
    }

    // moves the data asset steps of `from` to the end of `into`, in their order
    static void take_data_asset_steps(std::vector<std::unique_ptr<ICommand>>& from, std::vector<std::unique_ptr<ICommand>>& into)
    {
        for (auto& cmd : from)
        {
            if (cmd->changes_asset())
            {
                into.push_back(std::move(cmd));
            }
        }
        std::erase(from, nullptr);
    }

    // data asset steps done elsewhere (a play session, a released scene) become done steps of this history, after its
    // done ones and before its undone ones: they change no scene, so the save point and redo stay as they were
    void add_kept_steps(std::vector<std::unique_ptr<ICommand>> kept)
    {
        for (auto& cmd : kept)
        {
            undo_stack_.push_back(std::move(cmd));
            save_point_.on_insert_done();
        }
        drop_oldest_over_limit();
        edit_ended_ = true;
    }

    // drops the steps `changes` is true for, the edit history's too
    template<typename Changes>
    void drop_steps(const Changes& changes)
    {
        drop_steps(undo_stack_, redo_stack_, save_point_, changes);
        if (edit_history_)
        {
            drop_steps(edit_history_->undo, edit_history_->redo, edit_history_->save_point, changes);
        }
        edit_ended_ = true;
    }

    template<typename Changes>
    static void drop_steps(std::vector<std::unique_ptr<ICommand>>& undo, std::vector<std::unique_ptr<ICommand>>& redo, SavePoint& save_point,
        const Changes& changes)
    {
        // from the newest step: the undone ones (the redo stack's first is the newest), then the done ones
        for (std::size_t i = 0; i < redo.size(); i++)
        {
            if (changes(redo[i]))
            {
                save_point.on_erase(undo.size() + redo.size() - 1 - i);
            }
        }
        for (std::size_t i = undo.size(); i-- > 0;)
        {
            if (changes(undo[i]))
            {
                save_point.on_erase(i);
            }
        }
        std::erase_if(undo, changes);
        std::erase_if(redo, changes);
    }

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

// Gives the editor's undo steps the data assets: GL-free, so the steps can be tested without an application.
class IDataAssetValues
{
public:
    virtual ~IDataAssetValues() = default;
    // Gives the data asset (loaded when it isn't any more) the value, as its file would hold it (DataAsset::set_value_text);
    // false (logged) when it can't be loaded or the value can't be used.
    virtual bool set_data_asset_value_text(std::string_view asset_id, std::string_view text) = 0;
};

// Changes a data asset's value, like an asset edited in Unity's inspector: one undo step on the editor's history,
// which changes no scene (the asset is dirty until it is saved, DataAsset::is_dirty). The values are kept as the
// file's text, asset references by asset ID, so undo finds them whatever was released meanwhile.
class CommandUpdateDataAsset : public ICommand
{
public:
    CommandUpdateDataAsset(IDataAssetValues& assets, std::string_view asset_id, std::string prev_text, std::string new_text)
        : assets_(assets)
        , asset_id_(asset_id)
        , prev_(std::move(prev_text))
        , new_(std::move(new_text))
    {
    }

    void execute(Scene& /*sc*/) override { set_ = assets_.set_data_asset_value_text(asset_id_, new_); }
    void undo(Scene& /*sc*/) override { assets_.set_data_asset_value_text(asset_id_, prev_); }
    // a value which couldn't be set (logged) leaves no undo step
    bool has_effect() const override { return set_; }
    // the same data asset, changed again by the same edit
    bool merge_with(const ICommand* other) override
    {
        const auto* next = dynamic_cast<const CommandUpdateDataAsset*>(other);
        if (!next || next->asset_id_ != asset_id_)
        {
            return false;
        }
        new_ = next->new_;
        return true;
    }
    bool changes_scene() const override { return false; }
    std::string_view get_data_asset() const override { return asset_id_; }

private:
    IDataAssetValues& assets_;
    const std::string asset_id_;
    const std::string prev_;
    std::string new_;
    bool set_ = false;
};
}  // namespace engine
