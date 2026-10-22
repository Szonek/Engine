#include "application_editor.h"

#include "scene.h"
#include "math_helpers.h"
#include "world_transform.h"
#include "logger.h"
#include "profiler.h"
#include "animation_controller.h"
#include "engine_string_impl_def.h"

#include "components_internals/camera_internal_component.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_opengl3.h"

#include "imguizmo/ImGuizmo.h"

#include <fmt/format.h>

#include <array>
#include <functional>
#include <cstring>
#include <string>
#include <string_view>
#include <map>
#include <memory>
#include <numeric>
#include <vector>

namespace
{
constexpr const engine_keyboard_keys_t G_MOUSE_SELECT_KEYBOARD_KEY = engine_keyboard_keys_t::ENGINE_KEYBOARD_KEY_LCTRL;

inline auto get_spherical_coordinates(const auto& cartesian)
{
    const float r = std::sqrt(
        std::pow(cartesian[0], 2) +
        std::pow(cartesian[1], 2) +
        std::pow(cartesian[2], 2)
    );


    float phi = std::atan2(cartesian[2] / cartesian[0], cartesian[0]);
    const float theta = std::acos(cartesian[1] / r);

    if (cartesian[0] < 0)
        phi += 3.1415f;

    std::array<float, 3> ret{ 0.0f };
    ret[0] = r;
    ret[1] = phi;
    ret[2] = theta;
    return ret;
}

inline auto get_cartesian_coordinates(const auto& spherical)
{
    std::array<float, 3> ret{ 0.0f };

    ret[0] = spherical[0] * std::cos(spherical[2]) * std::cos(spherical[1]);
    ret[1] = spherical[0] * std::sin(spherical[2]);
    ret[2] = spherical[0] * std::cos(spherical[2]) * std::sin(spherical[1]);

    return ret;
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

    const auto entity_id = std::string(node->name) + "##" + std::to_string(static_cast<std::uint32_t>(node->entity));
    // a collapsed node returns false but can still be clicked, dragged etc., so its item is handled either way
    const bool open = ImGui::TreeNodeEx(entity_id.c_str(), dispaly_flags);
    // tooltip the id
    ImGui::SetItemTooltip("ID: %llu (entity: %u)", static_cast<unsigned long long>(scene->get_persistent_id(node->entity)), static_cast<std::uint32_t>(node->entity));

    // select entity with LMB
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        // but if tree was expanded (i.e. by pressing arrow) it does not count as selection of entity
        if (!ImGui::IsItemToggledOpen())
        {
            ctx.set_selected_entity(scene, node->entity, commands);
        }
    }
    
    // context menu with RMB
    if (ImGui::BeginPopupContextItem())
    {
        // delete entity
        if (ImGui::MenuItem("Delete"))
        {
            ctx.set_selected_entity(scene, entt::null, commands);
            commands.execute_command(std::make_unique<engine::CommandDestroyEntity>(*scene, node->entity));
        }

        //rename entity
        static decltype(engine_name_component_t::name) new_name = "New name";
        if(ImGui::Button("Rename"))
        {
            commands.execute_command(std::make_unique<engine::CommandRenameEntity>(*scene, node->entity, new_name));
            // reset static new name
            std::strcpy(new_name, "New name");
        }
        ImGui::SameLine();        
        ImGui::InputText("##edit", new_name, IM_ARRAYSIZE(new_name));
        ImGui::EndPopup();
    }

    // drag and drop
    if (ImGui::BeginDragDropSource())
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
        }
        ImGui::EndDragDropTarget();
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

template<typename T>
inline void display_component(std::string_view name, engine::Scene* scene, engine::CommandManager& commands, entt::entity entity, std::function<bool(const engine::Scene* scene, T& comp)> fn)
{
    ENGINE_PROFILE_SECTION_N("editor-display_component");
    const bool has_component = scene->has_component<T>(entity);
    if (!has_component)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::CollapsingHeader(name.data(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowItemOverlap))
    {
        if (has_component)
        {
            auto comp = *scene->get_component<T>(entity);
            if (fn(scene, comp))
            {
                //scene->update_component<T>(entity, comp);
                commands.execute_command(std::make_unique<engine::CommandUpdateComponent<T>>(*scene, entity, comp));
            }

        }
    }
    else
    {
        ImGui::SameLine();
    }
    if (!has_component)
    {
        ImGui::EndDisabled();
    }
    if (has_component)
    {
        const std::string label = std::string("Remove##") + std::string(name);
        if (ImGui::Button(label.c_str()))
        {
            scene->remove_component<T>(entity);
        }
    }
    else
    {
        const std::string label = std::string("Add##") + std::string(name);
        if (ImGui::Button(label.c_str()))
        {
            scene->add_component<T>(entity);
        }
    }
}

