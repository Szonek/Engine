#include "editor.h"
#include "scene.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_opengl3.h"

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
    ImGui::ShowDemoWindow();
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
