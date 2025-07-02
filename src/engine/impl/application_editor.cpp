#include "application_editor.h"

#include "scene.h"
#include "math_helpers.h"
#include "logger.h"
#include "profiler.h"
#include "skin.h"
#include "animation_controller.h"

#include "components_internals/guizmo_component.h"
#include "components_internals/outline_component.h"
#include "components_internals/camera_internal_component.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_opengl3.h"

#include "imguizmo/ImGuizmo.h"

#include <string>
#include <map>
#include <numeric>
#include <vector>

namespace
{
constexpr const engine_keyboard_keys_t G_MOUSE_SELECT_KEYBOARD_KEY = engine_keyboard_keys_t::ENGINE_KEYBOARD_KEY_LCTRL;

struct FileEntry
{
    bool is_directory = false;
    std::filesystem::path path;
};

inline std::vector<FileEntry> get_files_in_directory(const std::filesystem::path& dir_path, std::vector<std::string> file_exts = {})
{
    std::vector<FileEntry> files{};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path))
    {

        const auto file_has_supported_extensions = file_exts.empty() ? true : std::find(file_exts.begin(), file_exts.end(), entry.path().extension()) != file_exts.end();
        if (file_has_supported_extensions)
        {
            FileEntry ret{};
            ret.is_directory = entry.is_directory();
            ret.path = entry.path();
            files.push_back(ret);
        }
    }
    return files;
}

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

    bool displayed = false;
};

inline void traverse_hierarchy(entity_node_t* node, std::function<void(entity_node_t*)> fn)
{
    fn(node);
    for (auto& child : node->children)
    {
        traverse_hierarchy(child, fn);
    }
}

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

inline void display_node(entity_node_t* node, engine::Scene* scene, engine::SceneHierarchyContext& ctx)
{
    ENGINE_PROFILE_SECTION_N("editor-display_node");
    node->displayed = true;
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
    if (ImGui::TreeNodeEx(entity_id.c_str(), dispaly_flags))
    {
        // tooltip the id
        ImGui::SetItemTooltip("ID: %d", node->entity);

        // select entity with LMB
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            // but if tree was expanded (i.e. by pressing arrow) it does not count as selection of entity
            if (!ImGui::IsItemToggledOpen())
            {
                ctx.set_selected_entity(scene, node->entity);
            }
        }
        
        // context menu with RMB
        if (ImGui::BeginPopupContextItem())
        {
            // delete entity
            if (ImGui::MenuItem("Delete"))
            {
                traverse_hierarchy(node, [&scene](entity_node_t* n) { scene->destroy_entity(n->entity); });                
            }
            
            //rename entity
            static decltype(engine_name_component_t::name) new_name = "New name";
            if(ImGui::Button("Rename"))
            {   
                // update component
                auto nc = *scene->get_component<engine_name_component_t>(node->entity);
                std::strcpy(nc.name, new_name);
                scene->update_component<engine_name_component_t>(node->entity, nc);

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


                if (scene->has_component<engine_parent_component_t>(payload_n))
                {
                    engine_parent_component_t pc{};
                    pc.parent = static_cast<std::uint32_t>(node->entity);
                    scene->update_component<engine_parent_component_t>(payload_n, pc);
                }
                else
                {
                    auto pc = *scene->add_component<engine_parent_component_t>(payload_n);
                    pc.parent = static_cast<std::uint32_t>(node->entity);
                    scene->update_component<engine_parent_component_t>(payload_n, pc);
                }
            }
            ImGui::EndDragDropTarget();
        }

        else
        {
            for (auto& child : node->children)
            {
                display_node(child, scene, ctx);
            }
        }
        ImGui::TreePop();
    }
}

template<typename T>
inline void display_component(std::string_view name, engine::Scene* scene, entt::entity entity, std::function<bool(const engine::Scene* scene, T& comp)> fn)
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
                scene->update_component<T>(entity, comp);
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

bool display_transform_component(const engine::Scene* scene, engine_tranform_component_t& c)
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
    const auto geo_name = scene->get_application()->get_geometry_name(c.geometry);
    ImGui::Text("Geometry: %s", geo_name.c_str());
    ImGui::InputInt("Geometry ID", reinterpret_cast<std::int32_t*>(&c.geometry));
    return true;
}

