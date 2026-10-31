#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace engine
{
// Whether the scene has unsaved changes, from where its undo history stands relative to the step it was saved at, like
// Qt's QUndoStack::cleanIndex: undoing back to the save point makes the scene clean again, undoing past it makes it
// unsaved. Only steps which change the scene count (ICommand::changes_scene), so a selection change after Save keeps it
// clean. When the save point can't be reached any more (it fell off the history, or new steps replaced the undone ones
// it was in) the scene stays unsaved until the next save (Qt's clean index of -1), and so does a change made without
// an undo step. GL-free: CommandManager tells it what happened to its history.
class SavePoint
{
public:
    // the history is empty and the scene as it was loaded (or new): clean
    SavePoint() = default;

    bool is_dirty() const
    {
        if (!saved_)
        {
            return true;
        }
        const auto [first, last] = std::minmax(*saved_, position_);
        return std::any_of(steps_.begin() + static_cast<std::ptrdiff_t>(first), steps_.begin() + static_cast<std::ptrdiff_t>(last), [](bool changes) { return changes; });
    }

    // the scene was saved as it is now
    void mark_saved() { saved_ = position_; }
    // the scene changed without an undo step: no undo brings it back to the saved state
    void mark_changed() { saved_.reset(); }

    // a new step was done: the undone ones (the redo stack) are gone
    void on_execute(bool changes_scene)
    {
        steps_.resize(position_);
        if (saved_ && *saved_ > position_)
        {
            saved_.reset();
        }
        steps_.push_back(changes_scene);
        position_++;
    }
    // A step may merge into the last one (a drag of the same value) only when that one isn't the saved state: the
    // merged step would change the scene past the save point without moving away from it.
    bool can_merge() const { return !saved_ || *saved_ != position_; }
    // a step was merged into the last one
    void on_merge(bool changes_scene)
    {
        if (position_ > 0 && changes_scene)
        {
            steps_[position_ - 1] = true;
        }
    }
    void on_undo()
    {
        if (position_ > 0)
        {
            position_--;
        }
    }
    void on_redo()
    {
        if (position_ < steps_.size())
        {
            position_++;
        }
    }
    // the oldest step fell off the history
    void on_drop_oldest()
    {
        if (steps_.empty())
        {
            return;
        }
        steps_.erase(steps_.begin());
        if (position_ > 0)
        {
            position_--;
        }
        if (saved_)
        {
            if (*saved_ == 0)
            {
                saved_.reset();
            }
            else
            {
                (*saved_)--;
            }
        }
    }

private:
    // whether each step of the history changes the scene, the oldest first; the ones from position_ on are undone
    std::vector<bool> steps_;
    // the number of steps done (not undone)
    std::size_t position_ = 0;
    // position_ when the scene was saved, nullopt when that state can't be reached by undo / redo any more
    std::optional<std::size_t> saved_ = 0;
};
}  // namespace engine
