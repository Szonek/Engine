#include "editor.h"
#include "scene.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_opengl3.h"

#include <string>
#include <map>

namespace
{
struct hierarchy_context_t
{
    entt::entity selected = entt::null;
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
    uint32_t dispaly_flags = ctx.selected == node->entity ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
    if (node->children.empty()) // if is leaf
    {
        dispaly_flags |= ImGuiTreeNodeFlags_Leaf;
    }
    else
    {
        dispaly_flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
    }
    if (ImGui::TreeNodeEx(node->name.c_str(), dispaly_flags))
    {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            ctx.selected = node->entity;
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

    ImGui::Begin("Scene Hierarchy");  
    static hierarchy_context_t ctx;
    for (auto& [e, f] : entity_map)
    {
        if (!f.displayed && !f.parent)
        {
            display_node(&f, scene, ctx);
        }
    }
    ImGui::End();
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