bool display_skinned_mesh_component(const engine::Scene* scene, engine_skinned_mesh_component_t& c)
{
    const auto typed_skin = reinterpret_cast<engine::Skin*>(c.skin);
    bool enabled = !c.disable;
    if (ImGui::Checkbox("Enabled", &enabled))
    {
        c.disable = !c.disable;
    }
    const auto geo_name = scene->get_application()->get_geometry_name(c.geometry);
    ImGui::Text("Geometry: %s", geo_name.c_str());
    ImGui::InputInt("Geometry ID", reinterpret_cast<std::int32_t*>(&c.geometry));
    ImGui::Text("Skin: %s", typed_skin ? typed_skin->get_name().c_str() : "None");
    return true;
}

bool display_skin_component(const engine::Scene* scene, engine_skin_component_t& c)
{
    const auto typed_skin = reinterpret_cast<engine::Skin*>(c.skin);
    ImGui::Text("Name: %s", typed_skin ? typed_skin->get_name().c_str() : "None");
    return false;
}

bool display_animation_controller_component(const engine::Scene* scene, engine_animation_controller_component_t& c)
{
    const auto typed_controller = reinterpret_cast<engine::AnimationController*>(c.controller);
    if (!typed_controller)
    {
        ImGui::Text("No animation controller assigned");
    }
    else
    {
        ImGui::Text("Animation controller assigned");
    }
    return false;
}


bool display_light_component(const engine::Scene* scene, engine_light_component_t& c)
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
    bool requires_component_updated = false;
    // list of types
    const char* items[] = { "Pong", "User" };
    std::int32_t selected_type = c.type;
    if (ImGui::ListBox("Type", &selected_type, items, std::size(items)))
    {
        requires_component_updated = true;
        c.type = static_cast<engine_material_type_t>(selected_type);
        // set some valid values when changeed the type
        std::memset(&c.data, 0, sizeof(c.data));
        //ToDO: defaults
    }


    if (c.type == ENGINE_MATERIAL_TYPE_PONG)
    {
        requires_component_updated |= ImGui::ColorEdit4("Diffuse Color", c.data.pong.diffuse_color);
        requires_component_updated |= ImGui::SliderFloat("Shininess", &c.data.pong.shininess, 0.0f, 128.0f);
        requires_component_updated |= ImGui::InputInt("Texture Diffuse ID", reinterpret_cast<std::int32_t*>(&c.data.pong.diffuse_texture));
        requires_component_updated |= ImGui::InputInt("Texture Specular ID", reinterpret_cast<std::int32_t*>(&c.data.pong.specular_texture));
    }
    else if (c.type == ENGINE_MATERIAL_TYPE_USER)
    {
        requires_component_updated |= ImGui::InputInt("Shader ID", reinterpret_cast<std::int32_t*>(&c.data.user.shader));
        for (auto i = 0; i < ENGINE_MATERIAL_USER_MAX_UNIFORM_BUFFER_SIZE; i++)
        {
            const std::string tex_name = "Texture_" + std::to_string(i) + " ID";
            requires_component_updated |= ImGui::InputInt(tex_name.c_str(), reinterpret_cast<std::int32_t*>(&c.data.user.texture_bindings[i]));
        }
    }

    return requires_component_updated;
}

