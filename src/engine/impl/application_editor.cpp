#include "application_editor.h"

#include "scene.h"
#include "math_helpers.h"
#include "logger.h"
#include "profiler.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_opengl3.h"

#include <string>
#include <map>
#include <numeric>

namespace
{
constexpr const char* g_editor_camera_name = "__engine__camera-editor__";
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

class hierarchy_context_t
{
public:
    void set_selected_entity(entt::entity e)
    {
        selected_ = e;
    }
    entt::entity get_selected_entity() const
    {
        return selected_;
    }

    bool has_selected_entity() const
    {
        return selected_ != entt::null;
    }

    void unselect_entity()
    {
        selected_ = entt::null;
    }

private:
    entt::entity selected_ = entt::null;
};

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

inline void display_node(entity_node_t* node, engine::Scene* scene, hierarchy_context_t& ctx)
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

    const auto entity_id = std::string(node->name) + "##" + std::to_string(static_cast<std::uint32_t>(node->entity));
    if (ImGui::TreeNodeEx(entity_id.c_str(), dispaly_flags))
    {
        // tooltip the id
        ImGui::SetItemTooltip("ID: %d", node->entity);

        // select entity with LMB
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            ctx.set_selected_entity(node->entity);
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
inline void display_component(std::string_view name, engine::Scene* scene, entt::entity entity, std::function<bool(T& comp)> fn)
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
            if (fn(comp))
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

bool display_transform_component(engine_tranform_component_t& c)
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

bool display_mesh_component(engine_mesh_component_t& c)
{
    bool enabled = !c.disable;
    if (ImGui::Checkbox("Enabled", &enabled))
    {
        c.disable = !c.disable;
    }
    ImGui::InputInt("Geometry ID", reinterpret_cast<std::int32_t*>(&c.geometry));
    return true;
}

bool display_light_component(engine_light_component_t& c)
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


bool display_material_component(engine_material_component_t& c)
{
    ImGui::InputInt("Material ID", reinterpret_cast<std::int32_t*>(&c.material));
    return true;
}

bool display_collider_component(engine_collider_component_t& c)
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

bool display_rigidbody_component(engine_rigid_body_component_t& c)
{
    bool requires_update = ImGui::SliderFloat("Mass", &c.mass, 0.0f, 100.0f);

    return requires_update;
}

bool display_camera_component(engine_camera_component_t& c)
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

void render_scene_hierarchy_panel(engine::Scene* scene, float delta_time)
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
        if (name.compare(g_editor_camera_name) == 0)
        {
            // dont' add camera editor to hierarchy, so user can't manipulate it
            continue;
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

    static hierarchy_context_t ctx;
    
    ImGui::Begin("Scene Panel");

    if (ImGui::Button("Add entity"))
    {
        auto e = scene->create_new_entity();
        auto nc = scene->add_component<engine_name_component_t>(e);
        const auto new_name = "Entity " + std::to_string(static_cast<std::uint32_t>(e));
        std::memcpy(nc->name, new_name.c_str(), new_name.size());
    }

    static bool phys_debug_draw_check = true;
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
                display_node(&f, scene, ctx);
            }
        }

        ImGui::TreePop();
    }
    /*
    here all components should be visible
        - the one which are not avaialbe in the entitiy (Grayd out) and a "+" button to add them
        - the one which are available in the entity (whatever color) and a "-" button to remove them
    */
    ImGui::SeparatorText("Entity properties.");
    if (ctx.has_selected_entity())
    {
        const auto selected = ctx.get_selected_entity();
        display_component<engine_tranform_component_t>("Transform", scene, selected, display_transform_component);
        display_component<engine_light_component_t>("Light", scene, selected, display_light_component);
        display_component<engine_camera_component_t>("Camera", scene, selected, display_camera_component);
        display_component<engine_mesh_component_t>("Mesh", scene, selected, display_mesh_component);
        display_component<engine_material_component_t>("Material", scene, selected, display_material_component);
        display_component<engine_collider_component_t>("Collider", scene, selected, display_collider_component);
        display_component<engine_rigid_body_component_t>("Rigid Body", scene, selected, display_rigidbody_component);
    }
    else
    {
        ImGui::Text("Select entity to display its components.");
    }

    ImGui::End(); // scene panel
}

} // namespace anonymous


