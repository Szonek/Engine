#include "ui_manager.h"
#include "asset_store.h"
#include "engine.h"
#include "logger.h"
#include "math_helpers.h"

#include <RmlUi/Core.h>

#include <fmt/format.h>

#include <RmlUi/Core.h>
#include <RmlUi/Core/ID.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Debugger.h>

#include <cassert>
#include <iostream>


engine::UiManager::UiManager(RenderContext& rdx)
    : rdx_(rdx)
    , current_font_idx_(ENGINE_INVALID_OBJECT_HANDLE) // start with, since 0 is invalid index
{
    current_font_idx_++;
    //Rml::Initialise();
    // create context with some aribtrary name and dimension.  (dimensions wil lbe update in update(..))
    //const auto window_size_pixels = rdx_.get_window_size_in_pixels();
    //ui_rml_context_ = Rml::CreateContext("app", Rml::Vector2i(window_size_pixels.width, window_size_pixels.height));
    //assert(ui_rml_context_);
}

engine::UiManager::UiManager(UiManager&& rhs)
    : rdx_(rhs.rdx_)
{
}

engine::UiManager& engine::UiManager::operator=(UiManager&& rhs)
{
    if (this != &rhs)
    {
    }
    return *this;
}

engine::UiManager::~UiManager()
{
    if (ui_rml_sdl_interface_)
    {
        delete ui_rml_sdl_interface_;
    }
    if (ui_rml_gl3_renderer_)
    {
        delete ui_rml_gl3_renderer_;
    }

    if (ui_rml_context_)
    {
        Rml::RemoveContext(ui_rml_context_->GetName());
        Rml::Shutdown();
    }
}

engine::UiDataHandle engine::UiManager::create_data_handle(std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings)
{
   auto constructor = ui_rml_context_->CreateDataModel(name.data());
   return UiDataHandle(&constructor, bindings);
}

engine::UiDocument engine::UiManager::load_document_from_file(std::string_view file_name)
{
    Rml::ElementDocument* document = ui_rml_context_->LoadDocument((AssetStore::get_instance().get_ui_docs_base_path() / file_name).string());
    return UiDocument(document);
}

std::uint32_t engine::UiManager::load_font_from_file(std::string_view file_name, std::string_view /*handle_name*/)
{
    const auto font_path = AssetStore::get_instance().get_font_base_path() / file_name;
    bool success = Rml::LoadFontFace(font_path.string(), true);
    if (success)
    {
        return current_font_idx_++;
    }
    return ENGINE_INVALID_OBJECT_HANDLE;
}

std::uint32_t engine::UiManager::get_font(std::string_view /*name*/) const
{
    assert(false && "Not implemented!");
    return std::uint32_t();
}

void engine::UiManager::parse_sdl_event(SDL_Event ev)
{
    RmlSDL::InputEventHandler(ui_rml_context_, ev);
}

void engine::UiManager::update_state_and_render()
{
    // update
    ui_rml_context_->Update();

    // render
    ui_rml_gl3_renderer_->BeginFrame();
    ui_rml_context_->Render();
    ui_rml_gl3_renderer_->EndFrame();
}