bool display_collider_component(const engine::Scene* scene, engine_collider_component_t& c)
{
    auto print_collider_type = [](engine_collider_type_t type)
     {
        switch (type)
        {
            case ENGINE_COLLIDER_TYPE_BOX: return "Box";
            case ENGINE_COLLIDER_TYPE_SPHERE: return "Sphere";
            case ENGINE_COLLIDER_TYPE_COMPOUND: return "Compound";
            default: return "Unknown";
        }
    };
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
            ImGui::Text("Child %d", i);
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

bool display_rigidbody_component(const engine::Scene* scene, engine_rigid_body_component_t& c)
{
    bool requires_update = ImGui::SliderFloat("Mass", &c.mass, 0.0f, 100.0f);

    return requires_update;
}

bool display_camera_component(const engine::Scene* scene, engine_camera_component_t& c)
{
    // is it enabled?
    ImGui::Checkbox("Enabled", &c.enabled);

    // type
    const char* items[] = { "Orthographic",  "Perspective" };
    std::int32_t selected_type = c.type;
    if (ImGui::ListBox("Type", &selected_type, items, std::size(items)))
    {
        c.type = static_cast<engine_camera_projection_type_t>(selected_type);
    }

    // fov or scale, based on type
    if (c.type == ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE)
    {
        ImGui::DragFloat("FOV", &c.type_union.perspective_fov, 0.1f);
    }
    else
    {
        ImGui::DragFloat("Scale", &c.type_union.orthographics_scale, 0.1f);
    }

    // target
    ImGui::DragFloat3("Target", c.target, 0.1f);

    //viewport rect
    ImGui::DragFloat4("Viewport", &c.viewport_rect.x, 0.1f);

    // pitch, yaw, roll
    ImGui::DragFloat("Pitch", &c.pitch, 0.1f);
    ImGui::DragFloat("Yaw", &c.yaw, 0.1f);
    ImGui::DragFloat("Roll", &c.roll, 0.1f);

    // clip planes
    ImGui::DragFloat("Near Clip Plane", &c.clip_plane_near, 0.1f);
    ImGui::DragFloat("Far Clip Plane", &c.clip_plane_far, 0.1f);
    return true;
}

} // namespace anonymous


engine::ApplicationEditor::ApplicationEditor(const engine_application_create_desc_t& desc, engine_result_code_t& out_code)
    : Application(desc, out_code)
    , outline_effect_(desc.width, desc.height)
{
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
}

engine::ApplicationEditor::~ApplicationEditor()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void engine::ApplicationEditor::on_frame_begine(const engine_application_frame_begine_info_t & frame_begin_info)
{
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    // DockSpace
    const auto viewport = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
    if (!editor_windows_context_.is_initialized())
    {
        editor_windows_context_.initialize(dockspace_id);
    }
}

void engine::ApplicationEditor::on_frame_end()
{
    ImGui::Render();
    static bool render_im_gui = true;
    if(ImGui::IsKeyPressed(ImGuiKey_F1))
    {
        render_im_gui = !render_im_gui;
    }
    if (render_im_gui)
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
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
        camera_context_.on_scene_update_pre(scene, delta_time);
    }
}

void engine::ApplicationEditor::on_scene_update_post(Scene* scene, float delta_time)
{
    ENGINE_PROFILE_SECTION_N("editor-on_scene_update_post");
    const auto viewport = rdx_.get_window_size_in_pixels();

    render_editor_controls(scene, delta_time);
    render_scene_hierarchy_panel(scene, delta_time);
    render_entity_properties_panel(scene, delta_time);
    render_debug_panel(scene, delta_time);

    if (editor_view_ == EditorView::eEditor)
    {
        render_guizmo(scene);
        render_outline(scene);
        handle_mouse_picking(scene);
    }

    camera_context_.on_scene_update_post(scene, delta_time);
}

void engine::ApplicationEditor::render_editor_controls(class Scene* scene, float dt)
{
    ImGui::Begin(editor_windows_context_.get_window_up());

    const auto ww = ImGui::GetWindowWidth();
    const auto wh = ImGui::GetWindowHeight();
    const auto pos = ImGui::GetWindowPos();

    static std::uint32_t selected = 0;
    if (ImGui::Selectable("Scene", editor_view_ == EditorView::eGame, ImGuiSelectableFlags_None, ImVec2(50, 0)))
    {
        editor_view_ = EditorView::eGame;
    }
    ImGui::SameLine();
    if(ImGui::Selectable("Editor", editor_view_ == EditorView::eEditor, ImGuiSelectableFlags_None, ImVec2(50, 0)))
    {
        editor_view_ = EditorView::eEditor;
    }
    ImGui::End();
}

