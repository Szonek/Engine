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