// Combo box picking a resource by its asset ID (atlas name) instead of typing its index; "None" selects
// ENGINE_INVALID_OBJECT_HANDLE. The list of assets is only built while the combo is open. Returns true when the
// selection changed.
bool display_asset_combo(const char* label, std::uint32_t& handle, std::string_view current_name, const std::function<std::vector<std::pair<std::uint32_t, std::string>>()>& get_assets)
{
    const auto preview = handle == ENGINE_INVALID_OBJECT_HANDLE ? std::string("None")
        : current_name.empty() ? fmt::format("<missing #{}>", handle) : std::string(current_name);
    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str()))
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
                handle = idx;
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

bool display_geometry_combo(const engine::Scene* scene, std::uint32_t& geometry)
{
    const auto* app = scene->get_application();
    return display_asset_combo("Geometry", geometry, app->get_geometry_name(geometry), [app] { return app->get_geometry_names(); });
}

bool display_texture_combo(const engine::Scene* scene, const char* label, std::uint32_t& texture)
{
    const auto* app = scene->get_application();
    return display_asset_combo(label, texture, app->get_texture_name(texture), [app] { return app->get_texture_names(); });
}

// Draws a value of a registered type from its description, a widget per field kind; for the components without a
// hand-written panel (game-defined components). Edits the value in place and returns true when a field changed.
class TypeInspector
{
public:
    explicit TypeInspector(const engine::Scene* scene) : scene_(scene) {}

