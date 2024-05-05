#pragma once
#include "graphics.h"
#include "engine.h"
#include "ui_document.h"

#include <array>

#include <glm/glm.hpp>

namespace Rml
{
    class Context;
    class ElementDocument;
}

namespace engine
{
class UiManager
{
public:
    UiManager(RenderContext& rdx);
    UiManager(const UiManager& rhs) = delete;
    UiManager& operator=(const UiManager& rhs) = delete;
    UiManager(UiManager&&);
    UiManager& operator=(UiManager&& rhs);
    ~UiManager();

    engine::UiDataHandle create_data_handle(std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings);
    UiDocument load_document_from_file(std::string_view file_name);

    bool load_font_from_file(std::string_view file_name, std::string_view handle_name);

    void parse_sdl_event(SDL_Event ev);

    void update_state_and_render();

private:
    RenderContext& rdx_;
    Rml::Context* ui_rml_context_;
};


}  // namespace engine