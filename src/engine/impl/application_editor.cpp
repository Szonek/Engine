#include "application_editor.h"

#include "scene.h"
#include "math_helpers.h"
#include "logger.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_opengl3.h"

#include <string>
#include <map>



namespace
{

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

    if (ImGui::TreeNodeEx(node->name.c_str(), dispaly_flags))
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
    bool requires_update = ImGui::DragFloat3("Position", c.position, v_speed);

    glm::vec3 rot = glm::degrees(glm::eulerAngles(glm::make_quat(c.rotation)));
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(rot), v_speed))
    {
        requires_update = true;
        const auto final_rot = glm::normalize(glm::quat(glm::radians(rot)));
        std::memcpy(c.rotation, glm::value_ptr(final_rot), sizeof(c.rotation));
    }
    requires_update |= ImGui::DragFloat3("Scale", c.scale, v_speed);
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
            requires_component_updated |= ImGui::DragFloat3("Position", child.transform, 0.1f);

            glm::vec3 rot = glm::degrees(glm::eulerAngles(glm::make_quat(child.rotation_quaternion)));
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rot), 0.1f))
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
    assert(ImGui_ImplSDL3_InitForOpenGL(rdx_.get_sdl_window(), rdx_.get_sdl_gl_context()));
    assert(ImGui_ImplOpenGL3_Init());
}

engine::ApplicationEditor::~ApplicationEditor()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void engine::ApplicationEditor::on_frame_begine()
{
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
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

void engine::ApplicationEditor::on_scene_update(Scene* scene, float delta_time)
{
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

    static hierarchy_context_t ctx;
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::Begin("Scene Panel");

    if(ImGui::Button("Add entity"))
    {
        auto e = scene->create_new_entity();
        auto nc = scene->add_component<engine_name_component_t>(e);
        const auto new_name = "Entity " + std::to_string(static_cast<std::uint32_t>(e));
        std::memcpy(nc->name, new_name.c_str(), new_name.size());
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

    ImGui::Begin("Geometry inspecotr");
    ImGui::Text("Geometry inspector");
    ImGui::End();
}

bool engine::ApplicationEditor::is_mouse_enabled()
{
    return !ImGui::GetIO().WantCaptureMouse;
}
