#pragma once
#include "graphics.h"

#include <array>
#include <string_view>

namespace engine
{
class Font
{
public:
    struct character_t
    {
        std::array<std::uint32_t, 2> size;
        std::array<std::int32_t, 2> bearing;
        std::array<float, 2> offset_in_atlas_normalized;
        std::uint32_t advance;
    };

public:
    Font();
    Font(std::string_view file_name);
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&&) noexcept;
    Font& operator=(Font&&) noexcept;
    ~Font();

    Texture2D& get_texture() { return atlas_.texture; }
    const character_t& get_character_info(char c) const { return glyph_map_[static_cast<std::size_t>(c)]; }
private:
    struct atlas_t
    {
        std::uint32_t width{ 0u };
        std::uint32_t height{ 0u };
        std::uint32_t padding{ 0u };
        Texture2D texture;
    };

    static constexpr inline std::size_t max_chars_count_ = 128;
    using characters_map = std::array<character_t, max_chars_count_>;

private:
    void* ft_handle_;
    atlas_t atlas_;
    characters_map glyph_map_;
};


}  // namespace engine