    bool draw_struct(const engine::TypeInfo& type, std::byte* value)
    {
        // the case each union holds before its tag (drawn before it) is edited
        std::vector<const engine::UnionCase*> cases_before;
        for (const auto& field : type.fields)
        {
            if (field.kind == engine::FieldKind::eUnion)
            {
                cases_before.push_back(engine::find_active_case(type, field, value));
            }
        }

        bool changed = false;
        std::size_t union_index = 0;
        for (const auto& field : type.fields)
        {
            ImGui::PushID(field.name.c_str());
            if (field.kind == engine::FieldKind::eUnion)
            {
                // the tag selected another case: its bytes are the old case's, so the new one starts zeroed (like a
                // loaded file) before it is drawn
                if (engine::find_active_case(type, field, value) != cases_before[union_index++])
                {
                    engine::clear_union(field, value);
                }
                changed |= draw_union(type, field, value);
            }
            else
            {
                changed |= draw_field(field, value + field.offset);
            }
            ImGui::PopID();
        }
        return changed;
    }

private:
    bool draw_union(const engine::TypeInfo& type, const engine::FieldInfo& field, std::byte* value)
    {
        const auto* active = engine::find_active_case(type, field, value);
        if (!active)
        {
            ImGui::TextDisabled("%s: no data for this %s", field.name.c_str(), field.union_tag.c_str());
            return false;
        }
        bool changed = false;
        if (ImGui::TreeNodeEx(fmt::format("{} ({})", field.name, active->name).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed = draw_struct(*active->type, value + field.offset);
            ImGui::TreePop();
        }
        return changed;
    }

    bool draw_field(const engine::FieldInfo& field, std::byte* p)
    {
        using engine::FieldKind;
        if (!field.is_array)
        {
            return draw_element(field, p, field.name.c_str());
        }
        // short arrays of numbers (vectors, colours) on one line
        const auto count = static_cast<int>(field.count);
        if (count <= 4 && field.kind == FieldKind::eFloat)
        {
            return ImGui::DragScalarN(field.name.c_str(), ImGuiDataType_Float, p, count, 0.1f);
        }
        if (count <= 4 && (field.kind == FieldKind::eInt32 || field.kind == FieldKind::eUint32))
        {
            return ImGui::DragScalarN(field.name.c_str(), field.kind == FieldKind::eInt32 ? ImGuiDataType_S32 : ImGuiDataType_U32, p, count, 1.0f);
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
        case FieldKind::eInt32: return ImGui::DragScalar(label, ImGuiDataType_S32, p, 1.0f);
        case FieldKind::eUint32: return ImGui::DragScalar(label, ImGuiDataType_U32, p, 1.0f);
        case FieldKind::eFloat: return ImGui::DragScalar(label, ImGuiDataType_Float, p, 0.1f);
        case FieldKind::eString: return draw_string(field, p, label);
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
        case FieldKind::eUnion: return false;  // drawn by draw_struct, which knows the tag
        case FieldKind::eAsset:
        {
            auto handle = read<std::uint32_t>(p);
            const auto* app = scene_->get_application();
            const bool changed = field.asset_kind == engine::AssetKind::eGeometry
                ? display_asset_combo(label, handle, app->get_geometry_name(handle), [app] { return app->get_geometry_names(); })
                : display_asset_combo(label, handle, app->get_texture_name(handle), [app] { return app->get_texture_names(); });
            std::memcpy(p, &handle, sizeof(handle));
            return changed;
        }
        case FieldKind::eGameObject: return draw_game_object(p, label);
        }
        return false;
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

    const engine::Scene* scene_;
};

// display_component for a component known only by its type description, drawn by the TypeInspector
void display_typed_component(const engine::TypeInfo& type, engine::Scene* scene, engine::CommandManager& commands, entt::entity entity)
{
    ENGINE_PROFILE_SECTION_N("editor-display_typed_component");
    ImGui::PushID(type.name.c_str());
    const bool has_component = scene->has_component(type, entity);
    if (!has_component)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::CollapsingHeader(type.name.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowItemOverlap))
    {
        if (has_component)
        {
            engine::ValueBuffer value(type.size);
            scene->get_component(type, entity, value.get());
            if (TypeInspector(scene).draw_struct(type, value.get()))
            {
                commands.execute_command(std::make_unique<engine::CommandUpdateTypedComponent>(*scene, entity, type, value.get()));
            }
        }
    }
    else
    {
        ImGui::SameLine();
    }
    if (!has_component)
    {
        ImGui::EndDisabled();
    }
    if (ImGui::Button(has_component ? "Remove" : "Add"))
    {
        if (has_component)
        {
            commands.execute_command(std::make_unique<engine::CommandRemoveTypedComponent>(*scene, entity, type));
        }
        else
        {
            commands.execute_command(std::make_unique<engine::CommandAddTypedComponent>(*scene, entity, type));
        }
    }
    ImGui::PopID();
}

bool display_transform_component(const engine::Scene* /*scene*/, engine_transform_component_t& c)
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

bool display_mesh_component(const engine::Scene* scene, engine_mesh_component_t& c)
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

bool display_skinned_mesh_component(const engine::Scene* scene, engine_skinned_mesh_component_t& c)
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

bool display_skin_component(const engine::Scene* scene, engine_skin_component_t& c)
{
    const auto name = scene->get_application()->get_skin_name(c.skin);
    ImGui::Text("Asset: %s", name.empty() ? "None" : name.c_str());
    return false;
}

bool display_folder_component(const engine::Scene* /*scene*/, engine_folder_component_t& c)
{
    return ImGui::InputText("Path##folder", c.path, ENGINE_FOLDER_PATH_MAX_LENGTH);
}

bool display_joint_attachment_component(const engine::Scene* scene, engine_joint_attachment_component_t& c)
{
    ImGui::Text("Skin of: %s", skin_owner_label(scene, c.skin).c_str());
    ImGui::Text("Joint name: %s", c.joint_name->str.c_str());
    return false;
}

bool display_animation_controller_component(const engine::Scene* scene, engine_animation_controller_component_t& c)
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


bool display_light_component(const engine::Scene* /*scene*/, engine_light_component_t& c)
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


bool display_material_component(const engine::Scene* scene, engine_material_component_t& c)
{
    // pong is the only material type
    bool requires_component_updated = false;
    requires_component_updated |= ImGui::ColorEdit4("Diffuse Color", c.data.pong.diffuse_color);
    requires_component_updated |= ImGui::SliderFloat("Shininess", &c.data.pong.shininess, 0.0f, 128.0f);
    requires_component_updated |= display_texture_combo(scene, "Diffuse Texture", c.data.pong.diffuse_texture);
    requires_component_updated |= display_texture_combo(scene, "Specular Texture", c.data.pong.specular_texture);
    return requires_component_updated;
}

bool display_collider_component(const engine::Scene* /*scene*/, engine_collider_component_t& c)
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
    // bouncies
    requires_component_updated |= ImGui::DragFloat("Bounciness", &c.bounciness, 0.1f);
    // friction
    requires_component_updated |= ImGui::DragFloat("Friction", &c.friction_static, 0.1f);
    return requires_component_updated;
}

