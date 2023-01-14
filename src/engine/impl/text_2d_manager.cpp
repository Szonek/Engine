#include "text_2d_manager.h"
#include "asset_store.h"
#include "engine.h"

#include <ft2build.h>
#include FT_FREETYPE_H  

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <cassert>
#include <iostream>

engine::TextManager::TextManager()
    : shader_font_(Shader("font.vs", "font.fs"))
    , current_font_idx_(1) // start with, since 0 is invalid index
{
    FT_Library ft_handle;
    if (FT_Init_FreeType(&ft_handle))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return;
    }
    font_handle_ = reinterpret_cast<FontImplHandle*>(ft_handle);

    float vertices[6][4] = {
        { 0.0, 1.0f, 0.0f, 0.0f },
        { 0.0, 0.0f, 0.0f, 1.0f },
        { 1.0, 0.0f, 1.0f, 1.0f },

        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 0.0f }
    };

    const auto* vertex_data = reinterpret_cast<const std::byte*>(vertices);
    std::array<Geometry::vertex_attribute_t, 2> vertex_layut;
    vertex_layut[0] = Geometry::vertex_attribute_t{ 0, 2, 4 * sizeof(float), 0 * sizeof(float), Geometry::vertex_attribute_t::Type::eFloat };
    vertex_layut[1] = Geometry::vertex_attribute_t{ 1, 2, 4 * sizeof(float), 2 * sizeof(float), Geometry::vertex_attribute_t::Type::eFloat };
    geometry_ = engine::Geometry(vertex_layut, { vertex_data, 6 * 4 * sizeof(float) }, 6);
}

engine::TextManager::TextManager(TextManager&& rhs)
    : shader_font_(std::move(rhs.shader_font_))
{
    std::swap(font_handle_, rhs.font_handle_);
}

engine::TextManager& engine::TextManager::operator=(TextManager&& rhs)
{
    if (this != &rhs)
    {
        std::swap(font_handle_, rhs.font_handle_);
        std::swap(shader_font_, rhs.shader_font_);
    }
    return *this;
}

engine::TextManager::~TextManager()
{
    if (font_handle_)
    {
        FT_Library ft_lib = reinterpret_cast<FT_Library>(font_handle_);
        FT_Done_FreeType(ft_lib);
    }
}

std::pair<bool, std::uint32_t> engine::TextManager::load_font_from_file(std::string_view file_name)
{
    if (!font_handle_)
    {
        return { false, ENGINE_INVALID_OBJECT_HANDLE};
    }
    auto ft_lib = reinterpret_cast<FT_Library>(font_handle_);
    FT_Face face_handle;

    const auto font_asset_data = AssetStore::get_instance().get_font_data(file_name);
    assert(font_asset_data.get_data_ptr() != nullptr && "Couldnt load font asset.");

    if (FT_New_Memory_Face(ft_lib, font_asset_data.get_data_ptr(), font_asset_data.get_size(), 0, &face_handle))
    {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return { false, ENGINE_INVALID_OBJECT_HANDLE };
    }
    else
    {
        characters_map fonts{};
        // set size to load glyphs as
        FT_Set_Pixel_Sizes(face_handle, 0, 48);

        atlas_t atlas = [&]()
        {
            atlas_t ret{};
            ret.padding = 3;
            // first pass <- calc atlas w and h
            for (unsigned char c = 0; c < max_chars_count_; c++)
            {
                // load character glyph 
                if (FT_Load_Char(face_handle, c, FT_LOAD_RENDER))
                {
                    assert("Failed to load glyph with FreeType library.");
                }
                const auto& glyph = face_handle->glyph;
                //FT_Render_Glyph(glyph, FT_RENDER_MODE_SDF);


                ret.height = std::max(ret.height, glyph->bitmap.rows);
                ret.width += glyph->bitmap.width + 2 * ret.padding;
            }
            ret.texture = Texture2D(
                ret.width,
                ret.height,
                false, nullptr, DataLayout::eR_U8, TextureAddressClampMode::eClampToEdge);

            return ret;
        }();

        std::uint32_t write_x_atlas_offset = atlas.padding;
        for (unsigned char c = 0; c < max_chars_count_; c++)
        {
            // load character glyph 
            if (FT_Load_Char(face_handle, c, FT_LOAD_RENDER))
            {
                assert("Failed to load glyph with FreeType library.");
            }
            const auto& glyph = face_handle->glyph;
            //FT_Render_Glyph(glyph, FT_RENDER_MODE_SDF);

            character_t character{};
            character.size = { glyph->bitmap.width, glyph->bitmap.rows };
            character.bearing = { glyph->bitmap_left, glyph->bitmap_top };
            character.advance = glyph->advance.x;
            if (glyph->bitmap.width != 0 && glyph->bitmap.rows != 0)
            {
                atlas.texture.upload_region(write_x_atlas_offset, 0, character.size[0], character.size[1], glyph->bitmap.buffer, DataLayout::eR_U8);
                character.offset_in_atlas_normalized[0] = static_cast<float>(write_x_atlas_offset) / static_cast<float>(atlas.width);
                character.offset_in_atlas_normalized[1] = 0;
                write_x_atlas_offset += character.size[0] + atlas.padding;
            }
            fonts[static_cast<std::size_t>(c)] = std::move(character);
        }
        atlases_[current_font_idx_] = std::move(atlas);
        fonts_[current_font_idx_] = std::move(fonts);
        return {true, current_font_idx_++};
    }

}