engine::ApplicationEditor::ApplicationEditor(const engine_application_create_desc_t& desc, engine_result_code_t& out_code)
    : Application(desc, out_code)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
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
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::Begin("Editor Controllers");
    if (ImGui::Button("Enable/Disable Editor Controller"))
    {
        editor_controlling_scene_ = !editor_controlling_scene_;
    }
    assert(frame_begin_info.delta_time > 0.0f);
    static float avg_fps = 0; 
    static std::vector<float> fps_values(60);
    static std::size_t fps_idx = 0;
    fps_values[fps_idx % fps_values.size()] = 1000.0f / frame_begin_info.delta_time;
    fps_idx++;
    ImGui::Text("FPS: %d", static_cast<std::uint32_t>(std::accumulate(fps_values.begin(), fps_values.end(), 0) / (fps_idx < fps_values.size() ? fps_idx : fps_values.size())));
    ImGui::End();
}

void engine::ApplicationEditor::on_frame_end()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void engine::ApplicationEditor::on_sdl_event(SDL_Event e)
{
    ImGui_ImplSDL3_ProcessEvent(&e);
}


void engine::ApplicationEditor::on_scene_update_pre(Scene* scene, float delta_time)
{
    ENGINE_PROFILE_SECTION_N("editor-on_scene_update_pre");
    if (editor_controlling_scene_)
    {
        camera_context_.on_scene_update_pre(scene, delta_time);
    }
}

void engine::ApplicationEditor::on_scene_update_post(Scene* scene, float delta_time)
{
    ENGINE_PROFILE_SECTION_N("editor-on_scene_update_post");
    camera_context_.on_scene_update_post(scene, delta_time);
    render_scene_hierarchy_panel(scene, delta_time);
}

bool engine::ApplicationEditor::is_mouse_enabled()
{
    const bool editor_is_using_mouse = editor_controlling_scene_ || ImGui::GetIO().WantCaptureMouse;
    return !editor_is_using_mouse;
}

bool engine::ApplicationEditor::is_keyboard_enabled()
{
    const bool editor_is_using_keybord = editor_controlling_scene_ || ImGui::GetIO().WantCaptureKeyboard;
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
            std::memcpy(c.name, g_editor_camera_name, std::strlen(g_editor_camera_name));
        });

    auto camera_comp = scene->add_component<engine_camera_component_t>(go_);
    scene->patch_component<engine_camera_component_t>(go_, [camera_comp](engine_camera_component_t& c)
        {
            c.enabled = true;
            c.clip_plane_near = 0.1f;
            c.clip_plane_far = 1000.0f;
            c.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
            c.type_union.perspective_fov = 45.0f;
            c.target[0] = 0.0f;
            c.target[1] = 0.0f;
            c.target[2] = 0.0f;
        });

    auto camera_transform_comp = scene->add_component<engine_tranform_component_t>(go_);
    scene->patch_component<engine_tranform_component_t>(go_, [](engine_tranform_component_t& c)
        {
            c.position[0] = 1.0f;
            c.position[1] = 1.0f;
            c.position[2] = 1.0f;
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
    auto cc = my_scene_->get_component<engine_camera_component_t>(go_);
    if (!cc->enabled)
    {
        return;
    }
    const auto mouse_coords = app_->mouse_get_coords();

    const auto dx = mouse_coords.x - mouse_coords_prev_.x;
    const auto dy = mouse_coords.y - mouse_coords_prev_.y;

    if (mouse_coords.x != mouse_coords_prev_.x || mouse_coords.y != mouse_coords_prev_.y)
    {
        mouse_coords_prev_ = mouse_coords;
    }

    const float move_speed = 1.0f * dt;

    const bool lmb = app_->mouse_is_button_down(ENGINE_MOUSE_BUTTON_LEFT);
    const bool rmb = app_->mouse_is_button_down(ENGINE_MOUSE_BUTTON_RIGHT);
    const bool mmb = app_->mouse_is_button_down(ENGINE_MOUSE_BUTTON_MIDDLE);

    //if (app_->keyboard_is_key_down(ENGINE_KEYBOARD_KEY_LSHIFT))
    {
        if (lmb)
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