void engine::ApplicationEditor::render_scene_hierarchy_panel(Scene* scene, float dt)
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
        entity_map.insert({ e, entity_node_t{ e, name } });
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
        auto e = scene->create_new_entity();
        auto nc = scene->add_component<engine_name_component_t>(e);
        const auto new_name = "Entity " + std::to_string(static_cast<std::uint32_t>(e));
        std::memcpy(nc->name, new_name.c_str(), new_name.size());
    }

    static bool phys_debug_draw_check = false;
    ImGui::Checkbox("Physics debug draw", &phys_debug_draw_check);
    scene->enable_physics_debug_draw(phys_debug_draw_check);

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

        for (auto& [e, f] : entity_map)
        {
            if (!f.displayed && !f.parent)
            {
                display_node(&f, scene, scene_hierarchy_context_);
            }
        }
        ImGui::TreePop();
    }
    ImGui::End(); // scene panel
}

void engine::ApplicationEditor::render_entity_properties_panel(class Scene* scene, float dt)
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
        display_component<engine_tranform_component_t>("Transform", scene, selected, display_transform_component);
        display_component<engine_light_component_t>("Light", scene, selected, display_light_component);
        display_component<engine_camera_component_t>("Camera", scene, selected, display_camera_component);
        display_component<engine_mesh_component_t>("Mesh", scene, selected, display_mesh_component);
        display_component<engine_skinned_mesh_component_t>("Skinned Mesh", scene, selected, display_skinned_mesh_component);
        display_component<engine_skin_component_t>("Skin", scene, selected, display_skin_component);
        display_component<engine_animation_controller_component_t>("Animation controller", scene, selected, display_animation_controller_component);
        display_component<engine_material_component_t>("Material", scene, selected, display_material_component);
        display_component<engine_collider_component_t>("Collider", scene, selected, display_collider_component);
        display_component<engine_rigid_body_component_t>("Rigid Body", scene, selected, display_rigidbody_component);
    }
    else
    {
        ImGui::Text("Select entity to display its components.");
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
    ImGui::EndDisabled();

    const bool is_rml_ui_debugger_enabled = ui_manager_.is_ui_document_debugger_enabled();
    const std::string rmlui_switch_title = is_rml_ui_debugger_enabled ? "Disable Rml UI Debugger" : "Enable Rml UI Debugger";
    if (ImGui::Button(rmlui_switch_title.c_str()))
    {
        ui_manager_.enable_ui_document_debugger(!is_rml_ui_debugger_enabled);
    }
    assert(dt > 0.0f);
    static float avg_fps = 0;
    static std::vector<float> fps_values(60);
    static std::size_t fps_idx = 0;
    fps_values[fps_idx % fps_values.size()] = 1000.0f / dt;
    fps_idx++;
    ImGui::Text("FPS: %d", static_cast<std::uint32_t>(std::accumulate(fps_values.begin(), fps_values.end(), 0) / (fps_idx < fps_values.size() ? fps_idx : fps_values.size())));
    ImGui::End();
}

void engine::ApplicationEditor::render_guizmo(Scene* scene)
{
    if (!draw_guizmo_)
    {
        return;
    }
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
        scene->attach_component_to_runtime_view<engine_tranform_component_t>(gizmo_view);
        scene->attach_component_to_runtime_view<engine::guizmo_component_t>(gizmo_view);
        for (const auto& entity : gizmo_view)
        {
            const auto transform_component = *scene->get_component<engine_tranform_component_t>(entity);
            // ImGuizmo manipulation
            auto model_matrix = glm::make_mat4x4(transform_component.local_to_world);

            ImGuizmo::SetOrthographic(camera.type == ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC); // Set the projection mode to perspective
            //ImGuizmo::SetDrawlist(); // Set the draw list to the current ImGui window's draw list
            ImGuizmo::SetRect(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);  // Set the rectangle area for the gizmo to cover the entire display area

            ImGuizmo::Manipulate(glm::value_ptr(camera_view), glm::value_ptr(camera_projection),
                ImGuizmo::OPERATION::TRANSLATE | ImGuizmo::OPERATION::SCALE | ImGuizmo::OPERATION::ROTATE,
                ImGuizmo::MODE::LOCAL,
                glm::value_ptr(model_matrix));

            if (ImGuizmo::IsUsing())
            {
                glm::vec3 translation{};
                glm::vec3 scale{};
                glm::vec3 rotation{};
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model_matrix), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));

                scene->patch_component<engine_tranform_component_t>(entity, [&](engine_tranform_component_t& c)
                    {
                        std::memcpy(c.position, glm::value_ptr(translation), sizeof(translation));
                        const glm::quat rot = glm::quat(glm::radians(rotation));
                        std::memcpy(c.rotation, glm::value_ptr(rot), sizeof(rot));
                        std::memcpy(c.scale, glm::value_ptr(scale), sizeof(scale));
                    });
            }
        }
    }
}

