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

bool engine::UiManager::load_font_from_file(std::string_view file_name, std::string_view handle_name)
{
    const auto font_path = AssetStore::get_instance().get_font_base_path() / file_name;
    const auto success = Rml::LoadFontFace(font_path.string(), true);

    //if (!font_handle_)
    //{
    //    return ENGINE_INVALID_OBJECT_HANDLE;
    //}
    //auto ft_lib = reinterpret_cast<FT_Library>(font_handle_);
    //FT_Face face_handle;

    //const auto font_asset_data = AssetStore::get_instance().get_font_data(file_name);
    //assert(font_asset_data.get_data_ptr() != nullptr && "Couldnt load font asset.");

    //if (FT_New_Memory_Face(ft_lib, font_asset_data.get_data_ptr(), font_asset_data.get_size(), 0, &face_handle))
    //{
    //    log::log(log::LogLevel::eError, fmt::format("Failed to load font {}\n", file_name));
    //    return ENGINE_INVALID_OBJECT_HANDLE;
    //}
    //else
    //{
    //    characters_map fonts{};
    //    // set size to load glyphs as
    //    FT_Set_Pixel_Sizes(face_handle, 0, 48);

    //    atlas_t atlas = [&]()
    //        {
    //            atlas_t ret{};
    //            ret.padding = 3;
    //            // first pass <- calc atlas w and h
    //            for (unsigned char c = 0; c < max_chars_count_; c++)
    //            {
    //                // load character glyph 
    //                if (FT_Load_Char(face_handle, c, FT_LOAD_RENDER))
    //                {
    //                    assert("Failed to load glyph with FreeType library.");
    //                }
    //                const auto& glyph = face_handle->glyph;
    //                //FT_Render_Glyph(glyph, FT_RENDER_MODE_SDF);


    //                ret.height = std::max(ret.height, glyph->bitmap.rows);
    //                ret.width += glyph->bitmap.width + 2 * ret.padding;
    //            }
    //            ret.texture = Texture2D(
    //                ret.width,
    //                ret.height,
    //                false, nullptr, DataLayout::eR_U8, TextureAddressClampMode::eClampToEdge);
    //            ret.s = handle_name;
    //            return ret;
    //        }();

    //        std::uint32_t write_x_atlas_offset = atlas.padding;
    //        for (unsigned char c = 0; c < max_chars_count_; c++)
    //        {
    //            // load character glyph 
    //            if (FT_Load_Char(face_handle, c, FT_LOAD_RENDER))
    //            {
    //                assert("Failed to load glyph with FreeType library.");
    //            }
    //            const auto& glyph = face_handle->glyph;
    //            //FT_Render_Glyph(glyph, FT_RENDER_MODE_SDF);

    //            character_t character{};
    //            character.size = { glyph->bitmap.width, glyph->bitmap.rows };
    //            character.bearing = { glyph->bitmap_left, glyph->bitmap_top };
    //            character.advance = glyph->advance.x;
    //            if (glyph->bitmap.width != 0 && glyph->bitmap.rows != 0)
    //            {
    //                atlas.texture.upload_region(write_x_atlas_offset, 0, character.size[0], character.size[1], glyph->bitmap.buffer, DataLayout::eR_U8);
    //                character.offset_in_atlas_normalized[0] = static_cast<float>(write_x_atlas_offset) / static_cast<float>(atlas.width);
    //                character.offset_in_atlas_normalized[1] = 0;
    //                write_x_atlas_offset += character.size[0] + atlas.padding;
    //            }
    //            fonts[static_cast<std::size_t>(c)] = std::move(character);
    //        }
    //        atlases_[current_font_idx_] = std::move(atlas);
    //        fonts_[current_font_idx_] = std::move(fonts);

    //        return current_font_idx_++;
    //}

    return success;
}

void engine::UiManager::parse_sdl_event(SDL_Event ev)
{
    RmlSDL::InputEventHandler(ui_rml_context_, ev);
}

void engine::UiManager::update_state_and_render()
{
    ui_rml_context_->Update();
    rdx_.begin_frame_ui_rendering();
    ui_rml_context_->Render();
    rdx_.end_frame_ui_rendering();
}
