#include "editor.h"
#include "scene.h"
#include "math_helpers.h"

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



inline void display_node(entity_node_t* node, engine::Scene* scene, hierarchy_context_t& ctx)
{
    node->displayed = true;
    uint32_t dispaly_flags = ctx.get_selected_entity() == node->entity ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
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
    if (scene->has_component<engine_tranform_component_t>(entity))
    {
        auto tc = scene->get_component<engine_tranform_component_t>(entity);
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_None))
        {
            const float v_speed = 0.1f;
            ImGui::DragFloat3("Position", tc->position, v_speed);

            glm::vec3 rot = glm::degrees(glm::eulerAngles(glm::make_quat(tc->rotation)));
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rot), v_speed))
            {
                const auto final_rot = glm::quat(glm::radians(rot));
                std::memcpy(tc->rotation, glm::value_ptr(final_rot), sizeof(tc->rotation));
            }
            ImGui::DragFloat3("Scale", tc->scale, v_speed);
        }
    }
}

void display_mesh_component(engine::Scene* scene, entt::entity entity)
{
    if (scene->has_component<engine_mesh_component_t>(entity))
    {
        auto c = scene->get_component<engine_mesh_component_t>(entity);
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_None))
        {
            bool enabled = !c->disable;
            if (ImGui::Checkbox("Enabled", &enabled))
            {
                c->disable = !c->disable;
            }
            ImGui::InputInt("Geometry ID", reinterpret_cast<std::int32_t*>(&c->geometry));
        }
    }
}

void display_material_component(engine::Scene* scene, entt::entity entity)
{
    if (scene->has_component<engine_material_component_t>(entity))
    {
        auto c = scene->get_component<engine_material_component_t>(entity);
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_None))
        {
            ImGui::InputInt("Material ID", reinterpret_cast<std::int32_t*>(&c->material));
        }
    }
}

void display_camera_component(engine::Scene* scene, entt::entity entity)
{
    if (scene->has_component<engine_camera_component_t>(entity))
    {
        auto c = scene->get_component<engine_camera_component_t>(entity);
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_None))
        {
            // is it enabled?
            ImGui::Checkbox("Enabled", &c->enabled);

            // type
            std::array<const char*, 2> items = {"Orthographic",  "Perspective"};
            std::int32_t selected_type = c->type;
            if (ImGui::ListBox("Type", &selected_type, items.data(), items.size()))
            {
                c->type = static_cast<engine_camera_projection_type_t>(selected_type);
            }

            // fov or scale, based on type
            if (c->type == ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE)
            {
                ImGui::DragFloat("FOV", &c->type_union.perspective_fov, 0.1f);
            }
            else
            {
                ImGui::DragFloat("Scale", &c->type_union.orthographics_scale, 0.1f);
            }

            // target
            ImGui::DragFloat3("Target", c->target, 0.1f);
            
            //viewport rect
            ImGui::DragFloat4("Viewport", &c->viewport_rect.x, 0.1f);

            // pitch, yaw, roll
            ImGui::DragFloat("Pitch", &c->pitch, 0.1f);
            ImGui::DragFloat("Yaw", &c->yaw, 0.1f);
            ImGui::DragFloat("Roll", &c->roll, 0.1f);

            // clip planes
            ImGui::DragFloat("Near Clip Plane", &c->clip_plane_near, 0.1f);
            ImGui::DragFloat("Far Clip Plane", &c->clip_plane_far, 0.1f);
        }
    }
}

}   // namespace anonymous

engine::Editor::Editor(SDL_Window* wnd, SDL_GLContext gl_ctx)
    : is_enabled_(true)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

    // Setup Platform/Renderer backends
    assert(ImGui_ImplSDL3_InitForOpenGL(wnd, gl_ctx));
    assert(ImGui_ImplOpenGL3_Init());
}

engine::Editor::Editor(Editor&& rhs) noexcept
{
    std::swap(is_enabled_, rhs.is_enabled_);
}

engine::Editor& engine::Editor::operator=(Editor&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::swap(is_enabled_, rhs.is_enabled_);
    }
    return *this;
}

engine::Editor::~Editor()
{
    if (!is_enabled_)
    {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

}

void engine::Editor::begin_frame()
{
    if (!is_enabled_)
    {
        return;
    }
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
}

void engine::Editor::render_scene_hierarchy(Scene* scene)
{
    if (!is_enabled_)
    {
        return;
    }

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

    ImGui::SeparatorText("Entity properties and components.");
    if (ctx.has_selected_entity())
    {
        const auto selected = ctx.get_selected_entity();
        display_transform_component(scene, selected);
        display_camera_component(scene, selected);
        display_mesh_component(scene, selected);
        display_material_component(scene, selected);
    }
    else
    {
        ImGui::Text("Select entity to display its components.");
    }

    ImGui::End(); // scene panel
}

void engine::Editor::end_frame()
{
    if (!is_enabled_)
    {
        return;
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void engine::Editor::handle_event(SDL_Event& ev)
{
    if (!is_enabled_)
    {
        return;
    }
    ImGui_ImplSDL3_ProcessEvent(&ev);
}

bool engine::Editor::wants_to_capture_mouse()
{
    if (!is_enabled_)
    {
        return false;
    }
    return ImGui::GetIO().WantCaptureMouse;
}