void engine::ApplicationEditor::handle_mouse_picking(Scene* scene)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }
    if (mouse_is_button_down(ENGINE_MOUSE_BUTTON_LEFT) && !keyboard_is_key_down(G_MOUSE_SELECT_KEYBOARD_KEY))
    {
        const auto window_size = rdx_.get_window_size_in_pixels();
        const auto mouse_coords = mouse_get_coords();
        viewport_t vp{};
        vp.x = mouse_coords.x * window_size.width;
        vp.y = mouse_coords.y * window_size.height;
        vp.width = 1;
        vp.height = 1;
        const auto pixels = fbo_scene_.download_pixels(1, vp, DataLayout::eR_U32);
        assert(pixels.size() == 4);
        const auto pixels_u32 = reinterpret_cast<const std::uint32_t*>(pixels.data());
        entt::entity entity_id = entt::null;
        if (pixels_u32[0] != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            entity_id = static_cast<entt::entity>(pixels_u32[0]);
        }
        scene_hierarchy_context_.set_selected_entity(scene, entity_id);
        scene_hierarchy_context_.set_forced_open_selected_parents(true);
    }
}

void engine::ApplicationEditor::render_outline(Scene* scene)
{
    const auto window_size = rdx_.get_window_size_in_pixels();
    outline_effect_.fbo_outline.bind(AccessType::eWriteOnly);

    const auto& [fbo_w, fbo_h] = outline_effect_.fbo_outline.get_size();
    if (fbo_w != window_size.width || fbo_h != window_size.height)
    {
        outline_effect_.fbo_outline.resize(window_size.width, window_size.height);
    }
    rdx_.set_clear_color(0.0f, 0.0f, 0.0, 1.0f);
    outline_effect_.fbo_outline.clear();

    auto camera_view = scene->create_runtime_view();
    scene->attach_component_to_runtime_view<engine_camera_component_t>(camera_view);
    for (const auto& camera_entity : camera_view)
    {
        const auto camera = *scene->get_component<engine_camera_component_t>(camera_entity);
        if (!camera.enabled)
        {
            continue;
        }
        const auto camera_internal = scene->get_component<engine::camera_internal_component_t>(camera_entity);
        const auto camera_view = scene->get_camera_view(camera_entity);
        const auto camera_projection = scene->get_camera_projection(camera_entity);

        auto outline_view = scene->create_runtime_view();
        scene->attach_component_to_runtime_view<engine::outline_component_t>(outline_view);
        scene->attach_component_to_runtime_view<engine_tranform_component_t>(outline_view);
        scene->attach_component_to_runtime_view<engine_mesh_component_t>(outline_view);
        scene->attach_component_to_runtime_view<engine_material_component_t>(outline_view);

        for (const auto& entity : outline_view)
        {
            const auto transform_component = scene->get_component<engine_tranform_component_t>(entity);
            const auto mesh_component = scene->get_component<engine_mesh_component_t>(entity);
            float color_white[3] = { 1.0f, 1.0f, 1.0f };

            const auto& white_texture = *textures_atlas_.get_object(0);
            const auto& geometry = *geometries_atlas_.get_object(mesh_component->geometry);
     
            engine::MaterialStaticGeometryUnlit::DrawContext ctx
            {
                .camera = camera_internal->camera_ubo,
                .model_matrix = transform_component->local_to_world,
                .color_diffuse = color_white,
                .texture_diffuse = white_texture
            };
            outline_effect_.material_static_geometry_unlit.draw(geometry, ctx);
        }
    }

    outline_effect_.fbo_outline.unbind();
    //ToDO: optimize memory barriers here, no need to wait for all the stages
    rdx_.dispatch_barrier(engine::RenderContext::MemoryBarrierBitMask::MEMORY_BARRIER_ALL_BIT);
    outline_effect_.compute_shader_edge_detection.bind();
    outline_effect_.compute_shader_edge_detection.set_texture("img_in", outline_effect_.fbo_outline.get_color_attachment(0), engine::AccessType::eReadOnly, engine::DataLayout::eRGBA_U8);
    outline_effect_.compute_shader_edge_detection.set_texture("img_out", fbo_scene_.get_color_attachment(0), engine::AccessType::eReadWrite, engine::DataLayout::eRGBA_U8);
    outline_effect_.compute_shader_edge_detection.dispatch(window_size.width / 32, window_size.height / 4, 1);
    rdx_.dispatch_barrier(engine::RenderContext::MemoryBarrierBitMask::MEMORY_BARRIER_ALL_BIT);
    fbo_scene_.bind();
}