bool display_rigidbody_component(const engine::Scene* /*scene*/, engine_rigid_body_component_t& c)
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

bool display_character_controller_component(const engine::Scene* /*scene*/, engine_character_controller_component_t& c)
{
    bool changed = false;
    changed |= ImGui::DragFloat("Radius##character_controller", &c.radius, 0.01f, 0.01f, 10.0f);
    changed |= ImGui::DragFloat("Height##character_controller", &c.height, 0.01f, 0.01f, 20.0f);
    changed |= ImGui::DragFloat("Step height##character_controller", &c.step_height, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("Max slope##character_controller", &c.max_slope, 0.5f, 0.0f, 90.0f);
    return changed;
}

bool display_nav_surface_component(const engine::Scene* /*scene*/, engine_nav_surface_component_t& c)
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

bool display_nav_modifier_component(const engine::Scene* /*scene*/, engine_nav_modifier_component_t& c)
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

bool display_camera_component(const engine::Scene* /*scene*/, engine_camera_component_t& c)
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
    if (editor_view_ == EditorView::eEditor)
    {
        camera_context_.on_scene_update_pre(scene, is_playing_ ? delta_time : frame_delta_time_);
    }
}

void engine::ApplicationEditor::on_scene_update_post(Scene* scene, float)
{
    ENGINE_PROFILE_SECTION_N("editor-on_scene_update_post");
    const float editor_delta_time = frame_delta_time_;

    render_editor_controls(scene, editor_delta_time);
    render_scene_hierarchy_panel(scene, editor_delta_time);
    render_entity_properties_panel(scene, editor_delta_time);
    render_debug_panel(scene, editor_delta_time);

    if (editor_view_ == EditorView::eEditor)
    {
        render_guizmo(scene);
        handle_mouse_picking(scene);
    }
    const auto lctrl = keyboard_is_key_down(ENGINE_KEYBOARD_KEY_LCTRL);
    const auto lshift = keyboard_is_key_down(ENGINE_KEYBOARD_KEY_LSHIFT);
    const auto z = keyboard_is_key_down(ENGINE_KEYBOARD_KEY_Z);
    static bool ctrl_shift_z_prev = lctrl && lshift && z;
    const auto ctrl_shift_z_curr = lctrl && lshift && z;
    if (ctrl_shift_z_prev && !ctrl_shift_z_curr)
    {
        commands_.redo();
        scene_hierarchy_context_.sync_selected_from_scene(scene);
    }
    ctrl_shift_z_prev = ctrl_shift_z_curr;

    static bool ctrl_z_prev = lctrl && !lshift && z;
    const auto ctrl_z_curr = lctrl && !lshift && z;
    if (ctrl_z_curr && !ctrl_z_prev)
    {
        commands_.undo();
        scene_hierarchy_context_.sync_selected_from_scene(scene);
    }
    ctrl_z_prev = ctrl_z_curr;



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
    // No item covers the image: ImGuizmo only starts a drag when no item is hovered.
    // The mouse stays with the scene while a button pressed over it is held.
    const bool hovered = ImGui::IsWindowHovered();
    const auto& io = ImGui::GetIO();
    const bool any_button_down = io.MouseDown[ImGuiMouseButton_Left] || io.MouseDown[ImGuiMouseButton_Right] || io.MouseDown[ImGuiMouseButton_Middle];
    const bool any_button_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
    if (hovered && any_button_clicked)
    {
        scene_view_mouse_held_ = true;
    }
    else if (!any_button_down)
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

void engine::ApplicationEditor::render_editor_controls(class Scene* /*scene*/, float /*dt*/)
{
    ImGui::Begin(editor_windows_context_.get_window_up());

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
    if (ImGui::Button(is_playing_ ? "Pause" : "Play"))
    {
        is_playing_ = !is_playing_;
    }
    ImGui::End();
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
        commands_.execute_command(std::make_unique<CommandAddEntity>(*scene));
    }

    bool phys_debug_draw_check = scene->physics().is_debug_draw_enabled();
    if (ImGui::Checkbox("Physics debug draw", &phys_debug_draw_check))
    {
        commands_.execute_command(std::make_unique<CommandSetPhysicsDebugDraw>(*scene, phys_debug_draw_check));
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
    ImGui::End(); // scene panel
}

void engine::ApplicationEditor::render_entity_properties_panel(class Scene* scene, float /*dt*/)
{
    /*
    here all components should be visible
        - the one which are not avaialbe in the entitiy (Grayd out) and a "+" button to add them
        - the one which are available in the entity (whatever color) and a "-" button to remove them
    */
    ImGui::Begin(editor_windows_context_.get_window_right());
    ImGui::SeparatorText("Entity properties.");
    if (scene_hierarchy_context_.has_selected_entity())
    {
        const auto selected = scene_hierarchy_context_.get_selected_entity();
        display_component<engine_folder_component_t>("Folder", scene, commands_, selected, display_folder_component);
        display_component<engine_transform_component_t>("Transform", scene, commands_, selected, display_transform_component);
        display_component<engine_light_component_t>("Light", scene, commands_, selected, display_light_component);
        display_component<engine_camera_component_t>("Camera", scene, commands_, selected, display_camera_component);
        display_component<engine_mesh_component_t>("Mesh", scene, commands_, selected, display_mesh_component);
        display_component<engine_skinned_mesh_component_t>("Skinned Mesh", scene, commands_, selected, display_skinned_mesh_component);
        display_component<engine_skin_component_t>("Skin", scene, commands_, selected, display_skin_component);
        display_component<engine_joint_attachment_component_t>("Joint Attachment", scene, commands_, selected, display_joint_attachment_component);
        display_component<engine_animation_controller_component_t>("Animation controller", scene, commands_, selected, display_animation_controller_component);
        display_component<engine_material_component_t>("Material", scene, commands_, selected, display_material_component);
        display_component<engine_collider_component_t>("Collider", scene, commands_, selected, display_collider_component);
        display_component<engine_rigid_body_component_t>("Rigid Body", scene, commands_, selected, display_rigidbody_component);
        display_component<engine_character_controller_component_t>("Character Controller", scene, commands_, selected, display_character_controller_component);
        display_component<engine_nav_surface_component_t>("Nav Surface", scene, commands_, selected, display_nav_surface_component);
        // like a bake: nothing rebuilds the nav mesh automatically, so after editing the nav surface, nav modifiers or
        // static colliders, build it again to see (and play with) the result
        if (scene->has_component<engine_nav_surface_component_t>(selected) && ImGui::Button("Build nav mesh"))
        {
            scene->build_nav_mesh();
        }
        display_component<engine_nav_modifier_component_t>("Nav Modifier", scene, commands_, selected, display_nav_modifier_component);
        // the game's components have no hand-written panel: drawn from their type descriptions
        for (const auto* type : get_type_registry().get_components())
        {
            if (type->game_defined)
            {
                display_typed_component(*type, scene, commands_, selected);
            }
        }
    }
    else
    {
        ImGui::Text("Select entity to display its components.");
    }
    ImGui::End();
}

void engine::ApplicationEditor::render_debug_panel(class Scene* /*scene*/, float dt)
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

            ImGuizmo::SetOrthographic(camera.type == ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC); // Set the projection mode to perspective
            ImGuizmo::Manipulate(glm::value_ptr(camera_view), glm::value_ptr(camera_projection),
                ImGuizmo::OPERATION::TRANSLATE | ImGuizmo::OPERATION::SCALE | ImGuizmo::OPERATION::ROTATE,
                ImGuizmo::MODE::LOCAL,
                glm::value_ptr(model_matrix));

            if (ImGuizmo::IsUsing())
            {
                auto c = *scene->get_component<engine_transform_component_t>(entity);
                if (engine::local_transform_from_world(scene->get_parent_space_matrix(entity), model_matrix, c))
                {
                    commands_.execute_command(std::make_unique<CommandUpdateComponent<engine_transform_component_t>>(*scene, entity, c));
                }
            }
        }
    }
    ImGui::End();
}

