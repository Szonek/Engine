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


inline void display_node(entity_node_t* node, const engine::Scene* scene, hierarchy_context_t& ctx)
{
    node->displayed = true;
    uint32_t dispaly_flags = ctx.get_selected_entity() == node->entity ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
    dispaly_flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (node->children.empty()) // if is leaf
    {
        dispaly_flags |= ImGuiTreeNodeFlags_Leaf;
        dispaly_flags |= ImGuiTreeNodeFlags_Bullet;
    }
    else
    {
        dispaly_flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
    }

    if (ImGui::TreeNodeEx(node->name.c_str(), dispaly_flags))
    {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            ctx.set_selected_entity(node->entity);
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

void display_transform_component(engine::Scene* scene, entt::entity entity)
{
    const bool has_component = scene->has_component<engine_tranform_component_t>(entity);
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowItemOverlap))
    {
        if (has_component)
        {
            auto c = *scene->get_component<engine_tranform_component_t>(entity);
            const float v_speed = 0.1f;
            ImGui::DragFloat3("Position", c.position, v_speed);

            glm::vec3 rot = glm::degrees(glm::eulerAngles(glm::make_quat(c.rotation)));
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rot), v_speed))
            {
                const auto final_rot = glm::quat(glm::radians(rot));
                std::memcpy(c.rotation, glm::value_ptr(final_rot), sizeof(c.rotation));
            }
            ImGui::DragFloat3("Scale", c.scale, v_speed);
            scene->update_component(entity, c);
        }
    }
    else
    {
        ImGui::SameLine();
        if (has_component)
        {
            if (ImGui::Button("Remove"))
            {
                scene->remove_component<engine_tranform_component_t>(entity);
            }
        }
        else
        {
            if (ImGui::Button("Add"))
            {
                scene->add_component<engine_tranform_component_t>(entity);
            }
        }
    }

}

void display_mesh_component(engine::Scene* scene, entt::entity entity)
{
    if (scene->has_component<engine_mesh_component_t>(entity))
    {
        auto c = *scene->get_component<engine_mesh_component_t>(entity);
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_None))
        {
            bool enabled = !c.disable;
            if (ImGui::Checkbox("Enabled", &enabled))
            {
                c.disable = !c.disable;
            }
            ImGui::InputInt("Geometry ID", reinterpret_cast<std::int32_t*>(&c.geometry));
            scene->update_component(entity, c);
        }
    }
}

void display_material_component(engine::Scene* scene, entt::entity entity)
{
    if (scene->has_component<engine_material_component_t>(entity))
    {
        auto c = *scene->get_component<engine_material_component_t>(entity);
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_None))
        {
            ImGui::InputInt("Material ID", reinterpret_cast<std::int32_t*>(&c.material));
            scene->update_component(entity, c);
        }
    }
}

void display_collider_component(engine::Scene* scene, entt::entity entity)
{
    auto print_collider_type = [](engine_collider_type_t type)
     {
        switch (type)
        {
            case ENGINE_COLLIDER_TYPE_NONE: return "None";
            case ENGINE_COLLIDER_TYPE_BOX: return "Box";
            case ENGINE_COLLIDER_TYPE_SPHERE: return "Sphere";
            case ENGINE_COLLIDER_TYPE_COMPOUND: return "Compound";
            default: return "Unknown";
        }
    };

    if (scene->has_component<engine_collider_component_t>(entity))
    {
        auto c = *scene->get_component<engine_collider_component_t>(entity);
        bool requires_component_updated = false;
        if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_None))
        {
            // list of types
            const char* items[] = { "None", "Box", "Sphere", "Compound" };
            std::int32_t selected_type = c.type;
            if (ImGui::ListBox("Type", &selected_type, items, std::size(items)))
            {
                requires_component_updated = true;
                c.type = static_cast<engine_collider_type_t>(selected_type);
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
                engine::log::log(engine::log::LogLevel::eError, "Compound collider not implemented for editor yet\n");
                c.type = ENGINE_COLLIDER_TYPE_BOX; // to avoid crash, remove when this code will support compud collider
                //for (std::size_t i = 0; i < ENGINE_COMPOUND_COLLIDER_MAX_CHILD_COLLIDERS; i++)
                //{
                    //auto& child = c.collider.compound.children[i];
                    //if (child.type != ENGINE_COLLIDER_TYPE_BOX)
                    //{
                    //    continue;
                    //}
                    //ImGui::Text("Child %d", i);
                    //std::int32_t child_selected_type = c.type;
                    //if (ImGui::ListBox("Child Type", &child_selected_type, items, std::size(items)))
                    //{
                    //    requires_component_updated = true;
                    //    c.type = static_cast<engine_collider_type_t>(selected_type);
                    //}
                    //ImGui::DragFloat3("Position", child.transform, 0.1f);
                    //ImGui::DragFloat3("Rotation", child.rotation, 0.1f);
                    //ImGui::DragFloat3("Scale", child.scale, 0.1f);
                //}
            }
            // trigger
            requires_component_updated |= ImGui::Checkbox("Is Trigger", &c.is_trigger);
            // bouncies
            requires_component_updated |= ImGui::DragFloat("Bounciness", &c.bounciness, 0.1f);
            // friction
            requires_component_updated |= ImGui::DragFloat("Friction", &c.friction_static, 0.1f);

            if (requires_component_updated)
            {
                scene->update_component(entity, c);
            }
        }
    }
}

void display_camera_component(engine::Scene* scene, entt::entity entity)
{
    if (scene->has_component<engine_camera_component_t>(entity))
    {
        auto c = *scene->get_component<engine_camera_component_t>(entity);
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_None))
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
            scene->update_component(entity, c);
        }
    }
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
        std::string name = "Entity " + std::to_string(static_cast<std::uint32_t>(e));
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

    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::Begin("Scene Panel");

    ImGui::SeparatorText("Hierarchy");
    static hierarchy_context_t ctx;

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ctx.unselect_entity();
    }

    for (auto& [e, f] : entity_map)
    {
        if (!f.displayed && !f.parent)
        {
            display_node(&f, scene, ctx);
        }
    }

    /*
    here all components should be visible
        - the one which are not avaialbe in the entitiy (Grayd out) and a "+" button to add them
        - the one which are available in the entity (whatever color) and a "-" button to remove them
    */
    ImGui::SeparatorText("Entity properties and components.");
    if (ctx.has_selected_entity())
    {
        const auto selected = ctx.get_selected_entity();
        display_transform_component(scene, selected);
        display_camera_component(scene, selected);
        display_mesh_component(scene, selected);
        display_material_component(scene, selected);
        display_collider_component(scene, selected);
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