bool engine::ApplicationEditor::is_mouse_enabled()
{
    const bool editor_is_using_mouse = editor_view_ == EditorView::eEditor || ImGui::GetIO().WantCaptureMouse;
    return !editor_is_using_mouse;
}

bool engine::ApplicationEditor::is_keyboard_enabled()
{
    const bool editor_is_using_keybord = editor_view_ == EditorView::eEditor || ImGui::GetIO().WantCaptureKeyboard;
    return !editor_is_using_keybord;
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
    , go_(scene->create_new_entity())
{
    auto nc = scene->add_component<engine_name_component_t>(go_);
    scene->patch_component<engine_name_component_t>(go_, [nc](engine_name_component_t& c)
        {   
            constexpr const char* editor_camera_name = "__engine_editor_camera__";
            std::memcpy(c.name, editor_camera_name, std::strlen(editor_camera_name));
        });

    auto camera_comp = scene->add_component<engine_camera_component_t>(go_);
    scene->patch_component<engine_camera_component_t>(go_, [camera_comp](engine_camera_component_t& c)
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

    auto camera_transform_comp = scene->add_component<engine_tranform_component_t>(go_);
    scene->patch_component<engine_tranform_component_t>(go_, [](engine_tranform_component_t& c)
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
    if (ImGui::GetIO().WantCaptureMouse || ImGuizmo::IsUsingAny())
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
    auto tc = my_scene_->get_component<engine_tranform_component_t>(go_);
    auto cc = my_scene_->get_component<engine_camera_component_t>(go_);
    my_scene_->patch_component<engine_tranform_component_t>(go_, [tc, new_position, cc](engine_tranform_component_t& c)
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
        auto tc = my_scene_->get_component<engine_tranform_component_t>(go_);
        // Rotate the camera left and right
        sc_[1] += delta.x;

        // Rotate the camera up and down
        // Prevent the camera from turning upside down (1.5f = approx. Pi / 2)
        sc_[2] = std::clamp(sc_[2] + delta.y, -1.5f, 1.5f);

        const auto new_position = get_cartesian_coordinates(sc_);
        auto cc = my_scene_->get_component<engine_camera_component_t>(go_);

        my_scene_->patch_component<engine_tranform_component_t>(go_, [tc, new_position, cc](engine_tranform_component_t& c)
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
    auto tc = *my_scene_->get_component<engine_tranform_component_t>(go_);
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
    my_scene_->update_component<engine_tranform_component_t>(go_, tc);
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
        view.each([scene, this, &camera_data](const auto& entity)
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

void engine::ApplicationEditor::CameraContext::on_scene_update_post(Scene* scene, float dt)
{
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

void engine::SceneHierarchyContext::set_selected_entity(engine::Scene* scene, entt::entity e)
{
    if (selected_ == e)
    {
        return;
    }
    if (selected_ != entt::null)
    {
        scene->remove_component<engine::guizmo_component_t>(selected_);
        scene->remove_component<engine::outline_component_t>(selected_);
    }

    selected_ = e;
    if (selected_ != entt::null)
    {
        scene->add_component<engine::guizmo_component_t>(selected_);
        scene->add_component<engine::outline_component_t>(selected_);
    }

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

engine::viewport_t engine::ApplicationEditor::EditorWindowsContext::get_scene_render_viewport() const
{
    const auto dock_id_scene = ImGui::DockBuilderGetNode(ImGui::GetID(window_scene));
    const auto pos = dock_id_scene->Pos;
    const auto size = dock_id_scene->Size;
    return viewport_t(pos.x, pos.y, size.x, size.y);
}
