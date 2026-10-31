// The editor's File menu: Save, Save As, Open and New of the scene, its dialogs and the unsaved changes prompt.
#include "application_editor.h"
#include "asset_store.h"
#include "editor_asset_browser.h"
#include "logger.h"
#include "scene.h"
#include "imgui/imgui.h"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
constexpr const char* SCENE_FILE_DIALOG = "Scene file";
constexpr const char* UNSAVED_PROMPT = "Unsaved changes";
constexpr const char* STOP_TO_SAVE = "Stop to save: nothing of a play session is saved";
// the templates File > New offers, a folder of ordinary scene files (relative to assets/)
constexpr std::string_view TEMPLATES_FOLDER = "scenes/templates";

// the scene's file as the toolbar and the title show it
std::string scene_display_name(const engine::Scene& scene)
{
    return scene.get_file_path().empty() ? std::string("Untitled") : scene.get_file_path();
}

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

bool engine::ApplicationEditor::on_quit_requested()
{
    if (!commands_.is_dirty())
    {
        return true;
    }
    after_prompt_ = { SceneFileAction::eQuit, {} };
    open_unsaved_prompt_ = true;
    return false;
}

void engine::ApplicationEditor::request_scene_file_action(Scene* scene, SceneFileAction action, std::string path)
{
    if (get_play_mode_state() != ENGINE_PLAY_MODE_EDIT)
    {
        log::log(log::LogLevel::eError, "Scene files: stop the play session first (nothing of it is saved).\n");
        return;
    }
    switch (action)
    {
    case SceneFileAction::eSave:
        if (path.empty())
        {
            path = scene->get_file_path();
        }
        // a new scene has no file yet
        if (path.empty())
        {
            open_scene_file_dialog(SceneFileDialog::eSaveAs, scene);
            return;
        }
        scene_file_request_ = { action, std::move(path) };
        return;
    case SceneFileAction::eOpen:
    case SceneFileAction::eNew:
        if (commands_.is_dirty())
        {
            after_prompt_ = { action, std::move(path) };
            open_unsaved_prompt_ = true;
            return;
        }
        scene_file_request_ = { action, std::move(path) };
        return;
    case SceneFileAction::eQuit:
    case SceneFileAction::eNone:
        return;
    }
}

void engine::ApplicationEditor::continue_after_prompt()
{
    const auto next = std::exchange(after_prompt_, {});
    if (next.action == SceneFileAction::eQuit)
    {
        confirm_quit();
    }
    else if (next.action != SceneFileAction::eNone)
    {
        scene_file_request_ = next;
    }
}

void engine::ApplicationEditor::forget_game_objects()
{
    modal_transform_.reset();
    modal_transform_entity_ = entt::null;
    gizmo_hovered_entity_ = entt::null;
    gizmo_hovered_handle_.reset();
    delete_without_undo_ = entt::null;
}

void engine::ApplicationEditor::apply_scene_file_request()
{
    if (scene_file_request_.action == SceneFileAction::eNone || !play_mode_scene_)
    {
        return;
    }
    const auto request = std::exchange(scene_file_request_, {});
    auto& scene = *play_mode_scene_;
    // a G / R / S preview isn't saved, and its game object is gone after Open and New
    if (modal_transform_)
    {
        cancel_modal_transform(&scene);
    }
    if (get_play_mode_state() != ENGINE_PLAY_MODE_EDIT)
    {
        after_prompt_ = {};
        return;
    }
    if (request.action == SceneFileAction::eSave)
    {
        if (!scene.save_file(request.path))
        {
            // what waited for the save doesn't go on without it
            after_prompt_ = {};
            return;
        }
        commands_.mark_saved();
        // a new file shows in the asset browser at once
        asset_browser_.refresh();
        log::log(log::LogLevel::eTrace, fmt::format("Scene saved to {}.\n", request.path));
        // Open, New or quit waiting for the prompt's Save
        continue_after_prompt();
        return;
    }
    const bool as_new = request.action == SceneFileAction::eNew;
    const auto content = read_scene_file(request.path);
    if (!content || !scene.open(*content, request.path, as_new))
    {
        // logged; the scene is as it was
        return;
    }
    // like opening a scene in Unity: the selection and the undo history belong to the old content
    forget_game_objects();
    scene_hierarchy_context_.reset();
    commands_.clear();
    log::log(log::LogLevel::eTrace, as_new ? fmt::format("New scene from the template {}.\n", request.path) : fmt::format("Scene {} opened.\n", request.path));
}

