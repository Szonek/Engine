#pragma once
#include "editor_asset_browser.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace engine
{
class Scene;

// The File menu's dialog, a modal ImGui popup limited to assets/scenes, like Unreal's (so a scene always has an asset
// ID): Save As (a folder and a file name, checked by get_scene_save_path; a file the scene places, directly or not, is
// refused), Open (a scene file) and New (a template in assets/scenes/templates). It picks a path; what is done with it
// is the editor's (SceneFileFlow).
class SceneFileDialog
{
public:
    enum class Kind
    {
        eNone,
        eSaveAs,
        eOpen,
        eNew,
    };
    struct result_t
    {
        Kind kind = Kind::eNone;
        // the confirmed asset ID (the file to save to, the file to open, the template); none: cancelled
        std::optional<std::string> path;
    };

    // opens at the next render, in the folder of the scene's file (Save As with its name), New in the templates' folder
    void open(Kind kind, const Scene& scene);
    // the popup is open (between open's render and the one it closed in)
    bool is_open() const;
    // draws the dialog while it is open; in the frame it closes, how it ended. Outside Edit mode (Play pressed while it
    // was open) it closes cancelled.
    std::optional<result_t> render(const Scene& scene, bool in_edit);

private:
    Kind kind_ = Kind::eNone;
    bool open_popup_ = false;
    // the folder (relative to assets/, "scenes" or under it), name typed (Save As) and file picked (Open, New)
    std::string folder_ = "scenes";
    std::array<char, 128> name_{};
    std::string picked_;
    // read from disk when the folder or the save path changes, not every frame: the folder's entries as listed, and
    // whether the checked save path exists and is a file the scene places
    std::optional<std::string> listed_folder_;
    std::vector<asset_entry_t> entries_;
    std::optional<std::string> checked_path_;
    bool path_exists_ = false;
    bool path_placed_ = false;
};
}  // namespace engine
