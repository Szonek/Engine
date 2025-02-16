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
{
    Rml::Initialise();
    // create context with some aribtrary name and dimension.  (dimensions wil lbe update in update(..))
    const auto window_size_pixels = rdx_.get_window_size_in_pixels();
    ui_rml_context_ = Rml::CreateContext("app", Rml::Vector2i(window_size_pixels.width, window_size_pixels.height));
    assert(ui_rml_context_);
}

engine::UiManager::UiManager(UiManager&& rhs)
    : rdx_(rhs.rdx_)
{
}

engine::UiManager& engine::UiManager::operator=(UiManager&& rhs)
{
    if (this != &rhs)
    {
        std::swap(rdx_, rhs.rdx_);
    }
    return *this;
}

engine::UiManager::~UiManager()
{
    if (ui_rml_context_)
    {
        Rml::RemoveContext(ui_rml_context_->GetName());
        Rml::Shutdown();
    }
}

engine::UiDataHandle engine::UiManager::create_data_handle(std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings)
{
   return UiDataHandle(ui_rml_context_, name, bindings);
}

engine::UiDocument engine::UiManager::load_document_from_file(std::string_view file_name)
{
    return UiDocument(ui_rml_context_, file_name);
}

bool engine::UiManager::load_font_from_file(std::string_view file_name, std::string_view /*handle_name*/)
{
    const auto font_path = AssetStore::get_instance().get_font_base_path() / file_name;
    const auto success = Rml::LoadFontFace(font_path.string(), true);
    return success;
}

void engine::UiManager::parse_sdl_event(SDL_Event ev)
{
    RmlSDL::InputEventHandler(ui_rml_context_, rdx_.get_sdl_window(), ev);
}

void engine::UiManager::update_state_and_render()
{
    ui_rml_context_->Update();
    rdx_.begin_frame_ui_rendering();
    ui_rml_context_->Render();
    rdx_.end_frame_ui_rendering();
}