void engine::ApplicationEditor::render_file_menu(Scene* scene)
{
    if (!ImGui::BeginMenu("File"))
    {
        return;
    }
    const bool in_edit = get_play_mode_state() == ENGINE_PLAY_MODE_EDIT;
    ImGui::BeginDisabled(!in_edit);
    if (ImGui::MenuItem("New...", "Ctrl+N"))
    {
        open_scene_file_dialog(SceneFileDialog::eNew, scene);
    }
    if (ImGui::MenuItem("Open...", "Ctrl+O"))
    {
        open_scene_file_dialog(SceneFileDialog::eOpen, scene);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S"))
    {
        request_scene_file_action(scene, SceneFileAction::eSave);
    }
    if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
    {
        open_scene_file_dialog(SceneFileDialog::eSaveAs, scene);
    }
    ImGui::EndDisabled();
    if (!in_edit)
    {
        ImGui::TextDisabled("%s", STOP_TO_SAVE);
    }
    ImGui::EndMenu();
}

void engine::ApplicationEditor::handle_file_shortcuts(Scene* scene)
{
    if (ImGui::GetIO().WantTextInput || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) || get_play_mode_state() != ENGINE_PLAY_MODE_EDIT)
    {
        return;
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S))
    {
        open_scene_file_dialog(SceneFileDialog::eSaveAs, scene);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
    {
        request_scene_file_action(scene, SceneFileAction::eSave);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))
    {
        open_scene_file_dialog(SceneFileDialog::eOpen, scene);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))
    {
        open_scene_file_dialog(SceneFileDialog::eNew, scene);
    }
}

void engine::ApplicationEditor::open_scene_file_dialog(SceneFileDialog dialog, Scene* scene)
{
    scene_file_dialog_ = dialog;
    open_scene_file_dialog_popup_ = true;
    scene_file_dialog_picked_.clear();
    scene_file_dialog_name_.fill('\0');
    const auto& path = scene->get_file_path();
    const bool in_scenes = !path.empty() && !std::filesystem::path(path).is_absolute();
    if (dialog == SceneFileDialog::eNew)
    {
        scene_file_dialog_folder_ = TEMPLATES_FOLDER;
    }
    else if (in_scenes)
    {
        // where the scene's file is, with its name to save over or change
        scene_file_dialog_folder_ = scenes_folder_of(path);
        if (dialog == SceneFileDialog::eSaveAs)
        {
            const auto name = std::filesystem::path(path).filename().string();
            std::strncpy(scene_file_dialog_name_.data(), name.c_str(), scene_file_dialog_name_.size() - 1);
        }
    }
    else if (scene_file_dialog_folder_ == TEMPLATES_FOLDER || !std::filesystem::is_directory(assets_root() / scene_file_dialog_folder_))
    {
        scene_file_dialog_folder_ = "scenes";
    }
}

void engine::ApplicationEditor::render_scene_file_dialogs(Scene* scene)
{
    render_scene_file_dialog(scene);
    render_unsaved_changes_prompt(scene);
}

