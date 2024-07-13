#include "text_renderer.h"
#include "logger.h"
#include "asset_store.h"


#include <ft2build.h>
#include FT_FREETYPE_H  

#include <fmt/format.h>

#include <cassert>

engine::Font::Font()
    : ft_handle_(nullptr)
{
}

engine::Font::Font(std::string_view file_name)
    : ft_handle_(nullptr)
{
    FT_Library ft_handle;
    if (FT_Init_FreeType(&ft_handle))
    {
        log::log(log::LogLevel::eCritical, "Could not init FreeType Library");
        return;
    }
    else
    {
        ft_handle_ = reinterpret_cast<void*>(ft_handle);
    }

    const auto font_asset_data = AssetStore::get_instance().get_font_data(file_name);
    assert(font_asset_data.get_data_ptr() != nullptr && "Couldnt load font asset.");

    FT_Face face_handle;
    if (FT_New_Memory_Face(ft_handle, font_asset_data.get_data_ptr(), font_asset_data.get_size(), 0, &face_handle))
    {
        log::log(log::LogLevel::eError, fmt::format("Failed to load font {}\n", file_name));
        throw std::runtime_error("Can't create font.");
    }
    else
    {
        // set size to load glyphs as
        FT_Set_Pixel_Sizes(face_handle, 0, 48);

        atlas_ = [&]()
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

            std::uint32_t write_x_atlas_offset = atlas_.padding;
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
                    atlas_.texture.upload_region(write_x_atlas_offset, 0, character.size[0], character.size[1], glyph->bitmap.buffer, DataLayout::eR_U8);
                    character.offset_in_atlas_normalized[0] = static_cast<float>(write_x_atlas_offset) / static_cast<float>(atlas_.width);
                    character.offset_in_atlas_normalized[1] = 0;
                    write_x_atlas_offset += character.size[0] + atlas_.padding;
                }
                glyph_map_[static_cast<std::size_t>(c)] = std::move(character);
            }
    }
}

engine::Font::Font(Font&& rhs) noexcept
    : ft_handle_(rhs.ft_handle_)
{
    rhs.ft_handle_ = nullptr;
}

engine::Font& engine::Font::operator=(Font&& rhs)  noexcept
{
    if (this != &rhs)
    {
        std::swap(ft_handle_, rhs.ft_handle_);
    }
    return *this;
}

engine::Font::~Font()
{
    if (ft_handle_)
    {
        FT_Library ft_lib = reinterpret_cast<FT_Library>(ft_handle_);
        FT_Done_FreeType(ft_lib);
    }
}
