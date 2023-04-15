#pragma once
#include "graphics.h"

#include <array>

namespace engine
{
class TextManager
{
public:
    TextManager();
    TextManager(const TextManager& rhs) = delete;
    TextManager& operator=(const TextManager& rhs) = delete;
    TextManager(TextManager&&);
    TextManager& operator=(TextManager&& rhs);
    ~TextManager();

    std::uint32_t load_font_from_file(std::string_view file_name, std::string_view handle_name);
    std::uint32_t get_font(std::string_view name) const;

    void render_text(RenderContext& rdx, std::string_view text, std::uint32_t font_idx, std::span<const float> parent_model_matrix, std::uint32_t screen_width, std::uint32_t screen_height);

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
    FontImplHandle* font_handle_ = nullptr;
    std::array<characters_map, 64> fonts_;
    std::array<atlas_t, 64> atlases_;
    std::uint32_t current_font_idx_ = 0;
    Shader shader_font_;
    Geometry geometry_;
};


}  // namespace engine