void engine::TextManager::render_text(RenderContext& rdx, std::string_view text, std::uint32_t font_idx, std::span<const float> parent_model_matrix, std::uint32_t screen_width, std::uint32_t screen_height)
{
    assert(font_idx < fonts_.size());
    assert(parent_model_matrix.size() == 16);

    if (text.empty())
    {
        return;
    }

    rdx.set_depth_test(false);
    rdx.set_blend_mode(true, RenderContext::BlendFactor::eSrcAlpha, RenderContext::BlendFactor::eOneMinusSrcAlpha, RenderContext::BlendFactor::eOne, RenderContext::BlendFactor::eZero);

    const auto screen_width_f = static_cast<float>(screen_width);
    const auto screen_height_f = static_cast<float>(screen_height);
    const auto projection = glm::ortho(0.0f, screen_width_f, 0.0f, screen_height_f, -100.0f, 100.0f);

    shader_font_.bind();
    shader_font_.set_uniform_mat_f4("projection", { glm::value_ptr(projection), sizeof(projection) / sizeof(float) });
    shader_font_.set_uniform_f4("glyph_color", std::array<float, 4>{1.0, 1.0, 1.0, 1.0f});

    auto& atlas = atlases_[font_idx];
    atlas.texture.bind(0);
    shader_font_.set_uniform_f2("atlas_size",
        std::array<float, 2>{static_cast<float>(atlas.width), static_cast<float>(atlas.height)});

    float cursor_x_pos = 0.0f;
    float cursor_y_pos = 0.0f;

    for (const auto c : text)
    {
        auto& font = fonts_[font_idx];
        const auto codepoint_idx = static_cast<std::size_t>(c);
        const auto& glyph = font[codepoint_idx];

        if (glyph.size[0] > 0)
        {
            const float xpos = cursor_x_pos + glyph.bearing[0];
            const float ypos = cursor_y_pos - (glyph.size[1] - glyph.bearing[1]);
            const float w = glyph.size[0];
            const float h = glyph.size[1];

            shader_font_.set_uniform_f2("glyph_size", std::array<float, 2>{w, h});
            shader_font_.set_uniform_f2("glyph_normalized_start_offset_in_atlas", glyph.offset_in_atlas_normalized);

            auto model_matrix = glm::make_mat4(parent_model_matrix.data());
            model_matrix = glm::translate(model_matrix, glm::vec3(xpos, ypos, 0.0f));
            model_matrix = glm::scale(model_matrix, glm::vec3(w, h, 1.0f));
            shader_font_.set_uniform_mat_f4("model_matrix", { glm::value_ptr(model_matrix), sizeof(model_matrix) / sizeof(float) });

            geometry_.bind();
            geometry_.draw(Geometry::Mode::eTriangles);
        }
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        cursor_x_pos += (glyph.advance >> 6); // bitshift by 6 to get value in pixels (2^6 = 64)
    }
    rdx.set_depth_test(true);
    rdx.set_blend_mode(false);
}