void engine::ApplicationEditor::handle_mouse_picking(Scene* scene)
{
    // a click on the gizmo moves the object, it doesn't pick what is under it
    if (!is_mouse_over_scene() || ImGuizmo::IsUsingAny())
    {
        return;
    }
    if (mouse_is_button_down(ENGINE_MOUSE_BUTTON_LEFT) && !keyboard_is_key_down(G_MOUSE_SELECT_KEYBOARD_KEY))
    {
        const auto scene_viewport = get_scene_viewport();
        const auto mouse_coords = mouse_get_coords();
        if (mouse_coords.x < 0.0f || mouse_coords.y < 0.0f)
        {
            return;  // held since a click over the scene, but moved off it
        }
        // entity ids come from the previous frame, which is what was visible on the screen when the user clicked
        std::uint32_t picked_id = ENGINE_INVALID_GAME_OBJECT_ID;
        const auto x = static_cast<std::uint32_t>(mouse_coords.x * scene_viewport.width);
        const auto y = static_cast<std::uint32_t>(mouse_coords.y * scene_viewport.height);
        if (!renderer_.read_entity_id(x, y, picked_id))
        {
            return;
        }
        entt::entity entity_id = entt::null;
        if (picked_id != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            entity_id = static_cast<entt::entity>(picked_id);
        }
        scene_hierarchy_context_.set_selected_entity(scene, entity_id, commands_);
        scene_hierarchy_context_.set_forced_open_selected_parents(true);
    }
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
}

