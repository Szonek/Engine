#pragma once
#include <optional>
#include <string>
#include <utility>

namespace engine
{
// The File menu's commands, like Unity's: Save writes the scene to its file (Save As when it has none), Open replaces
// the scene with a file, New with a template (a scene without a file); a quit asks first too.
enum class SceneFileAction
{
    eNone,
    eSave,
    eOpen,
    eNew,
    eQuit,
};

struct scene_file_request_t
{
    SceneFileAction action = SceneFileAction::eNone;
    // eSave: the file, eOpen: the scene file, eNew: the template (asset IDs: paths under assets/scenes)
    std::string path;
};

// What the File menu's commands wait for and in which order: Open, New and quit with unsaved changes wait for the
// unsaved changes prompt (Save / Don't save / Cancel); its Save waits for the file to be written, or first for Save As
// when the scene has no file; Don't save goes on at once, Cancel drops what waited, and so does a save which failed or
// a Save As which was cancelled. Saving, opening and creating are done at the start of a frame (take_request), never
// during a scene update. GL-free and ImGui-free: the editor shows the prompt and the Save As dialog when it is told to.
class SceneFileFlow
{
public:
    // Save (`path`: the file; empty: the scene's file), Open (the file) or New (the template). `scene_file` is the
    // scene's file ("" for a new scene) and `dirty` whether it has unsaved changes. True: Save As has to be shown first
    // (a save of a scene without a file), and its confirmed path is requested as a Save.
    [[nodiscard]] bool request(SceneFileAction action, std::string path, const std::string& scene_file, bool dirty)
    {
        switch (action)
        {
        case SceneFileAction::eSave:
            if (path.empty())
            {
                path = scene_file;
            }
            if (path.empty())
            {
                return true;
            }
            request_ = { action, std::move(path) };
            return false;
        case SceneFileAction::eOpen:
        case SceneFileAction::eNew:
            if (dirty)
            {
                wait_for_prompt({ action, std::move(path) });
                return false;
            }
            request_ = { action, std::move(path) };
            return false;
        case SceneFileAction::eQuit:
        case SceneFileAction::eNone:
            break;
        }
        return false;
    }

    // the window is closing: true when it may close now, false when the quit waits for the prompt
    [[nodiscard]] bool request_quit(bool dirty)
    {
        if (!dirty)
        {
            return true;
        }
        wait_for_prompt({ SceneFileAction::eQuit, {} });
        return false;
    }

    // true once after a command started waiting: the prompt opens
    bool take_prompt_open() { return std::exchange(prompt_open_, false); }
    // what waits for the prompt (or for the save its Save started), eNone when nothing does
    SceneFileAction get_waiting() const { return waiting_.action; }

    // the prompt's Save: true when Save As has to be shown first (the scene has no file)
    [[nodiscard]] bool prompt_save(const std::string& scene_file)
    {
        if (scene_file.empty())
        {
            return true;
        }
        request_ = { SceneFileAction::eSave, scene_file };
        return false;
    }
    // the prompt's Don't save: what waited goes on. True: it was a quit, which may go on now.
    [[nodiscard]] bool prompt_discard() { return continue_waiting(); }
    // the prompt's Cancel, a Save As or save which didn't happen, or a play session started: nothing waits any more
    void cancel_waiting() { waiting_ = {}; }

    // a Save, Open or New is due at the start of the frame
    bool has_request() const { return request_.action != SceneFileAction::eNone; }
    scene_file_request_t take_request() { return std::exchange(request_, {}); }
    // the requested Save was written: what waited for it goes on. True: it was a quit, which may go on now.
    [[nodiscard]] bool on_saved() { return continue_waiting(); }

private:
    void wait_for_prompt(scene_file_request_t waiting)
    {
        waiting_ = std::move(waiting);
        prompt_open_ = true;
    }

    bool continue_waiting()
    {
        auto next = std::exchange(waiting_, {});
        if (next.action == SceneFileAction::eQuit)
        {
            return true;
        }
        if (next.action != SceneFileAction::eNone)
        {
            request_ = std::move(next);
        }
        return false;
    }

    // done at the start of the next frame
    scene_file_request_t request_;
    // what waits for the prompt, or for the save the prompt's Save asked for
    scene_file_request_t waiting_;
    bool prompt_open_ = false;
};
}  // namespace engine
