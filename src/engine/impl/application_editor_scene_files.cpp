// The editor's File menu: Save, Save As, Open and New of the scene, its dialog and the unsaved changes prompt.
#include "application_editor.h"
#include "logger.h"
#include "scene.h"
#include "imgui/imgui.h"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <string>
#include <utility>

namespace
{
constexpr const char* UNSAVED_PROMPT = "Unsaved changes";
constexpr const char* STOP_TO_SAVE = "Stop to save: nothing of a play session is saved";

engine::SceneFileAction action_of(engine::SceneFileDialog::Kind kind)
{
    switch (kind)
    {
    case engine::SceneFileDialog::Kind::eSaveAs: return engine::SceneFileAction::eSave;
    case engine::SceneFileDialog::Kind::eOpen: return engine::SceneFileAction::eOpen;
    case engine::SceneFileDialog::Kind::eNew: return engine::SceneFileAction::eNew;
    case engine::SceneFileDialog::Kind::eNone: break;
    }
    return engine::SceneFileAction::eNone;
}
}  // namespace

std::string engine::ApplicationEditor::scene_display_name(const Scene& scene)
{
    return scene.get_file_path().empty() ? std::string("Untitled") : scene.get_file_path();
}

bool engine::ApplicationEditor::on_quit_requested()
{
    return scene_files_.request_quit(has_unsaved_changes());
}

bool engine::ApplicationEditor::has_unsaved_changes() const
{
    return commands_.is_dirty() || data_assets_.has_unsaved();
}

void engine::ApplicationEditor::request_scene_file_action(Scene* scene, SceneFileAction action, std::string path)
{
    if (get_play_mode_state() != ENGINE_PLAY_MODE_EDIT)
    {
        log::log(log::LogLevel::eError, "Scene files: stop the play session first (nothing of it is saved).\n");
        return;
    }
    // a new scene has no file yet
    if (scene_files_.request(action, std::move(path), scene->get_file_path(), has_unsaved_changes()))
    {
        scene_file_dialog_.open(SceneFileDialog::Kind::eSaveAs, *scene);
    }
    // Save All: the data assets are written with the scene (after Save As was confirmed, when the scene has no file)
    else if (action == SceneFileAction::eSave)
    {
        data_assets_.save_all();
    }
}

void engine::ApplicationEditor::forget_game_objects()
{
    modal_transform_.reset();
    modal_transform_entity_ = entt::null;
    gizmo_hovered_entity_ = entt::null;
    gizmo_hovered_handle_.reset();
    delete_without_undo_ = entt::null;
    scene_hierarchy_context_.end_rename();
    name_field_active_ = false;
    name_field_entity_ = entt::null;
}

void engine::ApplicationEditor::apply_scene_file_request()
{
    if (!scene_files_.has_request() || !play_mode_scene_)
    {
        return;
    }
    const auto request = scene_files_.take_request();
    auto& scene = *play_mode_scene_;
    // a G / R / S preview isn't saved, and its game object is gone after Open and New
    if (modal_transform_)
    {
        cancel_modal_transform(&scene);
    }
    if (get_play_mode_state() != ENGINE_PLAY_MODE_EDIT)
    {
        scene_files_.cancel_waiting();
        return;
    }
    if (request.action == SceneFileAction::eSave)
    {
        if (!scene.save_file(request.path))
        {
            // what waited for the save doesn't go on without it
            scene_files_.cancel_waiting();
            return;
        }
        commands_.mark_saved();
        // a new file shows in the asset browser at once
        asset_browser_.refresh();
        log::log(log::LogLevel::eTrace, fmt::format("Scene saved to {}.\n", request.path));
        // Open, New or quit waiting for the prompt's Save
        if (scene_files_.on_saved())
        {
            confirm_quit();
        }
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
        scene_file_dialog_.open(SceneFileDialog::Kind::eNew, *scene);
    }
    if (ImGui::MenuItem("Open...", "Ctrl+O"))
    {
        scene_file_dialog_.open(SceneFileDialog::Kind::eOpen, *scene);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S"))
    {
        request_scene_file_action(scene, SceneFileAction::eSave);
    }
    if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
    {
        scene_file_dialog_.open(SceneFileDialog::Kind::eSaveAs, *scene);
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
        scene_file_dialog_.open(SceneFileDialog::Kind::eSaveAs, *scene);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
    {
        request_scene_file_action(scene, SceneFileAction::eSave);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))
    {
        scene_file_dialog_.open(SceneFileDialog::Kind::eOpen, *scene);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))
    {
        scene_file_dialog_.open(SceneFileDialog::Kind::eNew, *scene);
    }
}

void engine::ApplicationEditor::render_scene_file_dialogs(Scene* scene)
{
    if (const auto result = scene_file_dialog_.render(*scene, get_play_mode_state() == ENGINE_PLAY_MODE_EDIT))
    {
        if (result->path)
        {
            request_scene_file_action(scene, action_of(result->kind), *result->path);
        }
        // what waited for the prompt's Save doesn't go on
        else if (result->kind == SceneFileDialog::Kind::eSaveAs)
        {
            scene_files_.cancel_waiting();
        }
    }
    render_unsaved_changes_prompt(scene);
}

void engine::ApplicationEditor::render_unsaved_changes_prompt(Scene* scene)
{
    // the scene file dialog's popup opens first: the prompt waits for it to close (Save As opened by the prompt)
    if (!scene_file_dialog_.is_open() && scene_files_.take_prompt_open())
    {
        ImGui::OpenPopup(UNSAVED_PROMPT);
    }
    if (!ImGui::BeginPopupModal(UNSAVED_PROMPT, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    const auto waiting = scene_files_.get_waiting();
    // nothing waits for an answer any more (a Play in the same frame dropped it)
    if (waiting == SceneFileAction::eNone)
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const bool scene_dirty = commands_.is_dirty();
    if (scene_dirty)
    {
        ImGui::Text("%s has unsaved changes.", scene_display_name(*scene).c_str());
    }
    // like Unreal's Save Content dialog: the data assets are saved with the scene
    if (const auto assets = data_assets_.get_unsaved(); !assets.empty())
    {
        ImGui::TextUnformatted(scene_dirty ? "So do these data assets:" : "These data assets have unsaved changes:");
        for (const auto& asset : assets)
        {
            ImGui::BulletText("%s", asset.c_str());
        }
    }
    ImGui::TextUnformatted(waiting == SceneFileAction::eQuit ? "Save them before closing?" : "Save them first?");
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
        if (!data_assets_.save_all())
        {
            // logged; what waited doesn't go on without its save
            scene_files_.cancel_waiting();
        }
        // what waited goes on once the file is written (apply_scene_file_request); a scene without a file is saved as
        // first
        else if (!scene_dirty)
        {
            if (scene_files_.on_saved())
            {
                confirm_quit();
            }
        }
        else if (scene_files_.prompt_save(scene->get_file_path()))
        {
            scene_file_dialog_.open(SceneFileDialog::Kind::eSaveAs, *scene);
        }
    }
    else if (discard)
    {
        ImGui::CloseCurrentPopup();
        // the data assets keep their changes in memory, still unsaved, unless the editor closes (like Unreal's Don't Save)
        if (scene_files_.prompt_discard())
        {
            confirm_quit();
        }
    }
    else if (cancel)
    {
        ImGui::CloseCurrentPopup();
        scene_files_.cancel_waiting();
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