engine::CameraScript::CameraScript(Scene* scene, ApplicationEditor* app)
    : my_scene_(scene)
    , app_(app)
    , go_(scene->create_transient_entity())
{
    scene->add_component<engine_name_component_t>(go_);
    scene->patch_component<engine_name_component_t>(go_, [](engine_name_component_t& c)
        {   
            constexpr const char* editor_camera_name = "__engine_editor_camera__";
            std::memcpy(c.name, editor_camera_name, std::strlen(editor_camera_name));
        });

    scene->add_component<engine_camera_component_t>(go_);
    scene->patch_component<engine_camera_component_t>(go_, [](engine_camera_component_t& c)
        {
            c.enabled = true;
            c.clip_plane_near = 0.1f;
            c.clip_plane_far = 1000.0f;
            c.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
            c.type_union.perspective_fov = 45.0f;
            c.target[0] = -2.0f;
            c.target[1] = 0.1f;
            c.target[2] = 0.2f;
        });

    auto camera_transform_comp = scene->add_component<engine_transform_component_t>(go_);
    scene->patch_component<engine_transform_component_t>(go_, [](engine_transform_component_t& c)
        {
            c.position[0] = -1.0f;
            c.position[1] = 1.7f;
            c.position[2] = 5.0f;
        });

    sc_ = get_spherical_coordinates(camera_transform_comp->position);
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

void engine::CameraScript::update(float dt)
{
    const auto mouse_coords = app_->mouse_get_coords();
    const auto dx = mouse_coords.x - mouse_coords_prev_.x;
    const auto dy = mouse_coords.y - mouse_coords_prev_.y;

    if (mouse_coords.x != mouse_coords_prev_.x || mouse_coords.y != mouse_coords_prev_.y)
    {
        mouse_coords_prev_ = mouse_coords;
    }

    //if not enabled - do nothing
    auto cc = my_scene_->get_component<engine_camera_component_t>(go_);
    if (!cc->enabled)
    {
        return;
    }

    // dont allow to move camera if mouse is down
    if (!app_->is_mouse_over_scene() || ImGuizmo::IsUsingAny())
    {
        return;
    }

    const float move_speed = 0.1f * dt;

    const bool lmb = app_->mouse_is_button_down(ENGINE_MOUSE_BUTTON_LEFT);
    const bool rmb = app_->mouse_is_button_down(ENGINE_MOUSE_BUTTON_RIGHT);
    const bool mmb = app_->mouse_is_button_down(ENGINE_MOUSE_BUTTON_MIDDLE);


    //if (app_->keyboard_is_key_down(ENGINE_KEYBOARD_KEY_LSHIFT))
    {
        if (lmb && app_->keyboard_is_key_down(G_MOUSE_SELECT_KEYBOARD_KEY))
        {
            rotate({ dx * move_speed, dy * move_speed });            
        }
        else if (rmb)
        {
            strafe(dx * move_speed, dy * move_speed);
        }
        else if (mmb)
        {
            translate({ 0.0f, 0.0f, dy * move_speed });
        }
    }
}

void engine::CameraScript::translate(const glm::vec3& delta)
{
    // Decrease the radius based on the delta's z value
    sc_[0] -= delta.z;
    // Make sure the radius doesn't go below a certain threshold to prevent the camera from going inside the target
    sc_[0] = std::max(sc_[0], 0.1f);
    // Update the camera's position based on the new spherical coordinates
    const auto new_position = get_cartesian_coordinates(sc_);
    auto cc = my_scene_->get_component<engine_camera_component_t>(go_);
    my_scene_->patch_component<engine_transform_component_t>(go_, [new_position, cc](engine_transform_component_t& c)
        {
            c.position[0] = new_position[0] + cc->target[0];
            c.position[1] = new_position[1] + cc->target[1];
            c.position[2] = new_position[2] + cc->target[2];
        });
}

void engine::CameraScript::rotate(const glm::vec2 delta)
{
    // https://nerdhut.de/2020/05/09/unity-arcball-camera-spherical-coordinates/
    if (delta.x != 0 || delta.y != 0)
    {
        // Rotate the camera left and right
        sc_[1] += delta.x;

        // Rotate the camera up and down
        // Prevent the camera from turning upside down (1.5f = approx. Pi / 2)
        sc_[2] = std::clamp(sc_[2] + delta.y, -1.5f, 1.5f);

        const auto new_position = get_cartesian_coordinates(sc_);
        auto cc = my_scene_->get_component<engine_camera_component_t>(go_);

        my_scene_->patch_component<engine_transform_component_t>(go_, [new_position, cc](engine_transform_component_t& c)
            {
                c.position[0] = new_position[0] + cc->target[0];
                c.position[1] = new_position[1] + cc->target[1];
                c.position[2] = new_position[2] + cc->target[2];
            });
    }
}

void engine::CameraScript::strafe(float delta_x, float delta_y)
{
    // Get the current camera orientation
    auto tc = *my_scene_->get_component<engine_transform_component_t>(go_);
    auto cc = *my_scene_->get_component<engine_camera_component_t>(go_);

    // Compute the right vector from the camera's orientation
    glm::vec3 forward(cc.target[0] - tc.position[0], cc.target[1] - tc.position[1], cc.target[2] - tc.position[2]);
    glm::vec3 up(0.0f, 1.0f, 0.0f); // Assuming the up vector is (0, 1, 0)
    glm::vec3 right = glm::normalize(glm::cross(forward, up));

    // Update the camera's position
    tc.position[0] += delta_x * right.x;
    tc.position[1] += delta_x * right.y + delta_y;
    tc.position[2] += delta_x * right.z;

    // Update the camera's target
    cc.target[0] += delta_x * right.x;
    cc.target[1] += delta_x * right.y + delta_y;
    cc.target[2] += delta_x * right.z;

    // Update the transform and camera components
    my_scene_->update_component<engine_transform_component_t>(go_, tc);
    my_scene_->update_component<engine_camera_component_t>(go_, cc);
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

void engine::ApplicationEditor::CameraContext::on_scene_update_pre(Scene* scene, float dt)
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
        camera_data.camera.update(dt);
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

void engine::SceneHierarchyContext::set_selected_entity(engine::Scene* scene, entt::entity e, CommandManager& commands)
{
    if (selected_ == e)
    {
        return;
    }
    commands.execute_command(std::make_unique<engine::CommandSetSelectedEntity>(*scene, selected_, e));
    selected_ = e;
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