void engine::ApplicationEditor::render_scene_file_dialog(Scene* scene)
{
    if (std::exchange(open_scene_file_dialog_popup_, false))
    {
        ImGui::OpenPopup(SCENE_FILE_DIALOG);
    }
    ImGui::SetNextWindowSize(ImVec2(520.0f, 420.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(SCENE_FILE_DIALOG))
    {
        return;
    }
    const auto dialog = scene_file_dialog_;
    const auto close = [this]()
        {
            scene_file_dialog_ = SceneFileDialog::eNone;
            ImGui::CloseCurrentPopup();
        };
    // a dialog closed while playing (Play pressed in its frame) or by a scene open has nothing to do
    if (dialog == SceneFileDialog::eNone || get_play_mode_state() != ENGINE_PLAY_MODE_EDIT)
    {
        // the prompt's Save waiting for this Save As doesn't go on
        if (dialog == SceneFileDialog::eSaveAs)
        {
            after_prompt_ = {};
        }
        close();
        ImGui::EndPopup();
        return;
    }
    ImGui::TextUnformatted(dialog == SceneFileDialog::eSaveAs ? "Save the scene as"
        : dialog == SceneFileDialog::eOpen ? "Open a scene"
        : "New scene from a template");
    ImGui::TextDisabled("assets/%s", scene_file_dialog_folder_.c_str());

    // the folders and scene files of the folder; a template is picked in its folder only
    std::optional<std::string> confirmed;
    const float footer = ImGui::GetFrameHeightWithSpacing() * (dialog == SceneFileDialog::eSaveAs ? 3.5f : 2.0f);
    if (ImGui::BeginChild("##files", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders))
    {
        if (dialog != SceneFileDialog::eNew && scene_file_dialog_folder_ != "scenes" && ImGui::Selectable("..", false, ImGuiSelectableFlags_AllowDoubleClick)
            && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            scene_file_dialog_folder_ = std::filesystem::path(scene_file_dialog_folder_).parent_path().generic_string();
        }
        for (const auto& entry : list_asset_folder(assets_root(), scene_file_dialog_folder_))
        {
            if (entry.kind == AssetFileKind::eFolder)
            {
                if (dialog == SceneFileDialog::eNew)
                {
                    continue;
                }
                const auto label = entry.name + "/";
                if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    scene_file_dialog_folder_ = entry.path;
                }
                continue;
            }
            const auto asset_id = get_asset_id(entry);
            if (entry.kind != AssetFileKind::eScene || !asset_id)
            {
                continue;
            }
            const bool picked = scene_file_dialog_picked_ == *asset_id;
            if (ImGui::Selectable(entry.name.c_str(), picked, ImGuiSelectableFlags_AllowDoubleClick))
            {
                scene_file_dialog_picked_ = *asset_id;
                if (dialog == SceneFileDialog::eSaveAs)
                {
                    scene_file_dialog_name_.fill('\0');
                    std::strncpy(scene_file_dialog_name_.data(), entry.name.c_str(), scene_file_dialog_name_.size() - 1);
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
    if (dialog == SceneFileDialog::eSaveAs)
    {
        ImGui::InputText("Name", scene_file_dialog_name_.data(), scene_file_dialog_name_.size());
        save_path = get_scene_save_path(scene_file_dialog_folder_, scene_file_dialog_name_.data());
        if (!save_path)
        {
            ImGui::TextDisabled("A file name, saved as .json in this folder.");
        }
        // a file the scene places (directly or not) would be placed inside itself once saved: the guard of
        // engineSceneLoad can't be bypassed by saving over it
        else if (scene->prefabs().places_scene_file(*save_path))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "The scene places %s: it can't be saved into it.", save_path->c_str());
            save_path.reset();
        }
        else
        {
            std::error_code error;
            const bool exists = std::filesystem::exists(AssetStore::get_instance().get_scenes_base_path() / *save_path, error);
            if (exists && *save_path != scene->get_file_path())
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
        can_confirm = !scene_file_dialog_picked_.empty();
    }

    ImGui::BeginDisabled(!can_confirm);
    const char* confirm_label = dialog == SceneFileDialog::eSaveAs ? "Save" : dialog == SceneFileDialog::eOpen ? "Open" : "Create";
    const bool confirm_pressed = ImGui::Button(confirm_label) || (can_confirm && ImGui::IsKeyPressed(ImGuiKey_Enter));
    ImGui::EndDisabled();
    if (confirm_pressed && can_confirm)
    {
        confirmed = dialog == SceneFileDialog::eSaveAs ? *save_path : scene_file_dialog_picked_;
    }
    ImGui::SameLine();
    const bool cancelled = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (confirmed)
    {
        close();
        const auto action = dialog == SceneFileDialog::eSaveAs ? SceneFileAction::eSave
            : dialog == SceneFileDialog::eOpen ? SceneFileAction::eOpen
            : SceneFileAction::eNew;
        request_scene_file_action(scene, action, *confirmed);
    }
    else if (cancelled)
    {
        // what waited for the prompt's Save doesn't go on
        if (dialog == SceneFileDialog::eSaveAs)
        {
            after_prompt_ = {};
        }
        close();
    }
    ImGui::EndPopup();
}

void engine::ApplicationEditor::render_unsaved_changes_prompt(Scene* scene)
{
    // the scene file dialog's popup opens first: the prompt waits for it to close (Save As opened by the prompt)
    if (open_unsaved_prompt_ && !ImGui::IsPopupOpen(SCENE_FILE_DIALOG))
    {
        open_unsaved_prompt_ = false;
        ImGui::OpenPopup(UNSAVED_PROMPT);
    }
    if (!ImGui::BeginPopupModal(UNSAVED_PROMPT, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    // nothing waits for an answer any more (a Play in the same frame dropped it)
    if (after_prompt_.action == SceneFileAction::eNone)
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::Text("%s has unsaved changes.", scene_display_name(*scene).c_str());
    ImGui::TextUnformatted(after_prompt_.action == SceneFileAction::eQuit ? "Save them before closing?" : "Save them first?");
    const bool in_edit = get_play_mode_state() == ENGINE_PLAY_MODE_EDIT;
    ImGui::BeginDisabled(!in_edit);
    const bool save = ImGui::Button("Save");
    ImGui::EndDisabled();
    if (!in_edit)
    {
        ImGui::SetItemTooltip("%s", STOP_TO_SAVE);
    }
    ImGui::SameLine();
    const bool discard = ImGui::Button("Don't save");
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (save && in_edit)
    {
        ImGui::CloseCurrentPopup();
        // the save goes on with what waited once it is written (apply_scene_file_request); a scene without a file is
        // saved as first
        if (scene->get_file_path().empty())
        {
            open_scene_file_dialog(SceneFileDialog::eSaveAs, scene);
        }
        else
        {
            scene_file_request_ = { SceneFileAction::eSave, scene->get_file_path() };
        }
    }
    else if (discard)
    {
        ImGui::CloseCurrentPopup();
        continue_after_prompt();
    }
    else if (cancel)
    {
        ImGui::CloseCurrentPopup();
        after_prompt_ = {};
    }
    ImGui::EndPopup();
}

void engine::ApplicationEditor::update_window_title(Scene* scene)
{
    auto* window = rdx_.get_sdl_window();
    if (window_title_base_.empty())
    {
        const char* title = SDL_GetWindowTitle(window);
        window_title_base_ = title && *title ? title : "Engine";
    }
    const auto title = fmt::format("{} - {}{}", window_title_base_, scene_display_name(*scene), commands_.is_dirty() ? "*" : "");
    if (title != window_title_)
    {
        window_title_ = title;
        SDL_SetWindowTitle(window, window_title_.c_str());
    }
}
