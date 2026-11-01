#include "editor_scene_file_dialog.h"
#include "asset_store.h"
#include "scene.h"
#include "imgui/imgui.h"

#include <cstring>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
constexpr const char* POPUP_NAME = "Scene file";
// the templates File > New offers, a folder of ordinary scene files (relative to assets/)
constexpr std::string_view TEMPLATES_FOLDER = "scenes/templates";

// the folder (relative to assets/) of a scene file's asset ID, for the dialog to start in
std::string scenes_folder_of(const std::string& asset_id)
{
    const auto parent = std::filesystem::path(asset_id).parent_path().generic_string();
    return parent.empty() ? std::string("scenes") : "scenes/" + parent;
}

std::filesystem::path assets_root()
{
    return engine::AssetStore::get_instance().get_scenes_base_path().parent_path();
}
}  // namespace

void engine::SceneFileDialog::open(Kind kind, const Scene& scene)
{
    kind_ = kind;
    open_popup_ = true;
    picked_.clear();
    name_.fill('\0');
    // files may have changed since the dialog was last open
    listed_folder_.reset();
    checked_path_.reset();
    const auto& path = scene.get_file_path();
    const bool in_scenes = !path.empty() && !std::filesystem::path(path).is_absolute();
    if (kind == Kind::eNew)
    {
        folder_ = TEMPLATES_FOLDER;
    }
    else if (in_scenes)
    {
        // where the scene's file is, with its name to save over or change
        folder_ = scenes_folder_of(path);
        if (kind == Kind::eSaveAs)
        {
            const auto name = std::filesystem::path(path).filename().string();
            std::strncpy(name_.data(), name.c_str(), name_.size() - 1);
        }
    }
    else if (folder_ == TEMPLATES_FOLDER || !std::filesystem::is_directory(assets_root() / folder_))
    {
        folder_ = "scenes";
    }
}

bool engine::SceneFileDialog::is_open() const
{
    return ImGui::IsPopupOpen(POPUP_NAME);
}

std::optional<engine::SceneFileDialog::result_t> engine::SceneFileDialog::render(const Scene& scene, bool in_edit)
{
    if (std::exchange(open_popup_, false))
    {
        ImGui::OpenPopup(POPUP_NAME);
    }
    ImGui::SetNextWindowSize(ImVec2(520.0f, 420.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(POPUP_NAME))
    {
        return std::nullopt;
    }
    const auto kind = kind_;
    const auto close = [this, kind](std::optional<std::string> path)
        {
            kind_ = Kind::eNone;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return result_t{ kind, std::move(path) };
        };
    // a dialog closed while playing (Play pressed in its frame) has nothing to do
    if (kind == Kind::eNone || !in_edit)
    {
        return close(std::nullopt);
    }
    ImGui::TextUnformatted(kind == Kind::eSaveAs ? "Save the scene as"
        : kind == Kind::eOpen ? "Open a scene"
        : "New scene from a template");
    ImGui::TextDisabled("assets/%s", folder_.c_str());

    // the folders and scene files of the folder; a template is picked in its folder only
    std::optional<std::string> confirmed;
    const float footer = ImGui::GetFrameHeightWithSpacing() * (kind == Kind::eSaveAs ? 3.5f : 2.0f);
    if (ImGui::BeginChild("##files", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders))
    {
        if (kind != Kind::eNew && folder_ != "scenes" && ImGui::Selectable("..", false, ImGuiSelectableFlags_AllowDoubleClick)
            && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            folder_ = std::filesystem::path(folder_).parent_path().generic_string();
        }
        if (listed_folder_ != folder_)
        {
            entries_ = list_asset_folder(assets_root(), folder_);
            listed_folder_ = folder_;
        }
        for (const auto& entry : entries_)
        {
            if (entry.kind == AssetFileKind::eFolder)
            {
                if (kind == Kind::eNew)
                {
                    continue;
                }
                const auto label = entry.name + "/";
                if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    folder_ = entry.path;
                }
                continue;
            }
            const auto asset_id = get_asset_id(entry);
            if (entry.kind != AssetFileKind::eScene || !asset_id)
            {
                continue;
            }
            const bool picked = picked_ == *asset_id;
            if (ImGui::Selectable(entry.name.c_str(), picked, ImGuiSelectableFlags_AllowDoubleClick))
            {
                picked_ = *asset_id;
                if (kind == Kind::eSaveAs)
                {
                    name_.fill('\0');
                    std::strncpy(name_.data(), entry.name.c_str(), name_.size() - 1);
                }
                else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    confirmed = *asset_id;
                }
            }
        }
    }
    ImGui::EndChild();

    bool can_confirm = false;
    std::optional<std::string> save_path;
    if (kind == Kind::eSaveAs)
    {
        ImGui::InputText("Name", name_.data(), name_.size());
        save_path = get_scene_save_path(folder_, name_.data());
        if (save_path && save_path != checked_path_)
        {
            std::error_code error;
            path_exists_ = std::filesystem::exists(AssetStore::get_instance().get_scenes_base_path() / *save_path, error);
            path_placed_ = scene.prefabs().places_scene_file(*save_path);
            checked_path_ = save_path;
        }
        if (!save_path)
        {
            ImGui::TextDisabled("A file name, saved as .json in this folder.");
        }
        // a file the scene places (directly or not) would be placed inside itself once saved: the guard of
        // engineSceneLoad can't be bypassed by saving over it
        else if (path_placed_)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "The scene places %s: it can't be saved into it.", save_path->c_str());
            save_path.reset();
        }
        else
        {
            if (path_exists_ && *save_path != scene.get_file_path())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s exists: saving replaces it.", save_path->c_str());
            }
            else
            {
                ImGui::TextDisabled("assets/scenes/%s", save_path->c_str());
            }
        }
        can_confirm = save_path.has_value();
    }
    else
    {
        can_confirm = !picked_.empty();
    }

    ImGui::BeginDisabled(!can_confirm);
    const char* confirm_label = kind == Kind::eSaveAs ? "Save" : kind == Kind::eOpen ? "Open" : "Create";
    const bool confirm_pressed = ImGui::Button(confirm_label) || (can_confirm && ImGui::IsKeyPressed(ImGuiKey_Enter));
    ImGui::EndDisabled();
    if (confirm_pressed && can_confirm)
    {
        confirmed = kind == Kind::eSaveAs ? *save_path : picked_;
    }
    ImGui::SameLine();
    const bool cancelled = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (confirmed)
    {
        return close(std::move(confirmed));
    }
    if (cancelled)
    {
        return close(std::nullopt);
    }
    ImGui::EndPopup();
    return std::nullopt;
}
