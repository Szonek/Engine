#pragma once
#include "components/text_component.h"
#include "components/rect_transform_component.h"
#include "components/image_component.h"
#include "graphics.h"
#include "engine.h"

#include <array>

#include <glm/glm.hpp>

namespace Rml
{
    class Context;
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

    engine_ui_document_data_handle_t create_ui_document_data_handle(std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings);
    void destroy_ui_document_data_handle(engine_ui_document_data_handle_t& handle);

    engine_ui_document_t load_ui_document_from_file(std::string_view file_name);
    std::uint32_t load_font_from_file(std::string_view file_name, std::string_view handle_name);
    std::uint32_t get_font(std::string_view name) const;

    void parse_sdl_event(SDL_Event ev);

    void begin_frame();
    void end_frame();

    void render_text(const engine_text_component_t& text_comp, const engine_rect_tranform_component_t& transform);

    void render_image(const engine_image_component_t& img_comp, const engine_rect_tranform_component_t& transform);

private:
    struct FontImplHandle;

    struct atlas_t
    {
        std::uint32_t width{ 0u };
        std::uint32_t height{ 0u };
        std::uint32_t padding{ 0u };
        Texture2D texture;
        std::string font_name;
    };

    struct character_t
    {
        std::array<std::uint32_t, 2> size;
        std::array<std::int32_t, 2> bearing;
        std::array<float, 2> offset_in_atlas_normalized;
        std::uint32_t advance;
    };
    static constexpr inline std::size_t max_chars_count_ = 128;
    using characters_map = std::array<character_t, max_chars_count_>;
private:
    RenderContext& rdx_;
    glm::mat4 ortho_projection;
    float current_window_width_ = 0.0f;
    float current_window_height_ = 0.0f;
    FontImplHandle* font_handle_ = nullptr;
    std::array<characters_map, 64> fonts_;
    std::array<atlas_t, 64> atlases_;
    std::uint32_t current_font_idx_ = 0;
    Shader shader_font_;
    Shader shader_image_;
    Geometry geometry_;

    Rml::Context* ui_rml_context_;
};


}  // namespace engine