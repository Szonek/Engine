#include "application_editor.h"

#include "scene.h"
#include "math_helpers.h"
#include "world_transform.h"
#include "logger.h"
#include "profiler.h"
#include "animation_controller.h"
#include "engine_string_impl_def.h"

#include "components_internals/camera_internal_component.h"
#include "components_internals/editor_object_internal_component.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_opengl3.h"

#include "imguizmo/ImGuizmo.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <functional>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <vector>

namespace
{
bool any_mouse_button_down(const ImGuiIO& io)
{
    return io.MouseDown[ImGuiMouseButton_Left] || io.MouseDown[ImGuiMouseButton_Right] || io.MouseDown[ImGuiMouseButton_Middle];
}

// what the editor camera reads this frame (and the scene view's right-click, ContextClick)
engine::editor_camera_input_t read_editor_camera_input()
{
    const auto& io = ImGui::GetIO();
    engine::editor_camera_input_t input{};
    if (ImGui::IsMousePosValid() && ImGui::IsMousePosValid(&io.MousePosPrev))
    {
        input.mouse_delta = { io.MouseDelta.x, io.MouseDelta.y };
    }
    input.wheel = io.MouseWheel;
    input.lmb = io.MouseDown[ImGuiMouseButton_Left];
    input.mmb = io.MouseDown[ImGuiMouseButton_Middle];
    input.rmb = io.MouseDown[ImGuiMouseButton_Right];
    input.alt = io.KeyAlt;
    input.shift = io.KeyShift;
    input.ctrl = io.KeyCtrl;
    input.move_forward = ImGui::IsKeyDown(ImGuiKey_W);
    input.move_back = ImGui::IsKeyDown(ImGuiKey_S);
    input.move_left = ImGui::IsKeyDown(ImGuiKey_A);
    input.move_right = ImGui::IsKeyDown(ImGuiKey_D);
    input.move_down = ImGui::IsKeyDown(ImGuiKey_Q);
    input.move_up = ImGui::IsKeyDown(ImGuiKey_E);
    return input;
}

struct entity_node_t
{
    entt::entity entity = entt::null;
    std::string name = "";

    entity_node_t* parent = nullptr;
    std::vector<entity_node_t*> children;

};


inline bool is_entity_parent_of(entt::entity parent, entt::entity child, engine::Scene* scene)
{
    if (parent == child)
    {
        return true;
    }
    if (scene->has_component<engine_parent_component_t>(child))
    {
        const auto pc = *scene->get_component<engine_parent_component_t>(child);
        return is_entity_parent_of(parent, static_cast<entt::entity>(pc.parent), scene);
    }
    return false;
}

// copies the game object with what is parented under it and selects the copy (see CommandDuplicateEntity)
void duplicate_entity(entt::entity e, engine::Scene* scene, engine::CommandManager& commands, engine::SceneHierarchyContext& ctx)
{
    commands.execute_command(*scene, std::make_unique<engine::CommandDuplicateEntity>(*scene, e, ctx.get_selected_entity()));
    ctx.sync_selected_from_scene(scene);
}

// renames the game object with an undo step; an empty or unchanged name changes nothing
void rename_entity(entt::entity e, std::string_view name, engine::Scene* scene, engine::CommandManager& commands)
{
    if (name.empty() || !scene->is_valid_entity(e) || !scene->has_component<engine_name_component_t>(e)
        || name == scene->get_component<engine_name_component_t>(e)->name)
    {
        return;
    }
    commands.execute_command(*scene, std::make_unique<engine::CommandRenameEntity>(*scene, e, name));
}

// the text field in the row of the game object renamed in place (SceneHierarchyContext::request_rename)
void display_rename_field(entt::entity e, engine::Scene* scene, engine::CommandManager& commands, engine::SceneHierarchyContext& ctx)
{
    auto& buffer = ctx.get_rename_buffer();
    ImGui::SameLine();
    if (ctx.take_rename_focus())
    {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##rename", buffer.data(), buffer.size(), ImGuiInputTextFlags_AutoSelectAll);
    // Enter and clicking away rename it; Escape has given the field its starting text back, which renames nothing
    if (ImGui::IsItemDeactivated())
    {
        rename_entity(e, buffer.data(), scene, commands);
        ctx.end_rename();
    }
    else if (!ImGui::IsItemActive() && !ctx.is_rename_starting())
    {
        // the field never got the keyboard focus (another window took it)
        ctx.end_rename();
    }
}

// Deletes the game object with an undo step; a runtime (transient) one isn't saved, so undo can't bring it back:
// deleting it is confirmed first. Nothing when it can't be deleted (CommandDestroyEntity::can_destroy).
void delete_entity(entt::entity e, engine::Scene* scene, engine::CommandManager& commands, engine::SceneHierarchyContext& ctx)
{
    if (scene->get_persistent_id(e) == engine::EntityIdRegistry::INVALID_ID)
    {
        ctx.request_delete_without_undo(e);
    }
    else if (engine::CommandDestroyEntity::can_destroy(*scene, e))
    {
        ctx.set_selected_entity(scene, entt::null, commands);
        commands.execute_command(*scene, std::make_unique<engine::CommandDestroyEntity>(*scene, e));
    }
}

// What can be done to one game object, in the hierarchy's context menu and the scene view's (same items, same rules)
void display_game_object_menu_items(entt::entity e, engine::Scene* scene, engine::CommandManager& commands, engine::SceneHierarchyContext& ctx)
{
    const auto instance_part = engine::EntityIdRegistry::is_hashed_id(scene->get_persistent_id(e));
    const auto transient = scene->get_persistent_id(e) == engine::EntityIdRegistry::INVALID_ID;
    if (ImGui::MenuItem("Rename", "F2", false, scene->has_component<engine_name_component_t>(e)))
    {
        ctx.request_rename(scene, e);
        ctx.set_forced_open_selected_parents(true);
    }

    const auto can_duplicate = engine::CommandDuplicateEntity::can_duplicate(*scene, e);
    if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, can_duplicate))
    {
        duplicate_entity(e, scene, commands, ctx);
    }
    if (!can_duplicate)
    {
        ImGui::SetItemTooltip(transient ? "A runtime (transient) object isn't scene content, so it isn't duplicated"
            : instance_part ? "Part of a prefab instance: it can't be duplicated out of it yet (duplicate the instance's root instead)"
            : "A part of a prefab instance is under it without its instance's root: move it back to its instance first");
    }

    const auto can_delete = engine::CommandDestroyEntity::can_destroy(*scene, e);
    if (transient)
    {
        if (ImGui::MenuItem("Delete (can't be undone)", "Del"))
        {
            delete_entity(e, scene, commands, ctx);
        }
        ImGui::SetItemTooltip("A runtime (transient) object isn't saved, so deleting it can't be undone");
    }
    else if (ImGui::MenuItem("Delete", "Del", false, can_delete))
    {
        delete_entity(e, scene, commands, ctx);
    }
    if (!transient && !can_delete)
    {
        ImGui::SetItemTooltip(instance_part ? "Part of a prefab instance: removing it from the instance can't be saved yet (delete the instance's root instead)"
            : "A part of a prefab instance is under it without its instance's root: move it back to its instance first");
    }
}

inline void display_node(entity_node_t* node, engine::Scene* scene, engine::CommandManager& commands, engine::SceneHierarchyContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("editor-display_node");
    uint32_t dispaly_flags = ctx.get_selected_entity() == node->entity ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
    //dispaly_flags |= ImGuiTreeNodeFlags_DefaultOpen;
    dispaly_flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node->children.empty()) // if is leaf
    {
        dispaly_flags |= ImGuiTreeNodeFlags_Leaf;
        dispaly_flags |= ImGuiTreeNodeFlags_Bullet;
    }
    else
    {
        dispaly_flags |= ImGuiTreeNodeFlags_OpenOnArrow;
    }

    // if we have selected entity by mouse then open all parents of selected entity for easier naviation in the scene panel
    if (ctx.has_selected_entity())
    {
        if (ctx.is_forced_open_selected_parents())
        {
            ImGui::SetNextItemOpen(is_entity_parent_of(node->entity, ctx.get_selected_entity(), scene));
        }
    }

    // while it is renamed, the text field next to the arrow shows the name
    const bool renaming = ctx.is_renaming(node->entity);
    const auto entity_id = (renaming ? std::string() : node->name) + "##" + std::to_string(static_cast<std::uint32_t>(node->entity));
    // a collapsed node returns false but can still be clicked, dragged etc., so its item is handled either way
    const bool open = ImGui::TreeNodeEx(entity_id.c_str(), dispaly_flags);
    // a game object its prefab instance created stays under its instance's root, like Unity's (which can't be
    // restructured): one moved out of it would be saved apart from its instance
    const auto persistent_id = scene->get_persistent_id(node->entity);
    const auto instance_part = engine::EntityIdRegistry::is_hashed_id(persistent_id);
    // tooltip the id
    ImGui::SetItemTooltip("ID: %llu (entity: %u)%s", static_cast<unsigned long long>(persistent_id), static_cast<std::uint32_t>(node->entity),
        instance_part ? "\nPart of a prefab instance: it can't be moved out of it (move the instance's root)" : "");

    // select entity with LMB
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        // but if tree was expanded (i.e. by pressing arrow) it does not count as selection of entity
        if (!ImGui::IsItemToggledOpen())
        {
            ctx.set_selected_entity(scene, node->entity, commands);
        }
    }
    // a double-click renames it, like Godot's scene dock
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
    {
        ctx.request_rename(scene, node->entity);
    }

    // context menu with RMB
    if (ImGui::BeginPopupContextItem())
    {
        display_game_object_menu_items(node->entity, scene, commands, ctx);
        ImGui::EndPopup();
    }

    // drag and drop
    if (!instance_part && ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("DND_ENTITY", &node->entity, sizeof(entt::entity));
        ImGui::Text("Drag and drop. Entity: %s", node->name.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY"))
        {
            IM_ASSERT(payload->DataSize == sizeof(entt::entity));
            entt::entity payload_n = *static_cast<entt::entity*>(payload->Data);
            //engine::log::log(engine::log::LogLevel::eInfo, "Dropped entity %d to %d\n", payload_n, node->entity);

            // a game object can't become a child of itself or of one of its own children (a cycle): such a drop is ignored
            const bool creates_cycle = is_entity_parent_of(payload_n, node->entity, scene);
            if (!creates_cycle && scene->has_component<engine_parent_component_t>(payload_n))
            {
                engine_parent_component_t pc{};
                pc.parent = static_cast<std::uint32_t>(node->entity);
                scene->update_component<engine_parent_component_t>(payload_n, pc);
            }
            else if (!creates_cycle)
            {
                auto pc = *scene->add_component<engine_parent_component_t>(payload_n);
                pc.parent = static_cast<std::uint32_t>(node->entity);
                scene->update_component<engine_parent_component_t>(payload_n, pc);
            }
            // reparenting has no undo step yet
            if (!creates_cycle)
            {
                commands.mark_changed();
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (renaming)
    {
        display_rename_field(node->entity, scene, commands, ctx);
    }

    if (open)
    {
        for (auto& child : node->children)
        {
            display_node(child, scene, commands, ctx);
        }
        ImGui::TreePop();
    }
}

// A folder of the scene hierarchy. The game objects without a parent are listed in the folder their folder component
// names; folders nest along the '/' separated path.
struct folder_node_t
{
    // from the hierarchy root, e.g. "environment/walls"; "" for the root
    std::string path;
    std::map<std::string, std::unique_ptr<folder_node_t>> folders;
    std::vector<entity_node_t*> entities;
};

// the folder path of the game object without empty segments ("a//b/" is "a/b")
inline std::string get_folder_path(const engine::Scene* scene, entt::entity entity)
{
    if (!scene->has_component<engine_folder_component_t>(entity))
    {
        return "";
    }
    const auto* folder = scene->get_component<engine_folder_component_t>(entity);
    const std::string_view path(folder->path, strnlen(folder->path, ENGINE_FOLDER_PATH_MAX_LENGTH));
    std::string ret;
    std::size_t begin = 0;
    while (begin <= path.size())
    {
        auto end = path.find('/', begin);
        end = end == std::string_view::npos ? path.size() : end;
        if (end > begin)
        {
            ret += (ret.empty() ? "" : "/") + std::string(path.substr(begin, end - begin));
        }
        begin = end + 1;
    }
    return ret;
}

inline folder_node_t& get_or_create_folder(folder_node_t& root, std::string_view path)
{
    auto* folder = &root;
    std::size_t begin = 0;
    while (begin < path.size())
    {
        auto end = path.find('/', begin);
        end = end == std::string_view::npos ? path.size() : end;
        auto& sub = folder->folders[std::string(path.substr(begin, end - begin))];
        if (!sub)
        {
            sub = std::make_unique<folder_node_t>();
            sub->path = std::string(path.substr(0, end));
        }
        folder = sub.get();
        begin = end + 1;
    }
    return *folder;
}

inline std::size_t count_folder_entities(const folder_node_t& folder)
{
    auto count = folder.entities.size();
    for (const auto& [name, sub] : folder.folders)
    {
        count += count_folder_entities(*sub);
    }
    return count;
}

// whether `folder_path` is `path` or one of its parent folders
inline bool is_folder_on_path(std::string_view folder_path, std::string_view path)
{
    return path == folder_path || (path.size() > folder_path.size() && path.substr(0, folder_path.size()) == folder_path && path[folder_path.size()] == '/');
}

// the folder of the top parent of the entity: the folder the entity is listed in
inline std::string get_listed_folder_path(const engine::Scene* scene, entt::entity entity)
{
    while (scene->has_component<engine_parent_component_t>(entity))
    {
        entity = static_cast<entt::entity>(scene->get_component<engine_parent_component_t>(entity)->parent);
    }
    return get_folder_path(scene, entity);
}

inline void display_folder(const std::string& name, folder_node_t& folder, engine::Scene* scene, engine::CommandManager& commands, engine::SceneHierarchyContext& ctx)
{
    // if we have selected entity by mouse then open all folders of selected entity for easier naviation in the scene panel
    if (ctx.has_selected_entity() && ctx.is_forced_open_selected_parents())
    {
        ImGui::SetNextItemOpen(is_folder_on_path(folder.path, get_listed_folder_path(scene, ctx.get_selected_entity())));
    }
    const auto label = fmt::format("{} ({})##folder:{}", name, count_folder_entities(folder), folder.path);
    const bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth);

    // an entity dropped on a folder is listed in it (and is no one's child anymore)
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY"))
        {
            IM_ASSERT(payload->DataSize == sizeof(entt::entity));
            const entt::entity dropped = *static_cast<entt::entity*>(payload->Data);
            if (scene->has_component<engine_parent_component_t>(dropped))
            {
                scene->remove_component<engine_parent_component_t>(dropped);
            }
            if (!scene->has_component<engine_folder_component_t>(dropped))
            {
                scene->add_component<engine_folder_component_t>(dropped);
            }
            engine_folder_component_t fc{};
            std::strncpy(fc.path, folder.path.c_str(), ENGINE_FOLDER_PATH_MAX_LENGTH - 1);
            scene->update_component<engine_folder_component_t>(dropped, fc);
            // moving to a folder has no undo step yet
            commands.mark_changed();
        }
        ImGui::EndDragDropTarget();
    }

    if (open)
    {
        for (auto& [sub_name, sub] : folder.folders)
        {
            display_folder(sub_name, *sub, scene, commands, ctx);
        }
        for (auto* node : folder.entities)
        {
            display_node(node, scene, commands, ctx);
        }
        ImGui::TreePop();
    }
}

// The header of a component the game object has, with its menu (like Unity's ⋮ menu): opened by right-clicking the
// header or by its button at the header's right. Returns whether the header is open; `remove` is set when Remove was
// picked.
bool display_component_header(const char* name, bool& remove)
{
    const bool open = ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);
    constexpr const char* menu = "##component_menu";
    ImGui::OpenPopupOnItemClick(menu, ImGuiPopupFlags_MouseButtonRight);
    const char* button = "...";
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(button).x - ImGui::GetStyle().FramePadding.x * 2.0f);
    if (ImGui::SmallButton(button))
    {
        ImGui::OpenPopup(menu);
    }
    remove = false;
    if (ImGui::BeginPopup(menu))
    {
        remove = ImGui::MenuItem("Remove");
        ImGui::EndPopup();
    }
    return open;
}

// a component of the engine the game object has, with its hand-written panel `fn`
template<typename T>
inline void display_component(std::string_view name, engine::Scene* scene, engine::CommandManager& commands, entt::entity entity, std::function<bool(engine::Scene* scene, T& comp)> fn)
{
    ENGINE_PROFILE_SECTION_N("editor-display_component");
    ImGui::PushID(name.data());
    bool remove = false;
    if (display_component_header(name.data(), remove) && !remove)
    {
        auto comp = *scene->get_component<T>(entity);
        if (fn(scene, comp))
        {
            commands.execute_command(*scene, std::make_unique<engine::CommandUpdateComponent<T>>(*scene, entity, comp));
        }
    }
    if (remove)
    {
        scene->remove_component<T>(entity);
        // adding and removing the engine's components have no undo step yet
        commands.mark_changed();
    }
    ImGui::PopID();
}

// Combo box picking a resource by its asset ID (atlas name) instead of typing its index; "None" selects
// ENGINE_INVALID_OBJECT_HANDLE. The list of assets is only built while the combo is open. An asset of the field's kind
// dragged from the asset browser can be dropped on it (like Unity's object fields); the scene then holds the file it
// comes from (SceneAssets). Returns true when the selection changed.
bool display_asset_combo(engine::Scene* scene, engine::AssetKind kind, const char* label, std::uint32_t& handle, std::string_view current_name,
    const std::function<std::vector<std::pair<std::uint32_t, std::string>>()>& get_assets)
{
    const auto preview = handle == ENGINE_INVALID_OBJECT_HANDLE ? std::string("None")
        : current_name.empty() ? fmt::format("<missing #{}>", handle) : std::string(current_name);
    bool changed = false;
    const bool open = ImGui::BeginCombo(label, preview.c_str());
    // while the combo is closed, the last item is its preview; the browser drags meshes, images, models and scene files
    const auto* payload_type = kind == engine::AssetKind::eGeometry ? engine::ASSET_PAYLOAD_GEOMETRY
        : kind == engine::AssetKind::eTexture ? engine::ASSET_PAYLOAD_TEXTURE
        : kind == engine::AssetKind::ePrefab ? engine::ASSET_PAYLOAD_PREFAB : nullptr;
    if (!open && payload_type && ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payload_type))
        {
            const std::string_view asset_id(static_cast<const char*>(payload->Data));
            if (const auto dropped = scene->assets().resolve(kind, asset_id); dropped != ENGINE_INVALID_OBJECT_HANDLE)
            {
                changed = dropped != handle;
                handle = dropped;
            }
            else
            {
                engine::log::log(engine::log::LogLevel::eError, fmt::format("Editor: the asset {} dropped on {} can't be found.\n", asset_id, label));
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (open)
    {
        if (ImGui::Selectable("None", handle == ENGINE_INVALID_OBJECT_HANDLE))
        {
            changed = handle != ENGINE_INVALID_OBJECT_HANDLE;
            handle = ENGINE_INVALID_OBJECT_HANDLE;
        }
        for (const auto& [idx, name] : get_assets())
        {
            const auto selected = idx == handle;
            if (ImGui::Selectable(name.c_str(), selected))
            {
                changed |= !selected;
                // the scene holds a data asset or prefab it references (whoever loaded it may release it); the other
                // kinds are held by whoever created them
                handle = kind == engine::AssetKind::eDataAsset || kind == engine::AssetKind::ePrefab ? scene->assets().resolve(kind, name) : idx;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool display_geometry_combo(engine::Scene* scene, std::uint32_t& geometry)
{
    const auto* app = scene->get_application();
    return display_asset_combo(scene, engine::AssetKind::eGeometry, "Geometry", geometry, app->get_geometry_name(geometry), [app] { return app->get_geometry_names(); });
}

bool display_texture_combo(engine::Scene* scene, const char* label, std::uint32_t& texture)
{
    const auto* app = scene->get_application();
    return display_asset_combo(scene, engine::AssetKind::eTexture, label, texture, app->get_texture_name(texture), [app] { return app->get_texture_names(); });
}

// Draws a value of a game-defined type from its description, a widget per field kind (game types have no unions; the
// engine's components have hand-written panels). Edits the value in place and returns true when a field changed.
class TypeInspector
{
public:
    explicit TypeInspector(engine::Scene* scene) : scene_(scene) {}

    bool draw_struct(const engine::TypeInfo& type, std::byte* value)
    {
        bool changed = false;
        for (const auto& field : type.fields)
        {
            if (field.hints.hidden)
            {
                continue;
            }
            ImGui::PushID(field.name.c_str());
            ImGui::BeginDisabled(field.hints.read_only);
            changed |= draw_field(field, value + field.offset);
            ImGui::EndDisabled();
            if (!field.hints.tooltip.empty())
            {
                ImGui::SetItemTooltip("%s", field.hints.tooltip.c_str());
            }
            ImGui::PopID();
        }
        return changed;
    }

private:
    bool draw_field(const engine::FieldInfo& field, std::byte* p)
    {
        using engine::FieldKind;
        if (!field.is_array)
        {
            return draw_element(field, p, field.name.c_str());
        }
        const auto count = static_cast<int>(field.count);
        if (field.hints.color)
        {
            auto* rgba = reinterpret_cast<float*>(p);
            return count == 4 ? ImGui::ColorEdit4(field.name.c_str(), rgba) : ImGui::ColorEdit3(field.name.c_str(), rgba);
        }
        // short arrays of numbers (vectors) on one line
        if (count <= 4 && is_number(field.kind))
        {
            return draw_numbers(field, p, field.name.c_str(), count);
        }
        bool changed = false;
        if (ImGui::TreeNode(fmt::format("{} [{}]", field.name, field.count).c_str()))
        {
            for (std::size_t i = 0; i < field.count; i++)
            {
                ImGui::PushID(static_cast<int>(i));
                changed |= draw_element(field, p + i * field.size, fmt::format("[{}]", i).c_str());
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        return changed;
    }

    bool draw_element(const engine::FieldInfo& field, std::byte* p, const char* label)
    {
        using engine::FieldKind;
        switch (field.kind)
        {
        case FieldKind::eBool: return ImGui::Checkbox(label, reinterpret_cast<bool*>(p));
        case FieldKind::eInt32:
        case FieldKind::eUint32:
        case FieldKind::eFloat: return draw_numbers(field, p, label, 1);
        case FieldKind::eString: return draw_string(field, p, label);
        case FieldKind::eEngineString: return false;  // game types have none (the engine's components have their own panels)
        case FieldKind::eEnum: return draw_enum(field, p, label);
        case FieldKind::eStruct:
        {
            bool changed = false;
            if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
            {
                changed = draw_struct(*field.struct_type, p);
                ImGui::TreePop();
            }
            return changed;
        }
        case FieldKind::eUnion: return false;  // game types have no unions (the engine's components have their own panels)
        case FieldKind::eAsset:
        {
            auto handle = read<std::uint32_t>(p);
            const auto* app = scene_->get_application();
            bool changed = false;
            switch (field.asset_kind)
            {
            case engine::AssetKind::eGeometry:
                changed = display_asset_combo(scene_, field.asset_kind, label, handle, app->get_geometry_name(handle), [app] { return app->get_geometry_names(); });
                break;
            case engine::AssetKind::eDataAsset:
                // the loaded data assets holding the field's type
                changed = display_asset_combo(scene_, field.asset_kind, label, handle, app->get_data_asset_name(handle), [app, &field] {
                    auto assets = app->get_data_asset_names();
                    std::erase_if(assets, [&](const auto& asset) { return &app->get_data_asset(asset.first)->get_type() != field.data_type; });
                    return assets;
                });
                break;
            case engine::AssetKind::ePrefab:
                // the loaded models and scene files; others are dropped from the asset browser
                changed = display_asset_combo(scene_, field.asset_kind, label, handle, app->get_prefab_name(handle), [app] { return app->get_prefab_names(); });
                break;
            default:
                changed = display_asset_combo(scene_, field.asset_kind, label, handle, app->get_texture_name(handle), [app] { return app->get_texture_names(); });
                break;
            }
            std::memcpy(p, &handle, sizeof(handle));
            return changed;
        }
        case FieldKind::eGameObject: return draw_game_object(p, label);
        }
        return false;
    }

    static bool is_number(engine::FieldKind kind)
    {
        return kind == engine::FieldKind::eInt32 || kind == engine::FieldKind::eUint32 || kind == engine::FieldKind::eFloat;
    }

    // `count` numbers on one line, kept in the field's range when it has one
    static bool draw_numbers(const engine::FieldInfo& field, std::byte* p, const char* label, int count)
    {
        using engine::FieldKind;
        const auto data_type = field.kind == FieldKind::eInt32 ? ImGuiDataType_S32 : field.kind == FieldKind::eUint32 ? ImGuiDataType_U32 : ImGuiDataType_Float;
        const float speed = field.kind == FieldKind::eFloat ? 0.1f : 1.0f;
        if (!field.hints.range)
        {
            return ImGui::DragScalarN(label, data_type, p, count, speed);
        }
        // the limits in the field's own type (they fit it: the type's description was checked)
        const auto [min_value, max_value] = *field.hints.range;
        std::int32_t min_i = static_cast<std::int32_t>(min_value), max_i = static_cast<std::int32_t>(max_value);
        std::uint32_t min_u = static_cast<std::uint32_t>(min_value), max_u = static_cast<std::uint32_t>(max_value);
        const void* min_p = field.kind == FieldKind::eInt32 ? static_cast<const void*>(&min_i) : field.kind == FieldKind::eUint32 ? static_cast<const void*>(&min_u) : &min_value;
        const void* max_p = field.kind == FieldKind::eInt32 ? static_cast<const void*>(&max_i) : field.kind == FieldKind::eUint32 ? static_cast<const void*>(&max_u) : &max_value;
        return ImGui::DragScalarN(label, data_type, p, count, speed, min_p, max_p, nullptr, ImGuiSliderFlags_AlwaysClamp);
    }

    template<typename T>
    static T read(const std::byte* p)
    {
        T value;
        std::memcpy(&value, p, sizeof(T));
        return value;
    }

    // A char array needn't hold its terminator (a full one is saved as it is), so it is edited in a terminated copy,
    // written back when it changed (at most size - 1 chars and the terminator).
    static bool draw_string(const engine::FieldInfo& field, std::byte* p, const char* label)
    {
        const auto* chars = reinterpret_cast<const char*>(p);
        std::vector<char> text(field.size + 1, '\0');
        std::memcpy(text.data(), chars, strnlen(chars, field.size - 1));
        if (!ImGui::InputText(label, text.data(), field.size))
        {
            return false;
        }
        std::memcpy(p, text.data(), field.size);
        return true;
    }

    static bool draw_enum(const engine::FieldInfo& field, std::byte* p, const char* label)
    {
        const auto value = read<std::int32_t>(p);
        const auto current = std::ranges::find(field.enum_values, value, &engine::EnumValue::value);
        const auto preview = current == field.enum_values.end() ? fmt::format("<{}>", value) : current->name;
        bool changed = false;
        if (ImGui::BeginCombo(label, preview.c_str()))
        {
            for (const auto& v : field.enum_values)
            {
                if (ImGui::Selectable(v.name.c_str(), v.value == value) && v.value != value)
                {
                    std::memcpy(p, &v.value, sizeof(v.value));
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    // "name #persistent ID"; a game object without a persistent ID (transient) isn't saved, so references to it aren't
    std::string game_object_label(entt::entity e) const
    {
        const auto name = scene_->has_component<engine_name_component_t>(e) ? std::string(scene_->get_component<engine_name_component_t>(e)->name) : std::string("game object");
        const auto id = scene_->get_persistent_id(e);
        return id == engine::EntityIdRegistry::INVALID_ID ? fmt::format("{} (transient)", name) : fmt::format("{} #{}", name, id);
    }

    // the referenced game object by name and persistent ID, picked from the named game objects which are saved
    bool draw_game_object(std::byte* p, const char* label)
    {
        const auto go = read<engine_game_object_t>(p);
        const auto entity = static_cast<entt::entity>(go);
        const auto preview = go == ENGINE_INVALID_GAME_OBJECT_ID ? std::string("None")
            : !scene_->is_valid_entity(entity) ? fmt::format("<destroyed {}>", go) : game_object_label(entity);
        bool changed = false;
        if (ImGui::BeginCombo(label, preview.c_str()))
        {
            const auto select = [&](engine_game_object_t candidate, const std::string& text)
            {
                if (ImGui::Selectable(text.c_str(), candidate == go) && candidate != go)
                {
                    std::memcpy(p, &candidate, sizeof(candidate));
                    changed = true;
                }
            };
            select(ENGINE_INVALID_GAME_OBJECT_ID, "None");
            for (const auto e : scene_->get_all_entities())
            {
                if (scene_->has_component<engine_name_component_t>(e) && scene_->get_persistent_id(e) != engine::EntityIdRegistry::INVALID_ID)
                {
                    ImGui::PushID(static_cast<int>(e));
                    select(static_cast<engine_game_object_t>(e), game_object_label(e));
                    ImGui::PopID();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    engine::Scene* scene_;
};

// display_component for a component known only by its type description, drawn by the TypeInspector
void display_typed_component(const engine::TypeInfo& type, engine::Scene* scene, engine::CommandManager& commands, entt::entity entity)
{
    ENGINE_PROFILE_SECTION_N("editor-display_typed_component");
    ImGui::PushID(type.name.c_str());
    bool remove = false;
    if (display_component_header(type.name.c_str(), remove) && !remove)
    {
        engine::ValueBuffer value(type.size);
        scene->get_component(type, entity, value.get());
        if (TypeInspector(scene).draw_struct(type, value.get()))
        {
            // with the components the new value requires (a script's config), in the same undo step
            commands.execute_command(*scene, engine::make_update_typed_component_command(*scene, entity, type, value.get()));
        }
    }
    if (remove)
    {
        commands.execute_command(*scene, std::make_unique<engine::CommandRemoveTypedComponent>(*scene, entity, type));
    }
    ImGui::PopID();
}

bool display_transform_component(engine::Scene* /*scene*/, engine_transform_component_t& c)
{
    const float v_speed = 0.1f;
    bool requires_update = ImGui::DragFloat3("Position##transform", c.position, v_speed);

    glm::vec3 rot = glm::degrees(glm::eulerAngles(glm::make_quat(c.rotation)));
    if (ImGui::DragFloat3("Rotation##transform", glm::value_ptr(rot), v_speed))
    {
        requires_update = true;
        const auto final_rot = glm::normalize(glm::quat(glm::radians(rot)));
        std::memcpy(c.rotation, glm::value_ptr(final_rot), sizeof(c.rotation));
    }
    requires_update |= ImGui::DragFloat3("Scale##transform", c.scale, v_speed);
    return requires_update;
}

bool display_mesh_component(engine::Scene* scene, engine_mesh_component_t& c)
{
    bool enabled = !c.disable;
    if (ImGui::Checkbox("Enabled", &enabled))
    {
        c.disable = !c.disable;
    }
    display_geometry_combo(scene, c.geometry);
    return true;
}

// "<name> (ID <persistent id>)" of the game object owning a skin instance, or "None"
std::string skin_owner_label(const engine::Scene* scene, engine_game_object_t skin_owner)
{
    const auto entity = static_cast<entt::entity>(skin_owner);
    if (skin_owner == ENGINE_INVALID_GAME_OBJECT_ID || !scene->is_valid_entity(entity))
    {
        return "None";
    }
    const auto* name = scene->has_component<engine_name_component_t>(entity) ? scene->get_component<engine_name_component_t>(entity)->name : "<unnamed>";
    return fmt::format("{} (ID {})", name, scene->get_persistent_id(entity));
}

bool display_skinned_mesh_component(engine::Scene* scene, engine_skinned_mesh_component_t& c)
{
    bool enabled = !c.disable;
    if (ImGui::Checkbox("Enabled", &enabled))
    {
        c.disable = !c.disable;
    }
    display_geometry_combo(scene, c.geometry);
    ImGui::Text("Skin of: %s", skin_owner_label(scene, c.skin).c_str());
    return true;
}

bool display_skin_component(engine::Scene* scene, engine_skin_component_t& c)
{
    const auto name = scene->get_application()->get_skin_name(c.skin);
    ImGui::Text("Asset: %s", name.empty() ? "None" : name.c_str());
    return false;
}

bool display_folder_component(engine::Scene* /*scene*/, engine_folder_component_t& c)
{
    return ImGui::InputText("Path##folder", c.path, ENGINE_FOLDER_PATH_MAX_LENGTH);
}

bool display_joint_attachment_component(engine::Scene* scene, engine_joint_attachment_component_t& c)
{
    ImGui::Text("Skin of: %s", skin_owner_label(scene, c.skin).c_str());
    ImGui::Text("Joint name: %s", c.joint_name->str.c_str());
    return false;
}

bool display_animation_controller_component(engine::Scene* scene, engine_animation_controller_component_t& c)
{
    const auto name = scene->get_application()->get_animation_set_name(c.animations);
    ImGui::Text("Animation set: %s", name.empty() ? "None" : name.c_str());

    // layer setup; an edit reconfigures the layers of the same controller (its animation events stay)
    bool changed = false;
    const char* modes[] = { "Override", "Additive" };
    for (std::uint32_t id = 0; id < c.layers_count && id < ENGINE_ANIMATION_CONTROLLER_MAX_LAYERS; id++)
    {
        auto& layer = c.layers[id];
        ImGui::PushID(static_cast<int>(id));
        const auto layer_name = std::string_view(layer.name, strnlen(layer.name, ENGINE_ANIMATION_LAYER_NAME_MAX_LENGTH));
        ImGui::SeparatorText((layer_name.empty() ? fmt::format("Layer {}", id) : fmt::format("Layer {} ({})", id, layer_name)).c_str());
        changed |= ImGui::InputText("Name", layer.name, ENGINE_ANIMATION_LAYER_NAME_MAX_LENGTH);
        std::int32_t mode = layer.mode;
        if (ImGui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes)))
        {
            layer.mode = static_cast<engine_animation_layer_mode_t>(mode);
            changed = true;
        }
        changed |= ImGui::DragFloat("Weight", &layer.weight, 0.05f, 0.0f, 10.0f);
        changed |= ImGui::Checkbox("Looping", &layer.looping);
        for (std::uint32_t i = 0; i < layer.joint_weights_count && i < ENGINE_ANIMATION_LAYER_MAX_JOINT_WEIGHTS; i++)
        {
            ImGui::PushID(static_cast<int>(i));
            changed |= ImGui::SliderFloat(layer.joint_weights[i].joint_name, &layer.joint_weights[i].weight, 0.0f, 1.0f);
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    return changed;
}


bool display_light_component(engine::Scene* /*scene*/, engine_light_component_t& c)
{
    bool requires_update = false;
    // intensity
    constexpr const float intensity_min = 0.0f;
    constexpr const float intensity_max = 1.0f;
    requires_update |= ImGui::DragFloat3("Ambient", c.intensity.ambient, 0.05f, intensity_min, intensity_max);
    requires_update |= ImGui::DragFloat3("Diffuse", c.intensity.diffuse, 0.05f, intensity_min, intensity_max);
    requires_update |= ImGui::DragFloat3("Specular", c.intensity.specular, 0.05f, intensity_min, intensity_max);
    
    //type 
    const char* items[] = { "Directional", "Point",  "Spot" };
    std::int32_t selected_type = c.type;
    
    if (ImGui::ListBox("Type", &selected_type, items, std::size(items)))
    {
        requires_update = true;
        c.type = static_cast<engine_light_type_t>(selected_type);
    }
    // detials about the type
    switch (c.type)
    {
        case ENGINE_LIGHT_TYPE_POINT:
        {
            requires_update |= ImGui::DragFloat("Constant", &c.point.constant, 0.1f);
            requires_update |= ImGui::DragFloat("Linear", &c.point.linear, 0.1f);
            requires_update |= ImGui::DragFloat("Quadratic", &c.point.quadratic, 0.1f);
            break;
        }
        case ENGINE_LIGHT_TYPE_DIRECTIONAL:
        {
            requires_update |= ImGui::DragFloat3("Direction", c.directional.direction, 0.1f);
            break;
        }
        case ENGINE_LIGHT_TYPE_SPOT:
        {
            requires_update |= ImGui::DragFloat3("Direction", c.spot.direction, 0.1f);
            requires_update |= ImGui::DragFloat("CutOff", &c.spot.cut_off, 0.1f);
            requires_update |= ImGui::DragFloat("OuterCutOff", &c.spot.outer_cut_off, 0.1f);
            requires_update |= ImGui::DragFloat("Constant", &c.spot.constant, 0.02f);
            requires_update |= ImGui::DragFloat("Linear", &c.spot.linear, 0.02f);
            requires_update |= ImGui::DragFloat("Quadratic", &c.spot.quadratic, 0.02f);
            break;
        }
    }

    return requires_update;
}


bool display_material_component(engine::Scene* scene, engine_material_component_t& c)
{
    // pong is the only material type
    bool requires_component_updated = false;
    requires_component_updated |= ImGui::ColorEdit4("Diffuse Color", c.diffuse_color);
    requires_component_updated |= ImGui::SliderFloat("Shininess", &c.shininess, 0.0f, 128.0f);
    requires_component_updated |= display_texture_combo(scene, "Diffuse Texture", c.diffuse_texture);
    requires_component_updated |= display_texture_combo(scene, "Specular Texture", c.specular_texture);
    return requires_component_updated;
}

bool display_collider_component(engine::Scene* /*scene*/, engine_collider_component_t& c)
{
    bool requires_component_updated = false;
    // list of types
    const char* items[] = { "Box", "Sphere", "Compound" };
    std::int32_t selected_type = c.type;
    if (ImGui::ListBox("Type", &selected_type, items, std::size(items)))
    {
        requires_component_updated = true;
        c.type = static_cast<engine_collider_type_t>(selected_type);
        
        // set some valid values when changeed the type
        std::memset(&c.collider, 0, sizeof(c.collider));
        if (c.type == ENGINE_COLLIDER_TYPE_BOX)
        {
            c.collider.box.size[0] = 1.0f;
            c.collider.box.size[1] = 1.0f;
            c.collider.box.size[2] = 1.0f;
        }
        else if (c.type == ENGINE_COLLIDER_TYPE_SPHERE)
        {
            c.collider.sphere.radius = 1.0f;
        }
        else if (c.type == ENGINE_COLLIDER_TYPE_COMPOUND)
        {
            auto& child = c.collider.compound.children[0];

            // important to set valid rotation quaternion!
            child.rotation_quaternion[3] = 1.0f;

            child.type = ENGINE_COLLIDER_TYPE_BOX;
            child.collider.box.size[0] = 1.0f;
            child.collider.box.size[1] = 1.0f;
            child.collider.box.size[2] = 1.0f;
        }
    }
    // data for concrete type
    if (c.type == ENGINE_COLLIDER_TYPE_BOX)
    {
        requires_component_updated |= ImGui::DragFloat3("Size", c.collider.box.size, 0.1f);
    }
    else if (c.type == ENGINE_COLLIDER_TYPE_SPHERE)
    {
        requires_component_updated |= ImGui::DragFloat("Radius", &c.collider.sphere.radius, 0.1f);
    }
    else if (c.type == ENGINE_COLLIDER_TYPE_COMPOUND)
    {
        for (std::size_t i = 0; i < ENGINE_COMPOUND_COLLIDER_MAX_CHILD_COLLIDERS; i++)
        {
            auto& child = c.collider.compound.children[i];
            ImGui::Text("Child %zu", i);
            std::int32_t child_selected_type = child.type;
            const char* items_child[] = { "Box", "Sphere" };
            if (ImGui::ListBox("Child Type", &child_selected_type, items_child, std::size(items_child)))
            {
                requires_component_updated = true;
                child.type = static_cast<engine_collider_type_t>(child_selected_type);
            }
            if (child.type == ENGINE_COLLIDER_TYPE_BOX)
            {
                requires_component_updated |= ImGui::DragFloat3("Size", child.collider.box.size, 0.1f);
            }
            else if (child.type == ENGINE_COLLIDER_TYPE_SPHERE)
            {
                requires_component_updated |= ImGui::DragFloat("Radius", &child.collider.sphere.radius, 0.1f);
            }
            requires_component_updated |= ImGui::DragFloat3("Position##collider", child.transform, 0.1f);

            glm::vec3 rot = glm::degrees(glm::eulerAngles(glm::make_quat(child.rotation_quaternion)));
            if (ImGui::DragFloat3("Rotation##collider", glm::value_ptr(rot), 0.1f))
            {
                requires_component_updated = true;
                const auto final_rot = glm::normalize(glm::quat(glm::radians(rot)));
                std::memcpy(child.rotation_quaternion, glm::value_ptr(final_rot), sizeof(child.rotation_quaternion));
            }
        }
    }
    // trigger
    requires_component_updated |= ImGui::Checkbox("Is Trigger", &c.is_trigger);
    requires_component_updated |= ImGui::Checkbox("Disable", &c.disable);
    // bouncies
    requires_component_updated |= ImGui::DragFloat("Bounciness", &c.bounciness, 0.1f);
    // friction
    requires_component_updated |= ImGui::DragFloat("Friction", &c.friction_static, 0.1f);
    return requires_component_updated;
}

bool display_rigidbody_component(engine::Scene* /*scene*/, engine_rigid_body_component_t& c)
{
    const char* types[] = { "Dynamic", "Kinematic", "Static" };
    std::int32_t type = c.type;
    bool requires_update = false;
    if (ImGui::Combo("Type##rigid_body", &type, types, static_cast<int>(std::size(types))))
    {
        c.type = static_cast<engine_rigid_body_type_t>(type);
        requires_update = true;
    }
    if (c.type == ENGINE_RIGID_BODY_TYPE_DYNAMIC)
    {
        requires_update |= ImGui::SliderFloat("Mass", &c.mass, 0.0f, 100.0f);
    }
    return requires_update;
}

bool display_character_controller_component(engine::Scene* /*scene*/, engine_character_controller_component_t& c)
{
    bool changed = false;
    changed |= ImGui::DragFloat("Radius##character_controller", &c.radius, 0.01f, 0.01f, 10.0f);
    changed |= ImGui::DragFloat("Height##character_controller", &c.height, 0.01f, 0.01f, 20.0f);
    changed |= ImGui::DragFloat("Step height##character_controller", &c.step_height, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("Max slope##character_controller", &c.max_slope, 0.5f, 0.0f, 90.0f);
    return changed;
}

bool display_nav_surface_component(engine::Scene* /*scene*/, engine_nav_surface_component_t& c)
{
    // takes effect with the next nav mesh build ("Build nav mesh" below the component)
    bool changed = false;
    changed |= ImGui::DragFloat("Agent radius", &c.agent_radius, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("Agent height", &c.agent_height, 0.01f, 0.01f, 10.0f);
    changed |= ImGui::DragFloat("Agent max slope", &c.agent_max_slope, 0.5f, 0.0f, 90.0f);
    changed |= ImGui::DragFloat("Agent max climb", &c.agent_max_climb, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("Cell size", &c.cell_size, 0.005f, 0.01f, 1.0f);
    changed |= ImGui::DragFloat("Cell height", &c.cell_height, 0.005f, 0.01f, 1.0f);
    return changed;
}

bool display_nav_modifier_component(engine::Scene* /*scene*/, engine_nav_modifier_component_t& c)
{
    const char* types[] = { "Walkable", "Not walkable", "Ignore" };
    std::int32_t type = c.type;
    if (ImGui::Combo("Type##nav_modifier", &type, types, static_cast<int>(std::size(types))))
    {
        c.type = static_cast<engine_nav_modifier_type_t>(type);
        return true;
    }
    return false;
}

bool display_camera_component(engine::Scene* /*scene*/, engine_camera_component_t& c)
{
    bool update_required = false;
    // is it enabled?
    update_required |= ImGui::Checkbox("Enabled", &c.enabled);

    // type
    const char* items[] = { "Orthographic",  "Perspective" };
    std::int32_t selected_type = c.type;
    if (ImGui::ListBox("Type", &selected_type, items, std::size(items)))
    {
        update_required = true;
        c.type = static_cast<engine_camera_projection_type_t>(selected_type);
    }

    // fov or scale, based on type
    if (c.type == ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE)
    {
        update_required |= ImGui::DragFloat("FOV", &c.type_union.perspective_fov, 0.1f);
    }
    else
    {
        update_required |= ImGui::DragFloat("Scale", &c.type_union.orthographics_scale, 0.1f);
    }

    // target
    update_required |= ImGui::DragFloat3("Target", c.target, 0.1f);

    //viewport rect
    update_required |= ImGui::DragFloat4("Viewport", &c.viewport_rect.x, 0.1f);

    // pitch, yaw, roll
    update_required |= ImGui::DragFloat("Pitch", &c.pitch, 0.1f);
    update_required |= ImGui::DragFloat("Yaw", &c.yaw, 0.1f);
    update_required |= ImGui::DragFloat("Roll", &c.roll, 0.1f);

    // clip planes
    update_required |= ImGui::DragFloat("Near Clip Plane", &c.clip_plane_near, 0.1f);
    update_required |= ImGui::DragFloat("Far Clip Plane", &c.clip_plane_far, 0.1f);
    return update_required;
}

const auto GIZMO_OPERATION = ImGuizmo::OPERATION::TRANSLATE | ImGuizmo::OPERATION::SCALE | ImGuizmo::OPERATION::ROTATE;

// The gizmo handle under the cursor, picked in the order ImGuizmo::Manipulate picks them (move, scale, rotate); read
// right after Manipulate drew the game object's gizmo, while it isn't dragged.
std::optional<engine::ModalTransformMode> get_hovered_gizmo_handle()
{
    if (ImGuizmo::IsOver(ImGuizmo::OPERATION::TRANSLATE))
    {
        return engine::ModalTransformMode::eGrab;
    }
    // asked with the move handles, so the scale handles are where the combined gizmo draws them
    if (ImGuizmo::IsOver(ImGuizmo::OPERATION::TRANSLATE | ImGuizmo::OPERATION::SCALE))
    {
        return engine::ModalTransformMode::eScale;
    }
    if (ImGuizmo::IsOver(ImGuizmo::OPERATION::ROTATE))
    {
        return engine::ModalTransformMode::eRotate;
    }
    return std::nullopt;
}

// ImGuizmo's snap values for dragging a handle: a step per axis when moving, the first value when rotating (degrees)
// or scaling
std::array<float, 3> get_gizmo_snap_values(engine::ModalTransformMode handle, const engine::editor_snap_steps_t& steps)
{
    switch (handle)
    {
    case engine::ModalTransformMode::eGrab:
        return { steps.translate, steps.translate, steps.translate };
    case engine::ModalTransformMode::eRotate:
        return { steps.rotate, steps.rotate, steps.rotate };
    case engine::ModalTransformMode::eScale:
        return { steps.scale, steps.scale, steps.scale };
    }
    return {};
}

} // namespace anonymous


engine::ApplicationEditor::ApplicationEditor(const engine_application_create_desc_t& desc, RenderContext&& rdx, engine_result_code_t& out_code)
    : Application(desc, std::move(rdx), out_code)
{
    // hot reload of edited shaders
    FileWatcher::get_instance().start();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

    // Setup Platform/Renderer backends
    const auto init_sdl3 = ImGui_ImplSDL3_InitForOpenGL(rdx_.get_sdl_window(), rdx_.get_sdl_gl_context());
    assert(init_sdl3);
    const auto init_ogl = ImGui_ImplOpenGL3_Init();
    assert(init_ogl);

    // features of the same stage and order run in the order of registration: the outline is drawn on top of the grid
    grid_feature_ = std::make_shared<GridFeature>(rdx_);
    renderer_.add_feature(grid_feature_, RenderStage::ePostProcess);

    outline_feature_ = std::make_shared<OutlineFeature>();
    renderer_.add_feature(outline_feature_, RenderStage::ePostProcess);

    renderer_.add_feature(std::make_shared<CallbackRenderFeature>("Editor ImGui", [this](FrameContext& frame)
        {
            if (!render_imgui_)
            {
                return;
            }
            add_overlay_pass(frame, "ImGui", frame.backbuffer, [](rg::PassContext&)
                {
                    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                });
        }), RenderStage::eScreen);
}

engine::ApplicationEditor::~ApplicationEditor()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

bool engine::ApplicationEditor::is_game_view() const
{
    return editor_view_ == EditorView::eGame;
}

void engine::ApplicationEditor::on_frame_begine(const engine_application_frame_begine_info_t & frame_begin_info)
{
    frame_delta_time_ = frame_begin_info.delta_time;
    // Play and Stop pressed last frame, before anything of this frame runs: not during a scene update (the editor
    // camera has the game's cameras disabled then, the snapshot would keep them so) nor while the game's scripts run
    // when the scene was released since (a level exit, a restart), the request waits for the toolbar of the next one
    if (play_mode_request_ != PlayModeRequest::eNone && play_mode_scene_)
    {
        const auto request = std::exchange(play_mode_request_, PlayModeRequest::eNone);
        // a G / R / S preview isn't saved at Play nor kept at Stop: the game object goes back to where it was
        if (modal_transform_)
        {
            cancel_modal_transform(play_mode_scene_);
        }
        if (request == PlayModeRequest::ePlay)
        {
            enter_play_mode(*play_mode_scene_);
        }
        else
        {
            // this frame is the play session's last, the edit state comes back at its end (engineFrameEnd)
            exit_play_mode();
        }
    }
    // Save, Open or New asked for last frame, at the same point for the same reasons
    apply_scene_file_request();
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    // DockSpace
    const auto viewport = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpaceOverViewport(dockspace_id, viewport);
    if (!editor_windows_context_.is_initialized())
    {
        editor_windows_context_.initialize(dockspace_id);
    }
    // before the scenes update: their cameras use the size of the panel
    render_scene_view();
}

void engine::ApplicationEditor::on_frame_end()
{
    ImGui::Render();
    // without ImGui (F1) the scene is shown in the whole window
    renderer_.set_scene_output(render_imgui_ ? scene_view_texture_ : nullptr);
    if(ImGui::IsKeyPressed(ImGuiKey_F1))
    {
        render_imgui_ = !render_imgui_;
    }
    outline_feature_->set_enabled(editor_view_ == EditorView::eEditor);
    grid_feature_->set_enabled(draw_grid_ && editor_view_ == EditorView::eEditor);
}

void engine::ApplicationEditor::on_sdl_event(SDL_Event e)
{
    ImGui_ImplSDL3_ProcessEvent(&e);
}


void engine::ApplicationEditor::on_scene_update_pre(Scene* scene, float delta_time)
{
    ENGINE_PROFILE_SECTION_N("editor-on_scene_update_pre");
    scene_hierarchy_context_.resolve_selection(scene);
    if (mouse_consumed_ && !any_mouse_button_down(ImGui::GetIO()))
    {
        mouse_consumed_ = false;
    }
    if (editor_view_ == EditorView::eEditor)
    {
        // before the scene update, so this frame renders the preview
        handle_modal_transform(scene);
        const bool camera_accepts_input = !modal_transform_ && !mouse_consumed_;
        camera_context_.on_scene_update_pre(scene, is_playing() ? delta_time : frame_delta_time_, scene_hierarchy_context_.get_selected_entity(), camera_accepts_input);
    }
    else if (modal_transform_)
    {
        cancel_modal_transform(scene);
    }
}

void engine::ApplicationEditor::on_scene_update_post(Scene* scene, float)
{
    ENGINE_PROFILE_SECTION_N("editor-on_scene_update_post");
    const float editor_delta_time = frame_delta_time_;

    render_editor_controls(scene, editor_delta_time);
    render_scene_hierarchy_panel(scene, editor_delta_time);
    render_asset_browser_panel(scene);
    render_entity_properties_panel(scene, editor_delta_time);
    render_debug_panel(scene, editor_delta_time);

    if (!modal_transform_)
    {
        handle_asset_drop(scene);
    }
    if (editor_view_ == EditorView::eEditor)
    {
        if (modal_transform_)
        {
            draw_modal_transform(scene);
        }
        else if (!mouse_consumed_)
        {
            render_guizmo(scene);
            handle_mouse_picking(scene);
        }
    }
    handle_scene_view_context_menu(scene);
    // while a modal transform runs, Ctrl snaps and Z picks the axis: a combination pressed then (or still held when it
    // ends) isn't a new press
    const auto lctrl = keyboard_is_key_down(ENGINE_KEYBOARD_KEY_LCTRL);
    const auto lshift = keyboard_is_key_down(ENGINE_KEYBOARD_KEY_LSHIFT);
    const auto z = keyboard_is_key_down(ENGINE_KEYBOARD_KEY_Z);
    static bool ctrl_shift_z_prev = lctrl && lshift && z;
    const auto ctrl_shift_z_curr = lctrl && lshift && z;
    if (ctrl_shift_z_prev && !ctrl_shift_z_curr && !modal_transform_)
    {
        commands_.redo(*scene);
        scene_hierarchy_context_.sync_selected_from_scene(scene);
    }
    ctrl_shift_z_prev = ctrl_shift_z_curr;

    static bool ctrl_z_prev = lctrl && !lshift && z;
    const auto ctrl_z_curr = lctrl && !lshift && z;
    if (ctrl_z_curr && !ctrl_z_prev && !modal_transform_)
    {
        commands_.undo(*scene);
        scene_hierarchy_context_.sync_selected_from_scene(scene);
    }
    ctrl_z_prev = ctrl_z_curr;

    // duplicate the selection, in the editor view only (the game view gets the game's keys: D moves the player), and
    // not while typing into a text field
    static bool ctrl_d_prev = false;
    const auto ctrl_d_curr = lctrl && !lshift && keyboard_is_key_down(ENGINE_KEYBOARD_KEY_D);
    if (ctrl_d_curr && !ctrl_d_prev && editor_view_ == EditorView::eEditor && !ImGui::GetIO().WantTextInput && scene_hierarchy_context_.has_selected_entity())
    {
        duplicate_entity(scene_hierarchy_context_.get_selected_entity(), scene, commands_, scene_hierarchy_context_);
    }
    ctrl_d_prev = ctrl_d_curr;

    // F2 renames the selection in its hierarchy row, like Unity's and Godot's, from the hierarchy or the scene view
    if (editor_view_ == EditorView::eEditor && (hierarchy_focused_ || scene_view_focused_) && !ImGui::GetIO().WantTextInput
        && ImGui::IsKeyPressed(ImGuiKey_F2, false) && scene_hierarchy_context_.has_selected_entity())
    {
        scene_hierarchy_context_.request_rename(scene, scene_hierarchy_context_.get_selected_entity());
        scene_hierarchy_context_.set_forced_open_selected_parents(true);
    }
    // Delete deletes the selection, from the same places (a runtime object asks first)
    if (editor_view_ == EditorView::eEditor && (hierarchy_focused_ || scene_view_focused_) && !ImGui::GetIO().WantTextInput && !modal_transform_
        && ImGui::IsKeyPressed(ImGuiKey_Delete, false) && scene_hierarchy_context_.has_selected_entity())
    {
        delete_entity(scene_hierarchy_context_.get_selected_entity(), scene, commands_, scene_hierarchy_context_);
    }

    camera_context_.on_scene_update_post(scene, editor_delta_time);
}

void engine::ApplicationEditor::render_scene_view()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(editor_windows_context_.get_window_scene(), nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleVar();

    const auto pos = ImGui::GetCursorScreenPos();
    const auto size = ImGui::GetContentRegionAvail();
    const auto width = static_cast<std::uint32_t>(std::max(size.x, 1.0f));
    const auto height = static_cast<std::uint32_t>(std::max(size.y, 1.0f));
    if (!scene_view_texture_ || scene_view_rect_.width != width || scene_view_rect_.height != height)
    {
        scene_view_texture_ = std::make_shared<Texture2D>(width, height, false, nullptr, DataLayout::eRGBA_U8, TextureAddressClampMode::eClampToEdge);
    }
    scene_view_rect_ = viewport_t{ static_cast<std::uint32_t>(pos.x), static_cast<std::uint32_t>(pos.y), width, height };

    // the scene is rendered into the texture at the end of this frame; GL textures start at the bottom
    const auto image_max = ImVec2(pos.x + static_cast<float>(width), pos.y + static_cast<float>(height));
    ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(scene_view_texture_->get_imgui_texture_id()), pos, image_max, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    // a border in the toolbar's tint while playing or paused
    if (const auto play_mode = get_play_mode_state(); play_mode != ENGINE_PLAY_MODE_EDIT)
    {
        const auto tint = play_mode == ENGINE_PLAY_MODE_PAUSED ? PAUSED_TINT : PLAYING_TINT;
        const auto border = ImGui::GetColorU32(ImVec4(tint.x * 2.5f, tint.y * 2.5f, tint.z * 2.5f, 1.0f));
        ImGui::GetWindowDrawList()->AddRect(pos, image_max, border, 0.0f, 0, 3.0f);
    }
    // No item covers the image: ImGuizmo only starts a drag when no item is hovered.
    // The mouse stays with the scene while a button pressed over it is held.
    const bool hovered = ImGui::IsWindowHovered();
    const auto& io = ImGui::GetIO();
    const bool any_button_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
    if (hovered && any_button_clicked)
    {
        scene_view_mouse_held_ = true;
    }
    else if (!any_mouse_button_down(io))
    {
        scene_view_mouse_held_ = false;
    }
    scene_view_has_mouse_ = hovered || scene_view_mouse_held_;
    scene_view_focused_ = ImGui::IsWindowFocused();
    ImGui::End();
}

bool engine::ApplicationEditor::is_mouse_over_scene() const
{
    return !render_imgui_ || scene_view_has_mouse_;
}

engine::viewport_t engine::ApplicationEditor::get_scene_viewport() const
{
    if (!render_imgui_ || !scene_view_texture_)
    {
        return Application::get_scene_viewport();
    }
    return scene_view_rect_;
}

void engine::ApplicationEditor::handle_play_mode_shortcuts()
{
    // Unity's shortcuts: Ctrl+P plays and stops, Ctrl+Shift+P pauses and resumes, Ctrl+Alt+P steps
    const auto play_mode = get_play_mode_state();
    // nothing to play, pause or step in the frame a Stop ends the session in
    if (ImGui::GetIO().WantTextInput || play_mode == ENGINE_PLAY_MODE_EXITING)
    {
        return;
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_P))
    {
        play_mode_request_ = play_mode == ENGINE_PLAY_MODE_EDIT ? PlayModeRequest::ePlay : PlayModeRequest::eStop;
    }
    else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P) && play_mode != ENGINE_PLAY_MODE_EDIT)
    {
        set_play_mode_paused(play_mode == ENGINE_PLAY_MODE_PLAYING);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_P) && play_mode == ENGINE_PLAY_MODE_PAUSED)
    {
        step_play_mode();
    }
}

void engine::ApplicationEditor::render_editor_controls(class Scene* scene, float /*dt*/)
{
    play_mode_scene_ = scene;
    const auto play_mode = get_play_mode_state();
    // tinted while playing or paused, like Unity's play mode tint: edits made now are lost at Stop
    const bool in_play = play_mode != ENGINE_PLAY_MODE_EDIT;
    if (in_play)
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, play_mode == ENGINE_PLAY_MODE_PAUSED ? PAUSED_TINT : PLAYING_TINT);
    }
    ImGui::Begin(editor_windows_context_.get_window_up(), nullptr, ImGuiWindowFlags_MenuBar);
    if (in_play)
    {
        ImGui::PopStyleColor();
    }
    if (ImGui::BeginMenuBar())
    {
        render_file_menu(scene);
        ImGui::EndMenuBar();
    }

    if (ImGui::Selectable("Scene", editor_view_ == EditorView::eGame, ImGuiSelectableFlags_None, ImVec2(50, 0)))
    {
        editor_view_ = EditorView::eGame;
    }
    ImGui::SameLine();
    if(ImGui::Selectable("Editor", editor_view_ == EditorView::eEditor, ImGuiSelectableFlags_None, ImVec2(50, 0)))
    {
        editor_view_ = EditorView::eEditor;
    }
    ImGui::SameLine();
    // Play and Stop happen at the start of the next frame (on_frame_begine); nothing can be done in the frame a Stop
    // ends the session in
    const bool exiting = play_mode == ENGINE_PLAY_MODE_EXITING;
    ImGui::BeginDisabled(exiting);
    if (ImGui::Button(in_play ? "Stop" : "Play"))
    {
        play_mode_request_ = in_play ? PlayModeRequest::eStop : PlayModeRequest::ePlay;
    }
    ImGui::SetItemTooltip(in_play ? "Back to the scene as it was at Play (Ctrl+P)" : "Start a play session (Ctrl+P)");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!in_play || exiting);
    if (ImGui::Button(play_mode == ENGINE_PLAY_MODE_PAUSED ? "Resume" : "Pause"))
    {
        set_play_mode_paused(play_mode == ENGINE_PLAY_MODE_PLAYING);
    }
    ImGui::SetItemTooltip("Ctrl+Shift+P");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(play_mode != ENGINE_PLAY_MODE_PAUSED);
    if (ImGui::Button("Step"))
    {
        step_play_mode();
    }
    ImGui::SetItemTooltip("One frame (Ctrl+Alt+P)");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted(play_mode == ENGINE_PLAY_MODE_EDIT ? "Edit"
        : play_mode == ENGINE_PLAY_MODE_PAUSED ? "Paused"
        : exiting ? "Stopping" : "Playing");
    // the scene's file, * while it has unsaved changes (the edit state's, also while playing)
    ImGui::SameLine();
    ImGui::TextDisabled("|  %s%s", scene_display_name(*scene).c_str(), commands_.is_dirty() ? "*" : "");
    ImGui::End();
    handle_play_mode_shortcuts();
    handle_file_shortcuts(scene);
    render_scene_file_dialogs(scene);
    update_window_title(scene);
}

void engine::ApplicationEditor::render_scene_hierarchy_panel(Scene* scene, float /*dt*/)
{
    ENGINE_PROFILE_SECTION_N("editor-render_scene_hierarchy_panel");
    // build memory with all the entites
    std::map<entt::entity, entity_node_t> entity_map;
    for (auto e : scene->get_all_entities())
    {
        std::string name = "Unnamed";
        if (scene->has_component<engine_name_component_t>(e))
        {
            const auto nc = scene->get_component<engine_name_component_t>(e);
            name = nc->name;
        }
        entity_map.insert({ e, entity_node_t{ e, name, nullptr, {} } });
    }

    for (auto& [e, node] : entity_map)
    {
        if (scene->has_component<engine_parent_component_t>(e))
        {
            const auto& pc = scene->get_component<engine_parent_component_t>(e);
            const auto& e_parent = static_cast<entt::entity>(pc->parent);
            auto& parent_node = entity_map[e_parent];
            assert(node.parent == nullptr);
            node.parent = &parent_node;
            parent_node.children.push_back(&node);
        }
    }
    ImGui::Begin(editor_windows_context_.get_window_left());

    ImGui::SeparatorText("Scene options");
    if (ImGui::Button("Add entity"))
    {
        commands_.execute_command(*scene, std::make_unique<CommandAddEntity>());
    }

    bool phys_debug_draw_check = scene->physics().is_debug_draw_enabled();
    if (ImGui::Checkbox("Physics debug draw", &phys_debug_draw_check))
    {
        commands_.execute_command(*scene, std::make_unique<CommandSetPhysicsDebugDraw>(phys_debug_draw_check));
    }

    bool nav_mesh_debug_draw_check = scene->is_nav_mesh_debug_draw_enabled();
    if (ImGui::Checkbox("Nav mesh debug draw", &nav_mesh_debug_draw_check))
    {
        scene->enable_nav_mesh_debug_draw(nav_mesh_debug_draw_check);
    }

    ImGui::SeparatorText("Scene hierarchy");
    if (ImGui::TreeNodeEx("Scene Collection", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth))
    {

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY"))
            {
                IM_ASSERT(payload->DataSize == sizeof(entt::entity));
                entt::entity payload_n = *static_cast<entt::entity*>(payload->Data);
                //engine::log::log(engine::log::LogLevel::eInfo, "Dropped entity %d to %d\n", payload_n, node->entity);
                if (scene->has_component<engine_parent_component_t>(payload_n))
                {
                    scene->remove_component<engine_parent_component_t>(payload_n);
                    // unparenting has no undo step yet
                    commands_.mark_changed();
                }
            }
            ImGui::EndDragDropTarget();
        }

        // game objects without a parent are listed in their folders
        folder_node_t root_folder;
        for (auto& [e, node] : entity_map)
        {
            if (!node.parent)
            {
                get_or_create_folder(root_folder, get_folder_path(scene, e)).entities.push_back(&node);
            }
        }
        for (auto& [name, folder] : root_folder.folders)
        {
            display_folder(name, *folder, scene, commands_, scene_hierarchy_context_);
        }
        for (auto* node : root_folder.entities)
        {
            display_node(node, scene, commands_, scene_hierarchy_context_);
        }
        ImGui::TreePop();
    }
    // a click on the empty space below the game objects deselects everything, a file too, like Unity's hierarchy
    const auto empty_space = ImGui::GetContentRegionAvail();
    if (ImGui::InvisibleButton("##deselect_all", ImVec2(std::max(empty_space.x, 1.0f), std::max(empty_space.y, 1.0f))))
    {
        scene_hierarchy_context_.deselect_all(scene, commands_);
    }
    display_delete_without_undo_popup(scene);
    hierarchy_focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::End(); // scene panel
}

void engine::ApplicationEditor::display_delete_without_undo_popup(class Scene* scene)
{
    constexpr const char* popup = "Delete runtime object?";
    if (const auto requested = scene_hierarchy_context_.take_delete_without_undo_request(); requested != entt::null)
    {
        delete_without_undo_ = requested;
        ImGui::OpenPopup(popup);
    }
    if (!ImGui::BeginPopupModal(popup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    // gone meanwhile (e.g. destroyed by its script)
    if (!scene->is_valid_entity(delete_without_undo_))
    {
        delete_without_undo_ = entt::null;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const auto* name = scene->has_component<engine_name_component_t>(delete_without_undo_) ? scene->get_component<engine_name_component_t>(delete_without_undo_) : nullptr;
    ImGui::Text("\"%s\" is a runtime (transient) object: it isn't saved,", name ? name->name : "?");
    ImGui::TextUnformatted("so deleting it (and what is parented under it) can't be undone.");
    if (ImGui::Button("Delete"))
    {
        scene_hierarchy_context_.set_selected_entity(scene, entt::null, commands_);
        scene->destroy_entity(delete_without_undo_);
        delete_without_undo_ = entt::null;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        delete_without_undo_ = entt::null;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

namespace
{
// a component of the engine in the properties panel: drawn while the game object has it, offered by Add Component
// otherwise
struct engine_component_entry_t
{
    const char* name;
    const char* category;
    std::function<bool(const engine::Scene&, entt::entity)> has;
    std::function<void(engine::Scene&, entt::entity)> add;
    std::function<void(engine::Scene*, engine::CommandManager&, entt::entity)> draw;
};

template<typename T>
engine_component_entry_t make_engine_component_entry(const char* name, const char* category, bool (*fn)(engine::Scene*, T&))
{
    return engine_component_entry_t{ name, category,
        [](const engine::Scene& scene, entt::entity e) { return scene.has_component<T>(e); },
        [](engine::Scene& scene, entt::entity e) { scene.add_component<T>(e); },
        [name, fn](engine::Scene* scene, engine::CommandManager& commands, entt::entity e) { display_component<T>(name, scene, commands, e, fn); } };
}

// in the order the panel draws them, and the categories in the order Add Component lists them
const std::vector<engine_component_entry_t>& get_engine_component_entries()
{
    static const std::vector<engine_component_entry_t> entries = []()
    {
        std::vector<engine_component_entry_t> ret = {
            make_engine_component_entry<engine_folder_component_t>("Folder", "Scene", display_folder_component),
            make_engine_component_entry<engine_transform_component_t>("Transform", "Scene", display_transform_component),
            make_engine_component_entry<engine_light_component_t>("Light", "Rendering", display_light_component),
            make_engine_component_entry<engine_camera_component_t>("Camera", "Rendering", display_camera_component),
            make_engine_component_entry<engine_mesh_component_t>("Mesh", "Rendering", display_mesh_component),
            make_engine_component_entry<engine_skinned_mesh_component_t>("Skinned Mesh", "Rendering", display_skinned_mesh_component),
            make_engine_component_entry<engine_material_component_t>("Material", "Rendering", display_material_component),
            make_engine_component_entry<engine_skin_component_t>("Skin", "Animation", display_skin_component),
            make_engine_component_entry<engine_joint_attachment_component_t>("Joint Attachment", "Animation", display_joint_attachment_component),
            make_engine_component_entry<engine_animation_controller_component_t>("Animation controller", "Animation", display_animation_controller_component),
            make_engine_component_entry<engine_collider_component_t>("Collider", "Physics", display_collider_component),
            make_engine_component_entry<engine_rigid_body_component_t>("Rigid Body", "Physics", display_rigidbody_component),
            make_engine_component_entry<engine_character_controller_component_t>("Character Controller", "Physics", display_character_controller_component),
            make_engine_component_entry<engine_nav_surface_component_t>("Nav Surface", "Navigation", display_nav_surface_component),
            make_engine_component_entry<engine_nav_modifier_component_t>("Nav Modifier", "Navigation", display_nav_modifier_component),
        };
        // like a bake: nothing rebuilds the nav mesh automatically, so after editing the nav surface, nav modifiers or
        // static colliders, build it again to see (and play with) the result
        for (auto& entry : ret)
        {
            if (std::string_view(entry.name) == "Nav Surface")
            {
                entry.draw = [draw = entry.draw](engine::Scene* scene, engine::CommandManager& commands, entt::entity e)
                {
                    draw(scene, commands, e);
                    if (scene->has_component<engine_nav_surface_component_t>(e) && ImGui::Button("Build nav mesh"))
                    {
                        scene->build_nav_mesh();
                    }
                };
            }
        }
        return ret;
    }();
    return entries;
}

// the game's components have no hand-written panel: drawn from their type descriptions; one category for now
constexpr const char* GAME_COMPONENTS_CATEGORY = "Game";
}  // namespace

void engine::ApplicationEditor::display_add_component_button(Scene* scene, entt::entity entity)
{
    constexpr const char* popup = "##add_component";
    ImGui::Spacing();
    if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 0.0f)))
    {
        add_component_filter_.fill('\0');
        add_component_highlight_ = 0;
        ImGui::OpenPopup(popup);
    }
    if (!ImGui::BeginPopup(popup))
    {
        return;
    }
    // what the game object doesn't have yet: the engine's components, then the game's
    const auto& engine_entries = get_engine_component_entries();
    std::vector<component_choice_t> choices;
    std::vector<std::function<void()>> adds;
    for (const auto& entry : engine_entries)
    {
        if (!entry.has(*scene, entity))
        {
            choices.push_back(component_choice_t{ entry.name, entry.category });
            adds.push_back([this, scene, entity, &entry]()
                {
                    entry.add(*scene, entity);
                    // adding the engine's components has no undo step yet
                    commands_.mark_changed();
                });
        }
    }
    for (const auto* type : get_type_registry().get_components())
    {
        if (type->game_defined && !scene->has_component(*type, entity))
        {
            choices.push_back(component_choice_t{ type->name, GAME_COMPONENTS_CATEGORY });
            adds.push_back([this, scene, entity, type]()
                {
                    commands_.execute_command(*scene, std::make_unique<CommandAddTypedComponent>(*scene, entity, *type));
                });
        }
    }

    if (ImGui::IsWindowAppearing())
    {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##filter", "Search", add_component_filter_.data(), add_component_filter_.size()))
    {
        add_component_highlight_ = 0;
    }
    const auto groups = group_component_choices(choices, add_component_filter_.data());
    const auto listed = listed_component_choices(groups);
    // the arrow keys move the highlight through what is listed, Enter adds it (the search field keeps the focus)
    if (!listed.empty())
    {
        const auto count = static_cast<int>(listed.size());
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        {
            add_component_highlight_ = (add_component_highlight_ + 1) % count;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        {
            add_component_highlight_ = (add_component_highlight_ + count - 1) % count;
        }
        add_component_highlight_ = std::clamp(add_component_highlight_, 0, count - 1);
    }
    std::optional<std::size_t> picked;
    if (!listed.empty() && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
    {
        picked = listed[static_cast<std::size_t>(add_component_highlight_)];
    }
    if (listed.empty())
    {
        ImGui::TextDisabled(choices.empty() ? "It has every component" : "No component matches");
    }
    int position = 0;
    for (const auto& group : groups)
    {
        ImGui::SeparatorText(group.category.c_str());
        for (const auto choice : group.choices)
        {
            const bool highlighted = position++ == add_component_highlight_;
            ImGui::PushID(static_cast<int>(choice));
            if (ImGui::Selectable(choices[choice].name.c_str(), highlighted))
            {
                picked = choice;
            }
            if (highlighted)
            {
                ImGui::SetItemDefaultFocus();
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                {
                    ImGui::SetScrollHereY();
                }
            }
            ImGui::PopID();
        }
    }
    if (picked)
    {
        adds[*picked]();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void engine::ApplicationEditor::render_entity_properties_panel(class Scene* scene, float /*dt*/)
{
    ImGui::Begin(editor_windows_context_.get_window_right());
    // one thing is selected, like in Unity's inspector: a file or a game object (selecting one deselects the other,
    // CommandSetSelection)
    if (!scene_hierarchy_context_.has_selected_entity())
    {
        // the name field isn't drawn, so it is no longer edited
        name_field_active_ = false;
    }
    if (asset_browser_.has_selection())
    {
        ImGui::SeparatorText("Asset properties.");
        asset_browser_.draw_inspector();
    }
    else if (scene_hierarchy_context_.has_selected_entity())
    {
        ImGui::SeparatorText("Entity properties.");
        const auto selected = scene_hierarchy_context_.get_selected_entity();
        display_name_field(scene, selected);
        // like Unity's inspector: only the components it has, the others are added with Add Component
        for (const auto& entry : get_engine_component_entries())
        {
            if (entry.has(*scene, selected))
            {
                entry.draw(scene, commands_, selected);
            }
        }
        for (const auto* type : get_type_registry().get_components())
        {
            if (type->game_defined && scene->has_component(*type, selected))
            {
                display_typed_component(*type, scene, commands_, selected);
            }
        }
        display_add_component_button(scene, selected);
    }
    else
    {
        ImGui::SeparatorText("Entity properties.");
        ImGui::Text("Select entity to display its components.");
    }
    ImGui::End();
}

void engine::ApplicationEditor::display_name_field(Scene* scene, entt::entity entity)
{
    if (!name_field_active_)
    {
        // not being edited: shows the name as it is now (an undo or a script may have changed it)
        name_field_entity_ = entity;
        const auto* name = scene->has_component<engine_name_component_t>(entity) ? scene->get_component<engine_name_component_t>(entity) : nullptr;
        if (!name)
        {
            return;
        }
        std::strncpy(name_field_buffer_.data(), name->name, name_field_buffer_.size() - 1);
        name_field_buffer_.back() = '\0';
    }
    ImGui::InputText("Name", name_field_buffer_.data(), name_field_buffer_.size());
    name_field_active_ = ImGui::IsItemActive();
    // like the hierarchy's rename: Enter or clicking away renames it (one undo step), Escape gives the name back
    if (ImGui::IsItemDeactivated())
    {
        rename_entity(name_field_entity_, name_field_buffer_.data(), scene, commands_);
    }
}

void engine::ApplicationEditor::render_asset_browser_panel(Scene* scene)
{
    ENGINE_PROFILE_SECTION_N("editor-render_asset_browser_panel");
    ImGui::Begin(editor_windows_context_.get_window_down());
    if (const auto clicked = asset_browser_.draw())
    {
        // the file in place of the game object, one undo step which gives the game object back
        commands_.execute_command(*scene, std::make_unique<CommandSetSelection>(*scene, asset_browser_, scene_hierarchy_context_.get_selected_entity(), entt::null, *clicked));
        scene_hierarchy_context_.sync_selected_from_scene(scene);
    }
    // a scene file double-clicked: File > Open
    if (const auto open = asset_browser_.take_open_scene_request())
    {
        request_scene_file_action(scene, SceneFileAction::eOpen, *open);
    }
    ImGui::End();
}

void engine::ApplicationEditor::handle_asset_drop(Scene* scene)
{
    if (!render_imgui_)
    {
        return;
    }
    // the scene image covers the whole panel (Begin appends to the window)
    ImGui::Begin(editor_windows_context_.get_window_scene());
    const auto viewport = get_scene_viewport();
    const ImRect rect(static_cast<float>(viewport.x), static_cast<float>(viewport.y), static_cast<float>(viewport.x + viewport.width),
        static_cast<float>(viewport.y + viewport.height));
    if (ImGui::BeginDragDropTargetCustom(rect, ImGui::GetID("##scene_asset_drop")))
    {
        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ASSET_PAYLOAD_PREFAB);
        // what the scene is seen through: the editor camera in the editor view, the game's (its first enabled camera) in
        // the game view
        entt::entity camera_entity = editor_view_ == EditorView::eEditor ? camera_context_.get_camera_entity(scene) : entt::entity{ entt::null };
        if (editor_view_ == EditorView::eGame)
        {
            auto camera_view = scene->create_runtime_view();
            scene->attach_component_to_runtime_view<engine_camera_component_t>(camera_view);
            for (const auto entity : camera_view)
            {
                if (scene->get_component<engine_camera_component_t>(entity)->enabled)
                {
                    camera_entity = entity;
                    break;
                }
            }
        }
        if (payload && camera_entity != entt::null)
        {
            const auto position = get_mouse_scene_point(scene, camera_entity, get_scene_view_mouse());
            commands_.execute_command(*scene, std::make_unique<CommandPlacePrefab>(*scene, asset_browser_, static_cast<const char*>(payload->Data), position,
                scene_hierarchy_context_.get_selected_entity()));
            scene_hierarchy_context_.sync_selected_from_scene(scene);
            scene_hierarchy_context_.set_forced_open_selected_parents(true);
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::End();
}

void engine::ApplicationEditor::render_debug_panel(class Scene* scene, float dt)
{
    ImGui::Begin(editor_windows_context_.get_window_down_right());
    ImGui::SeparatorText("Debug options");

    ImGui::BeginDisabled(editor_view_ != EditorView::eEditor);
    const std::string guizmo_switch_title = draw_guizmo_ ? "Disable guizmo" : "Enable guizmo";
    if (ImGui::Button(guizmo_switch_title.c_str()))
    {
        draw_guizmo_ = !draw_guizmo_;
    }
    ImGui::Checkbox("Grid", &draw_grid_);
    if (auto* camera = camera_context_.get_camera(scene))
    {
        float fly_speed = camera->get_fly_speed();
        if (ImGui::DragFloat("Camera fly speed", &fly_speed, 0.1f, EditorCamera::MIN_FLY_SPEED, EditorCamera::MAX_FLY_SPEED, "%.1f m/s"))
        {
            camera->set_fly_speed(fly_speed);
        }
        ImGui::SetItemTooltip(
            "Middle drag / Alt + left drag: orbit\n"
            "Shift + middle drag: pan\n"
            "Wheel / Ctrl + middle drag: zoom\n"
            "Hold right button: fly (WASD, Q/E down/up, Shift faster, wheel: speed)\n"
            "F: frame selected\n"
            "Numpad 1/3/7: front/right/top view (Ctrl: opposite)");
    }
    ImGui::EndDisabled();

    const bool is_rml_ui_debugger_enabled = ui_manager_.is_ui_document_debugger_enabled();
    const std::string rmlui_switch_title = is_rml_ui_debugger_enabled ? "Disable Rml UI Debugger" : "Enable Rml UI Debugger";
    if (ImGui::Button(rmlui_switch_title.c_str()))
    {
        ui_manager_.enable_ui_document_debugger(!is_rml_ui_debugger_enabled);
    }
    assert(dt > 0.0f);
    static std::vector<float> fps_values(60);
    static std::size_t fps_idx = 0;
    fps_values[fps_idx % fps_values.size()] = 1000.0f / dt;
    fps_idx++;
    ImGui::Text("FPS: %d", static_cast<std::uint32_t>(std::accumulate(fps_values.begin(), fps_values.end(), 0) / (fps_idx < fps_values.size() ? fps_idx : fps_values.size())));
    // they are owned by their game objects: a number that keeps growing is a leak
    ImGui::Text("Animation controllers: %zu", engine::AnimationController::get_live_count());

    auto& render_settings = renderer_.get_settings();
    ImGui::SeparatorText("Shadows");
    {
        auto& shadows = render_settings.shadows;
        ImGui::Checkbox("Enabled##shadows", &shadows.enabled);
        static const std::array<std::uint32_t, 4> map_sizes = { 512, 1024, 2048, 4096 };
        if (ImGui::BeginCombo("Map size", std::to_string(shadows.map_size).c_str()))
        {
            for (const auto size : map_sizes)
            {
                if (ImGui::Selectable(std::to_string(size).c_str(), size == shadows.map_size))
                {
                    shadows.map_size = size;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::DragFloat("Distance", &shadows.distance, 0.5f, 1.0f, 1000.0f);
        ImGui::DragFloat("Caster distance", &shadows.caster_distance, 0.5f, 0.0f, 1000.0f);
        ImGui::DragFloat("Depth bias", &shadows.depth_bias, 0.0001f, 0.0f, 0.05f, "%.4f");
        ImGui::DragFloat("Normal offset (texels)", &shadows.normal_offset, 0.05f, 0.0f, 10.0f);
        int pcf_radius = static_cast<int>(shadows.pcf_radius);
        if (ImGui::SliderInt("PCF radius (0: hard)", &pcf_radius, 0, 4))
        {
            shadows.pcf_radius = static_cast<std::uint32_t>(pcf_radius);
        }
        ImGui::SliderFloat("Strength##shadows", &shadows.strength, 0.0f, 1.0f);
    }

    ImGui::SeparatorText("SSAO");
    {
        auto& ssao = render_settings.ssao;
        ImGui::Checkbox("Enabled##ssao", &ssao.enabled);
        ImGui::DragFloat("Radius", &ssao.radius, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Bias", &ssao.bias, 0.001f, 0.0f, 1.0f);
        ImGui::DragFloat("Power", &ssao.power, 0.05f, 0.1f, 8.0f);
        int sample_count = static_cast<int>(ssao.sample_count);
        if (ImGui::SliderInt("Samples", &sample_count, 1, static_cast<int>(SsaoSettings::MAX_SAMPLES)))
        {
            ssao.sample_count = static_cast<std::uint32_t>(sample_count);
        }
        ImGui::Checkbox("Blur", &ssao.blur);
        ImGui::SliderFloat("Direct light occlusion", &ssao.direct_light_occlusion, 0.0f, 1.0f);
    }

    {
        static const std::array<const char*, 3> debug_view_names = { "None", "Ambient occlusion", "Shadow" };
        int debug_view = static_cast<int>(render_settings.debug_view);
        if (ImGui::Combo("Debug view", &debug_view, debug_view_names.data(), static_cast<int>(debug_view_names.size())))
        {
            render_settings.debug_view = static_cast<RenderDebugView>(debug_view);
        }
    }

    ImGui::SeparatorText("Render graph (last frame)");
    const auto& render_graph = renderer_.get_last_frame_graph();
    for (const auto& pass : render_graph.get_pass_infos())
    {
        if (pass.culled)
        {
            ImGui::TextDisabled("%s (culled)", pass.name.c_str());
        }
        else
        {
            ImGui::BulletText("%s", pass.name.c_str());
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            for (const auto& name : pass.reads)
            {
                ImGui::Text("reads: %s", name.c_str());
            }
            for (const auto& name : pass.writes)
            {
                ImGui::Text("writes: %s", name.c_str());
            }
            ImGui::EndTooltip();
        }
    }
    ImGui::End();
}

void engine::ApplicationEditor::render_guizmo(Scene* scene)
{
    if (!draw_guizmo_)
    {
        return;
    }
    // drawn on top of the scene image, in its panel (Begin appends to the window)
    ImGui::Begin(editor_windows_context_.get_window_scene());
    ImGuizmo::SetDrawlist();
    const auto scene_viewport = get_scene_viewport();
    ImGuizmo::SetRect(static_cast<float>(scene_viewport.x), static_cast<float>(scene_viewport.y), static_cast<float>(scene_viewport.width), static_cast<float>(scene_viewport.height));

    auto camera_view = scene->create_runtime_view();
    scene->attach_component_to_runtime_view<engine_camera_component_t>(camera_view);
    for (const auto& camera_entity : camera_view)
    {
        const auto camera = *scene->get_component<engine_camera_component_t>(camera_entity);
        if (!camera.enabled)
        {
            continue;
        }
        const auto camera_view = scene->get_camera_view(camera_entity);
        const auto camera_projection = scene->get_camera_projection(camera_entity);

        auto gizmo_view = scene->create_runtime_view();
        scene->attach_component_to_runtime_view<engine_transform_component_t>(gizmo_view);
        scene->attach_component_to_runtime_view<engine::guizmo_component_t>(gizmo_view);
        for (const auto& entity : gizmo_view)
        {
            // ImGuizmo manipulates the world matrix, the transform component is local to the parent space
            auto model_matrix = scene->get_component<engine::world_transform_internal_component_t>(entity)->local_to_world;

            // Ctrl snaps like G / R / S; ImGuizmo snaps every handle by the same values, so they are the steps of the
            // handle the drag started on (the one under the cursor the frame before, when ImGuizmo started it)
            std::array<float, 3> snap_values{};
            const float* snap = nullptr;
            if (ImGui::GetIO().KeyCtrl && entity == gizmo_hovered_entity_ && gizmo_hovered_handle_)
            {
                snap_values = get_gizmo_snap_values(*gizmo_hovered_handle_, snap_steps_);
                snap = snap_values.data();
            }

            ImGuizmo::SetOrthographic(camera.type == ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC); // Set the projection mode to perspective
            ImGuizmo::Manipulate(glm::value_ptr(camera_view), glm::value_ptr(camera_projection),
                GIZMO_OPERATION,
                ImGuizmo::MODE::LOCAL,
                glm::value_ptr(model_matrix), nullptr, snap);

            if (!ImGuizmo::IsUsing())
            {
                if (const auto handle = get_hovered_gizmo_handle())
                {
                    gizmo_hovered_entity_ = entity;
                    gizmo_hovered_handle_ = handle;
                }
                else if (entity == gizmo_hovered_entity_)
                {
                    gizmo_hovered_entity_ = entt::null;
                    gizmo_hovered_handle_.reset();
                }
            }

            if (ImGuizmo::IsUsing())
            {
                auto c = *scene->get_component<engine_transform_component_t>(entity);
                if (engine::local_transform_from_world(scene->get_parent_space_matrix(entity), model_matrix, c))
                {
                    commands_.execute_command(*scene, std::make_unique<CommandUpdateComponent<engine_transform_component_t>>(*scene, entity, c));
                }
            }
        }
    }
    ImGui::End();
}

void engine::ApplicationEditor::handle_mouse_picking(Scene* scene)
{
    // a click on the gizmo moves the object, it doesn't pick what is under it
    if (!is_mouse_over_scene() || ImGuizmo::IsUsingAny() || mouse_consumed_)
    {
        return;
    }
    // Alt + left mouse button orbits the editor camera instead (either Alt, like the camera reads it)
    if (mouse_is_button_down(ENGINE_MOUSE_BUTTON_LEFT) && !ImGui::GetIO().KeyAlt)
    {
        const auto picked = pick_entity_under_mouse();
        if (!picked)
        {
            return;
        }
        const auto entity_id = *picked;
        if (entity_id == entt::null)
        {
            // empty space, like Unity: nothing stays selected, a file neither
            scene_hierarchy_context_.deselect_all(scene, commands_);
            return;
        }
        scene_hierarchy_context_.set_selected_entity(scene, entity_id, commands_);
        scene_hierarchy_context_.set_forced_open_selected_parents(true);
    }
}

std::optional<entt::entity> engine::ApplicationEditor::pick_entity_under_mouse()
{
    const auto scene_viewport = get_scene_viewport();
    const auto mouse_coords = mouse_get_coords();
    if (mouse_coords.x < 0.0f || mouse_coords.y < 0.0f)
    {
        return std::nullopt;  // held since a click over the scene, but moved off it
    }
    // entity ids come from the previous frame, which is what was visible on the screen when the user clicked
    std::uint32_t picked_id = ENGINE_INVALID_GAME_OBJECT_ID;
    const auto x = static_cast<std::uint32_t>(mouse_coords.x * scene_viewport.width);
    const auto y = static_cast<std::uint32_t>(mouse_coords.y * scene_viewport.height);
    if (!renderer_.read_entity_id(x, y, picked_id))
    {
        return std::nullopt;
    }
    return picked_id == ENGINE_INVALID_GAME_OBJECT_ID ? entt::entity{ entt::null } : static_cast<entt::entity>(picked_id);
}

glm::vec2 engine::ApplicationEditor::get_scene_view_mouse() const
{
    const auto& io = ImGui::GetIO();
    const auto viewport = get_scene_viewport();
    return glm::vec2(io.MousePos.x - static_cast<float>(viewport.x), io.MousePos.y - static_cast<float>(viewport.y));
}

glm::vec3 engine::ApplicationEditor::get_mouse_scene_point(Scene* scene, entt::entity camera_entity, const glm::vec2& mouse) const
{
    const auto viewport = get_scene_viewport();
    const modal_transform_view_t view{ scene->get_camera_view(camera_entity), scene->get_camera_projection(camera_entity),
        glm::vec2(static_cast<float>(viewport.width), static_cast<float>(viewport.height)) };
    glm::vec3 origin{};
    glm::vec3 direction{};
    mouse_ray(view, mouse, origin, direction);
    engine_ray_hit_t hit{};
    // nothing beyond the far plane: it couldn't be seen there
    const float max_distance = scene->get_component<engine_camera_component_t>(camera_entity)->clip_plane_far;
    return scene->physics().raycast(origin, direction, max_distance, {}, false, hit)
        ? glm::vec3(hit.position[0], hit.position[1], hit.position[2]) : get_ground_point(origin, direction, max_distance);
}

void engine::ApplicationEditor::handle_scene_view_context_menu(Scene* scene)
{
    constexpr const char* popup = "##scene_view_context_menu";
    const bool can_open = render_imgui_ && editor_view_ == EditorView::eEditor && !modal_transform_ && !mouse_consumed_
        && is_mouse_over_scene() && !ImGuizmo::IsUsingAny() && !gizmo_hovered_handle_;
    // fed every frame, so a press which started while it couldn't open isn't taken for a click later
    const bool clicked = context_click_.update(read_editor_camera_input(), can_open);
    // Begin appends to the scene view's window, so the popup belongs to it
    ImGui::Begin(editor_windows_context_.get_window_scene());
    if (clicked && can_open)
    {
        // like Unreal's: right-clicking a game object which isn't selected selects it; empty space keeps the selection
        if (const auto picked = pick_entity_under_mouse(); picked && *picked != entt::null && *picked != scene_hierarchy_context_.get_selected_entity())
        {
            scene_hierarchy_context_.set_selected_entity(scene, *picked, commands_);
            scene_hierarchy_context_.set_forced_open_selected_parents(true);
        }
        context_menu_mouse_ = get_scene_view_mouse();
        ImGui::OpenPopup(popup);
    }
    if (ImGui::BeginPopup(popup))
    {
        const auto selected = scene_hierarchy_context_.get_selected_entity();
        if (selected != entt::null && scene->is_valid_entity(selected))
        {
            if (const auto* name = scene->has_component<engine_name_component_t>(selected) ? scene->get_component<engine_name_component_t>(selected) : nullptr)
            {
                ImGui::SeparatorText(name->name);
            }
            if (ImGui::MenuItem("Frame selected", "F"))
            {
                camera_context_.frame_entity(scene, selected);
            }
            display_game_object_menu_items(selected, scene, commands_, scene_hierarchy_context_);
            ImGui::Separator();
            const bool can_transform = scene->has_component<engine_transform_component_t>(selected);
            if (ImGui::MenuItem("Move", "G", false, can_transform))
            {
                start_modal_transform(scene, ModalTransformMode::eGrab);
            }
            if (ImGui::MenuItem("Rotate", "R", false, can_transform))
            {
                start_modal_transform(scene, ModalTransformMode::eRotate);
            }
            if (ImGui::MenuItem("Scale", "S", false, can_transform))
            {
                start_modal_transform(scene, ModalTransformMode::eScale);
            }
        }
        else if (ImGui::MenuItem("Create empty game object here"))
        {
            // where the menu was opened: on what was clicked, or the ground
            const auto camera_entity = camera_context_.get_camera_entity(scene);
            commands_.execute_command(*scene, std::make_unique<CommandAddEntity>(camera_entity == entt::null ? glm::vec3(0.0f)
                : get_mouse_scene_point(scene, camera_entity, context_menu_mouse_)));
        }
        ImGui::EndPopup();
    }
    ImGui::End();
}

void engine::ApplicationEditor::start_modal_transform(Scene* scene, ModalTransformMode mode)
{
    const auto entity = scene_hierarchy_context_.get_selected_entity();
    const auto camera_entity = camera_context_.get_camera_entity(scene);
    if (modal_transform_ || entity == entt::null || camera_entity == entt::null || !scene->is_valid_entity(entity)
        || !scene->has_component<engine_transform_component_t>(entity) || !scene->has_component<world_transform_internal_component_t>(entity))
    {
        return;
    }
    const auto viewport = get_scene_viewport();
    const modal_transform_view_t view{ scene->get_camera_view(camera_entity), scene->get_camera_projection(camera_entity),
        glm::vec2(static_cast<float>(viewport.width), static_cast<float>(viewport.height)) };
    modal_transform_.emplace(mode, scene->get_component<world_transform_internal_component_t>(entity)->local_to_world, get_scene_view_mouse(), view, snap_steps_);
    modal_transform_entity_ = entity;
    modal_transform_original_ = *scene->get_component<engine_transform_component_t>(entity);
}

void engine::ApplicationEditor::handle_modal_transform(Scene* scene)
{
    const auto& io = ImGui::GetIO();
    const auto viewport = get_scene_viewport();
    const glm::vec2 mouse(io.MousePos.x - static_cast<float>(viewport.x), io.MousePos.y - static_cast<float>(viewport.y));

    const auto pressed_mode = []() -> std::optional<ModalTransformMode>
    {
        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) return ModalTransformMode::eGrab;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) return ModalTransformMode::eRotate;
        if (ImGui::IsKeyPressed(ImGuiKey_S, false)) return ModalTransformMode::eScale;
        return std::nullopt;
    };

    if (!modal_transform_)
    {
        // like Blender: the keys act on the view under the mouse; Ctrl / Alt combinations are left for other shortcuts
        if (!is_mouse_over_scene() || io.WantTextInput || io.KeyCtrl || io.KeyAlt || any_mouse_button_down(io) || ImGuizmo::IsUsingAny())
        {
            return;
        }
        if (const auto mode = pressed_mode())
        {
            start_modal_transform(scene, *mode);
        }
        return;
    }

    const auto entity = modal_transform_entity_;
    if (!scene->is_valid_entity(entity) || !scene->has_component<engine_transform_component_t>(entity))
    {
        modal_transform_.reset();
        return;
    }
    // Like Blender's modal operators, it owns the mouse buttons wherever the mouse is: the panels' widgets don't see
    // the clicks, including the one that ends it (locked until it is released). Set each frame, before the panels.
    const ImGuiID owner = ImHashStr("editor_modal_transform");
    ImGui::SetKeyOwner(ImGuiKey_MouseLeft, owner, ImGuiInputFlags_LockUntilRelease);
    ImGui::SetKeyOwner(ImGuiKey_MouseRight, owner, ImGuiInputFlags_LockUntilRelease);
    const bool left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left, ImGuiInputFlags_None, owner);
    const bool right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right, ImGuiInputFlags_None, owner);

    if (right_clicked || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        mouse_consumed_ = right_clicked;
        cancel_modal_transform(scene);
        return;
    }

    if (const auto mode = pressed_mode())
    {
        modal_transform_->set_mode(*mode, mouse);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_X, false)) modal_transform_->toggle_axis(ModalTransformAxis::eX);
    if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) modal_transform_->toggle_axis(ModalTransformAxis::eY);
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) modal_transform_->toggle_axis(ModalTransformAxis::eZ);

    // previewed on the component itself, so physics, children and the render see it; only the result is a command
    const auto world = modal_transform_->update(mouse, io.KeyCtrl);
    auto transform = *scene->get_component<engine_transform_component_t>(entity);
    if (local_transform_from_world(scene->get_parent_space_matrix(entity), world, transform))
    {
        scene->update_component<engine_transform_component_t>(entity, transform);
    }

    if (left_clicked || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
    {
        mouse_consumed_ = left_clicked;
        auto result = *scene->get_component<engine_transform_component_t>(entity);
        // the command records the transform from before the preview, so undo goes back there
        scene->update_component<engine_transform_component_t>(entity, modal_transform_original_);
        commands_.execute_command(*scene, std::make_unique<CommandUpdateComponent<engine_transform_component_t>>(*scene, entity, result));
        modal_transform_.reset();
    }
}

void engine::ApplicationEditor::cancel_modal_transform(Scene* scene)
{
    if (scene->is_valid_entity(modal_transform_entity_) && scene->has_component<engine_transform_component_t>(modal_transform_entity_))
    {
        scene->update_component<engine_transform_component_t>(modal_transform_entity_, modal_transform_original_);
    }
    modal_transform_.reset();
}

void engine::ApplicationEditor::draw_modal_transform(Scene* /*scene*/)
{
    const auto& transform = *modal_transform_;
    const auto viewport = get_scene_viewport();
    const ImVec2 offset(static_cast<float>(viewport.x), static_cast<float>(viewport.y));
    const auto to_screen = [&offset](const glm::vec2& p) { return ImVec2(p.x + offset.x, p.y + offset.y); };

    ImGui::Begin(editor_windows_context_.get_window_scene());
    auto* draw_list = ImGui::GetWindowDrawList();
    const auto center = transform.get_center_on_screen();

    static constexpr std::array<ImU32, 3> axis_colors = { IM_COL32(230, 70, 70, 255), IM_COL32(110, 200, 60, 255), IM_COL32(70, 130, 240, 255) };
    static constexpr std::array<const char*, 3> axis_names = { "X", "Y", "Z" };
    const auto axis = transform.get_axis();
    if (axis != ModalTransformAxis::eNone)
    {
        // the axis through the object, across the whole view
        const auto index = static_cast<std::size_t>(axis) - 1;
        auto direction = transform.project(transform.get_origin() + transform.get_axis_direction()) - center;
        if (glm::length(direction) > 1e-3f)
        {
            direction = glm::normalize(direction) * static_cast<float>(viewport.width + viewport.height);
            draw_list->AddLine(to_screen(center - direction), to_screen(center + direction), axis_colors[index], 1.5f);
        }
    }
    const auto& io = ImGui::GetIO();
    if (transform.get_mode() != ModalTransformMode::eGrab)
    {
        draw_list->AddLine(to_screen(center), io.MousePos, IM_COL32(255, 255, 255, 160), 1.0f);
    }
    draw_list->AddCircleFilled(to_screen(center), 3.0f, IM_COL32(255, 255, 255, 220));

    // the header, like Blender's
    std::string text;
    const auto value = transform.get_value();
    switch (transform.get_mode())
    {
    case ModalTransformMode::eGrab: text = fmt::format("Move  {:.2f}, {:.2f}, {:.2f}", value.x, value.y, value.z); break;
    case ModalTransformMode::eRotate: text = fmt::format("Rotate  {:.1f} deg", value.x); break;
    case ModalTransformMode::eScale: text = fmt::format("Scale  {:.3f}", value.x); break;
    }
    if (axis != ModalTransformAxis::eNone)
    {
        text += fmt::format("  along {} ({})", axis_names[static_cast<std::size_t>(axis) - 1], transform.is_local() ? "local" : "global");
    }
    const char* help = "LMB / Enter: confirm   RMB / Esc: cancel   X Y Z: axis (twice: local)   Ctrl: snap   G R S: switch";
    const ImVec2 text_pos(offset.x + 8.0f, offset.y + 8.0f);
    const auto line_height = ImGui::GetTextLineHeightWithSpacing();
    const auto help_size = ImGui::CalcTextSize(help);
    draw_list->AddRectFilled(ImVec2(text_pos.x - 4.0f, text_pos.y - 4.0f), ImVec2(text_pos.x + help_size.x + 4.0f, text_pos.y + 2.0f * line_height + 2.0f), IM_COL32(0, 0, 0, 160), 3.0f);
    draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), text.c_str());
    draw_list->AddText(ImVec2(text_pos.x, text_pos.y + line_height), IM_COL32(200, 200, 200, 255), help);
    ImGui::End();
}

bool engine::ApplicationEditor::is_mouse_enabled()
{
    return editor_view_ == EditorView::eGame && is_mouse_over_scene();
}

bool engine::ApplicationEditor::is_keyboard_enabled()
{
    // the scene panel being focused is enough: clicking it makes ImGui want the keyboard while the button is held
    const bool scene_has_keyboard = !render_imgui_ || scene_view_focused_ || !ImGui::GetIO().WantCaptureKeyboard;
    return editor_view_ == EditorView::eGame && scene_has_keyboard;
}

void engine::ApplicationEditor::on_scene_create(Scene* scene)
{
    camera_context_.attach_scene(scene, this);
}

void engine::ApplicationEditor::on_scene_release(Scene* scene)
{
    camera_context_.detach_scene(scene);
    if (play_mode_scene_ == scene)
    {
        play_mode_scene_ = nullptr;
    }
    // persistent IDs are per scene: the steps and the selection would find other game objects in the next one. While
    // playing (the game replaced its scene: a restart, a level exit) only the play session's steps go: Stop loads the
    // scene saved at Play into the next scene, where the edit history finds its game objects again.
    if (get_play_mode_state() == ENGINE_PLAY_MODE_EDIT)
    {
        commands_.clear();
    }
    else
    {
        commands_.drop_play_session_steps();
    }
    scene_hierarchy_context_.reset();
}

void engine::ApplicationEditor::on_play_mode_enter(Scene* /*scene*/)
{
    commands_.begin_play_session();
}

void engine::ApplicationEditor::on_play_mode_exit(Scene* /*scene*/)
{
    // the steps recorded while playing would write play session values into the edit state; the ones from before Play
    // come back and find their game objects by persistent ID
    commands_.end_play_session();
    // what the editor held of the play session's game objects
    forget_game_objects();
}

engine::CameraScript::CameraScript(Scene* scene, ApplicationEditor* app)
    : my_scene_(scene)
    , app_(app)
    , go_(scene->create_transient_entity())
{
    // kept when the scene is cleared (Stop restoring the scene saved at Play)
    scene->add_component<editor_object_internal_component_t>(go_);
    scene->add_component<engine_name_component_t>(go_);
    scene->patch_component<engine_name_component_t>(go_, [](engine_name_component_t& c)
        {   
            constexpr const char* editor_camera_name = "__engine_editor_camera__";
            std::memcpy(c.name, editor_camera_name, std::strlen(editor_camera_name));
        });

    scene->add_component<engine_camera_component_t>(go_);
    const auto pivot = camera_.get_pivot();
    scene->patch_component<engine_camera_component_t>(go_, [pivot](engine_camera_component_t& c)
        {
            c.enabled = true;
            c.clip_plane_near = 0.1f;
            c.clip_plane_far = 2.0f * EditorCamera::MAX_DISTANCE;
            c.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
            c.type_union.perspective_fov = 45.0f;
            std::memcpy(c.target, glm::value_ptr(pivot), sizeof(c.target));
        });

    scene->add_component<engine_transform_component_t>(go_);
    const auto eye = camera_.get_eye();
    scene->patch_component<engine_transform_component_t>(go_, [eye](engine_transform_component_t& c)
        {
            std::memcpy(c.position, glm::value_ptr(eye), sizeof(c.position));
        });
}

void engine::CameraScript::enable()
{
    my_scene_->patch_component<engine_camera_component_t>(go_, [](engine_camera_component_t& c)
        {
            c.enabled = true;
        });
}

void engine::CameraScript::disable()
{
    my_scene_->patch_component<engine_camera_component_t>(go_, [](engine_camera_component_t& c)
        {
            c.enabled = false;
        });
}

void engine::CameraScript::update(float dt, entt::entity selected, bool accepts_input)
{
    //if not enabled - do nothing
    auto cc = my_scene_->get_component<engine_camera_component_t>(go_);
    if (!cc->enabled)
    {
        return;
    }

    // the mouse belongs to another panel, or to the gizmo
    const auto& io = ImGui::GetIO();
    if (accepts_input && app_->is_mouse_over_scene() && !ImGuizmo::IsUsingAny())
    {
        camera_.update(read_editor_camera_input(), dt, static_cast<float>(app_->get_scene_viewport().height), get_fov_y());

        // shortcuts while the mouse is over the scene, like Blender's
        if (!io.WantTextInput && !camera_.is_flying())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_F, false) && selected != entt::null)
            {
                frame_entity(selected);
            }
            // numpad views (Blender): 1 front, 3 right, 7 top, Ctrl for the opposite side
            const float opposite = io.KeyCtrl ? glm::pi<float>() : 0.0f;
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad1, false))
            {
                camera_.set_view(opposite, 0.0f);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Keypad3, false))
            {
                camera_.set_view(-glm::half_pi<float>() + opposite, 0.0f);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Keypad7, false))
            {
                camera_.set_view(0.0f, io.KeyCtrl ? EditorCamera::MAX_PITCH : -EditorCamera::MAX_PITCH);
            }
        }
    }
    else
    {
        // nothing to steer, but a glide toward a framed object finishes
        camera_.update(editor_camera_input_t{}, dt, static_cast<float>(app_->get_scene_viewport().height), get_fov_y());
    }

    // only when the camera moved: a transform update is synced to physics and recomputes the world transforms
    const auto eye = camera_.get_eye();
    const auto pivot = camera_.get_pivot();
    if (glm::make_vec3(my_scene_->get_component<engine_transform_component_t>(go_)->position) != eye)
    {
        my_scene_->patch_component<engine_transform_component_t>(go_, [eye](engine_transform_component_t& c)
            {
                std::memcpy(c.position, glm::value_ptr(eye), sizeof(c.position));
            });
    }
    if (glm::make_vec3(cc->target) != pivot)
    {
        my_scene_->patch_component<engine_camera_component_t>(go_, [pivot](engine_camera_component_t& c)
            {
                std::memcpy(c.target, glm::value_ptr(pivot), sizeof(c.target));
            });
    }
}

bool engine::CameraScript::frame_entity(entt::entity entity)
{
    if (!my_scene_->is_valid_entity(entity) || !my_scene_->has_component<world_transform_internal_component_t>(entity))
    {
        return false;
    }
    // no mesh bounds yet: the origins of the hierarchy, each with a sphere about as big as its scale
    glm::vec3 min(std::numeric_limits<float>::max());
    glm::vec3 max(std::numeric_limits<float>::lowest());
    float size = 0.0f;
    std::vector<entt::entity> pending{ entity };
    while (!pending.empty())
    {
        const auto e = pending.back();
        pending.pop_back();
        if (my_scene_->has_component<world_transform_internal_component_t>(e))
        {
            const auto& world = my_scene_->get_component<world_transform_internal_component_t>(e)->local_to_world;
            const auto position = glm::vec3(world[3]);
            min = glm::min(min, position);
            max = glm::max(max, position);
            size = std::max({ size, glm::length(glm::vec3(world[0])), glm::length(glm::vec3(world[1])), glm::length(glm::vec3(world[2])) });
        }
        const auto children = my_scene_->get_children(e);
        pending.insert(pending.end(), children.begin(), children.end());
    }
    camera_.frame((min + max) * 0.5f, glm::length(max - min) * 0.5f + size, get_fov_y());
    return true;
}

float engine::CameraScript::get_fov_y() const
{
    return glm::radians(my_scene_->get_component<engine_camera_component_t>(go_)->type_union.perspective_fov);
}

void engine::ApplicationEditor::CameraContext::attach_scene(Scene* scene, ApplicationEditor* app)
{
    if (cameras_.find(scene) == cameras_.end())
    {
        cameras_.insert({ scene, { true, CameraScript{ scene, app }, {} } });
    }
    else
    {
        assert(!"Something gone really worng - scene pointer already existed in cache.");
    }
}

void engine::ApplicationEditor::CameraContext::detach_scene(Scene* scene)
{
    if (cameras_.find(scene) == cameras_.end())
    {
        assert(!"Something gone really worng - scene is being released but it's pointer was not cached?");
    }
    else
    {
        cameras_.erase(scene);
    }
}

void engine::ApplicationEditor::CameraContext::on_scene_update_pre(Scene* scene, float dt, entt::entity selected, bool accepts_input)
{
    auto& camera_data = cameras_[scene];
    if (camera_data.is_enabled)
    {
        // disable all active scene cameras
        auto view = scene->create_runtime_view();
        scene->attach_component_to_runtime_view<engine_camera_component_t>(view);
        view.each([scene, &camera_data](const auto& entity)
            {
                const auto cc = scene->get_component<engine_camera_component_t>(entity);
                if (cc->enabled)
                {
                    camera_data.user_camera_entities_to_enable_back.push_back(entity);
                    scene->patch_component<engine_camera_component_t>(entity, [](auto& c) { c.enabled = false; });
                }
            });
        // enable and update the editor camera
        camera_data.camera.enable();
        camera_data.camera.update(dt, selected, accepts_input);
    }
}

void engine::ApplicationEditor::CameraContext::on_scene_update_post(Scene* scene, float /*dt*/)
{
    ENGINE_PROFILE_SECTION;
    for (auto& [script, camera_data] : cameras_)
    {
        camera_data.camera.disable();
        for (auto e : camera_data.user_camera_entities_to_enable_back)
        {
            scene->patch_component<engine_camera_component_t>(e, [](auto& c) { c.enabled = true; });
        }
        camera_data.user_camera_entities_to_enable_back.clear();
    }

}

bool engine::ApplicationEditor::CameraContext::is_enabled(engine::Scene* scene) const
{
    return cameras_.at(scene).is_enabled;
}

entt::entity engine::ApplicationEditor::CameraContext::get_camera_entity(engine::Scene* scene) const
{
    const auto it = cameras_.find(scene);
    return it == cameras_.end() ? entt::null : it->second.camera.get_entity();
}

void engine::ApplicationEditor::CameraContext::frame_entity(engine::Scene* scene, entt::entity entity)
{
    if (const auto it = cameras_.find(scene); it != cameras_.end())
    {
        it->second.camera.frame_entity(entity);
    }
}

engine::EditorCamera* engine::ApplicationEditor::CameraContext::get_camera(engine::Scene* scene)
{
    const auto it = cameras_.find(scene);
    return it == cameras_.end() ? nullptr : &it->second.camera.get_camera();
}

void engine::SceneHierarchyContext::set_selected_entity(engine::Scene* scene, entt::entity e, CommandManager& commands)
{
    if (selected_ == e)
    {
        return;
    }
    // while a game object is selected, no file is: selecting one (from none) deselects the file in the same undo step
    commands.execute_command(*scene, std::make_unique<engine::CommandSetSelection>(*scene, assets_, selected_, e, ""));
    selected_ = e;
    selected_ref_ = scene->make_ref(e);
}

void engine::SceneHierarchyContext::deselect_all(engine::Scene* scene, CommandManager& commands)
{
    if (selected_ == entt::null && assets_.get_selected_asset().empty())
    {
        return;
    }
    commands.execute_command(*scene, std::make_unique<engine::CommandSetSelection>(*scene, assets_, selected_, entt::null, ""));
    selected_ = entt::null;
    selected_ref_ = {};
}

entt::entity engine::SceneHierarchyContext::get_selected_entity() const
{
    return selected_;
}

bool engine::SceneHierarchyContext::has_selected_entity() const
{
    return selected_ != entt::null;
}

void engine::SceneHierarchyContext::set_forced_open_selected_parents(bool value)
{
    force_open_selected_parents_ = value;
}

bool engine::SceneHierarchyContext::is_forced_open_selected_parents() const
{
    return force_open_selected_parents_;
}

void engine::SceneHierarchyContext::request_delete_without_undo(entt::entity e)
{
    delete_without_undo_ = e;
}

entt::entity engine::SceneHierarchyContext::take_delete_without_undo_request()
{
    return std::exchange(delete_without_undo_, entt::entity{ entt::null });
}

void engine::SceneHierarchyContext::request_rename(const engine::Scene* scene, entt::entity e)
{
    if (e == entt::null || !scene->is_valid_entity(e) || !scene->has_component<engine_name_component_t>(e))
    {
        return;
    }
    renaming_ = e;
    std::strncpy(rename_buffer_.data(), scene->get_component<engine_name_component_t>(e)->name, rename_buffer_.size() - 1);
    rename_buffer_.back() = '\0';
    rename_focus_ = true;
    rename_frame_ = ImGui::GetFrameCount();
}

bool engine::SceneHierarchyContext::is_rename_starting() const
{
    // the focus asked for in one frame activates the field in the next
    return ImGui::GetFrameCount() <= rename_frame_ + 2;
}

void engine::SceneHierarchyContext::sync_selected_from_scene(engine::Scene* scene)
{
    // Find an entity that currently has the guizmo component (the editor selection marker)
    entt::entity found = entt::null;
    auto view = scene->create_runtime_view();
    scene->attach_component_to_runtime_view<engine::guizmo_component_t>(view);
    for (const auto& e : view)
    {
        // If multiple, take the first (there should normally be at most one)
        found = e;
        break;
    }
    selected_ = found;
    selected_ref_ = scene->make_ref(found);
}

void engine::SceneHierarchyContext::resolve_selection(engine::Scene* scene)
{
    const auto e = scene->resolve(selected_ref_);
    if (e == entt::null)
    {
        // gone for good: never selected again by a game object which gets its ID later
        reset();
        return;
    }
    if (e != selected_)
    {
        selected_ = e;
        set_selection_markers(*scene, e, true);
    }
}

void engine::SceneHierarchyContext::reset()
{
    selected_ = entt::null;
    selected_ref_ = {};
    delete_without_undo_ = entt::null;
    renaming_ = entt::null;
}

void engine::ApplicationEditor::EditorWindowsContext::initialize(std::uint32_t dockspace_id)
{
    // Dock builder
    const auto viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dockspace_id); // Clear previous layout
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    auto window_scene_id = dockspace_id;

    ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(window_scene_id, ImGuiDir_Down, 0.2f, nullptr, &window_scene_id);
    ImGui::DockBuilderDockWindow(window_down, dock_id_down);
    ImGui::DockBuilderGetNode(dock_id_down)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    ImGuiID dock_id_down_right = ImGui::DockBuilderSplitNode(dock_id_down, ImGuiDir_Right, 0.3f, nullptr, &dock_id_down);
    ImGui::DockBuilderDockWindow(window_down_right, dock_id_down_right);
    ImGui::DockBuilderGetNode(dock_id_down_right)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(window_scene_id, ImGuiDir_Left, 0.2f, nullptr, &window_scene_id);
    ImGui::DockBuilderDockWindow(window_left, dock_id_left);
    ImGui::DockBuilderGetNode(dock_id_left)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(window_scene_id, ImGuiDir_Right, 0.2f, nullptr, &window_scene_id);
    ImGui::DockBuilderDockWindow(window_right, dock_id_right);
    ImGui::DockBuilderGetNode(dock_id_right)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    ImGuiID dock_id_up = ImGui::DockBuilderSplitNode(window_scene_id, ImGuiDir_Up, 0.2f, nullptr, &window_scene_id);
    ImGui::DockBuilderDockWindow(window_up, dock_id_up);
    ImGui::DockBuilderGetNode(dock_id_up)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    // window scene
    ImGui::DockBuilderDockWindow(window_scene, window_scene_id);
    ImGui::DockBuilderGetNode(window_scene_id)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

    ImGui::DockBuilderFinish(dockspace_id);

    this->initalized_ = true;
}
