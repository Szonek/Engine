#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <span>
#include <vector>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace engine
{
struct viewport_t
{
	std::uint32_t x;
	std::uint32_t y;
	std::uint32_t width;
	std::uint32_t height;
};

enum class DataLayout
{
    eRGBA_U8 = 0,
    eRGB_U8 = 1,
    eR_U8 = 2,

    // ..
    // ..
    // ..
    eRGBA_FP32,
    eR_FP32,
    eCount
};

enum class TextureAddressClampMode
{
    eClampToEdge = 0,
    //eClampToBorder = 1,
    // ...
    eCount
};

class Shader
{
public:
	enum class Type
	{
		eVertex = 0,
		eFragment = 1,
		eCount
	};
public:
	Shader(std::string_view vertex_shader_name, std::string fragment_shader_name);
	Shader(const Shader& rhs) = delete;
	Shader(Shader&& rhs) noexcept = default;
	Shader& operator=(const Shader& rhs) = delete;
	Shader& operator=(Shader&& rhs)  noexcept = default;

	~Shader();

	void bind() const;
	void set_uniform_f4(std::string_view name, std::span<const float> host_data);
	void set_uniform_f2(std::string_view name, std::span<const float> host_data);
	void set_uniform_ui2(std::string_view name, std::span<const std::uint32_t> host_data);
	void set_uniform_mat_f4(std::string_view name, std::span<const float> host_data);

private:
	std::int32_t get_uniform_location(std::string_view name);
	void compile_and_attach_to_program(std::uint32_t shader, std::string_view source);

private:
	std::uint32_t vertex_shader_;
	std::uint32_t fragment_shader_;
	std::uint32_t program_;

	std::unordered_map<std::string, std::int32_t> uniforms_locations_;
};



class Texture2D
{
public:
	Texture2D() = default;
	Texture2D(std::uint32_t width, std::uint32_t height, bool generate_mipmaps, const void* data, DataLayout layout, TextureAddressClampMode clamp_mode);
	Texture2D(std::string_view texture_name, bool generate_mipmaps);

	Texture2D(const Texture2D& rhs) = delete;
	Texture2D(Texture2D&& rhs) noexcept;
	Texture2D& operator=(const Texture2D& rhs) = delete;
	Texture2D& operator=(Texture2D&& rhs)  noexcept;

	~Texture2D();

    bool is_valid() const;

    bool upload_region(std::uint32_t x_pos, std::uint32_t y_pos, std::uint32_t width, std::uint32_t height, const void* data, DataLayout layout);

	void bind(std::uint32_t slot) const;

private:
	std::uint32_t texture_ = 0;
};

class Geometry
{
public:
	enum Mode
	{
		eTriangles = 0,
		eCount
	};

	struct vertex_attribute_t
	{
		enum class Type
		{
			eFloat = 0,
			eCount
		};
		std::uint32_t index = 0;
		std::uint32_t size = 0;
		std::uint32_t stride = 0;
		std::size_t offset = 0;
		Type type;
	};

public:
	Geometry() = default;
	Geometry(std::span<const vertex_attribute_t> vertex_layout, std::span<const std::byte> vertex_data, std::size_t vertex_count, std::span<const std::uint32_t> index_data = {});
	Geometry(const Geometry& rhs) = delete;
	Geometry(Geometry&& rhs) noexcept;
	Geometry& operator=(const Geometry& rhs) = delete;
	Geometry& operator=(Geometry&& rhs) noexcept;

	~Geometry();

	void bind() const;
	void draw(Mode mode) const;

private:
	std::uint32_t vbo_{0}; // vertex buffer
	std::uint32_t ibo_{0}; // index buffer
	std::uint32_t vao_{0};
	std::uint32_t vertex_count_{0};
	std::uint32_t index_count_{0};
};

class RenderContext
{
public:
	enum class PolygonFaceType
	{
		eFrontAndBack = 0,
		eCount
	};

	enum class PolygonMode
	{
		eFill = 0,
		eLine = 1,
		eCount
	};

    enum class BlendFactor
    {
        eZero = 0,
        eOne = 1,
        eSrcAlpha,
        eOneMinusSrcAlpha,
        eCount
    };

    struct window_size_t
    {
        std::int32_t width;
        std::int32_t height;
    };

public:
	RenderContext(std::string_view window_name, viewport_t init_size, bool init_fullscreen);
	
	RenderContext(const RenderContext&) = delete;
	RenderContext(RenderContext&& rhs) noexcept;
	RenderContext& operator=(const RenderContext&) = delete;
	RenderContext& operator=(RenderContext&& rhs) noexcept;
	~RenderContext();

    window_size_t get_window_size_in_pixels() const;

	void set_viewport(const viewport_t& viewport);
	void set_clear_color(float r, float g, float b, float a);
	void set_polygon_mode(PolygonFaceType face, PolygonMode mode);
    void set_depth_test(bool flag);

    void set_blend_mode(bool enable, BlendFactor src_rgb = BlendFactor::eOne, BlendFactor dst_rgb = BlendFactor::eZero, BlendFactor src_a = BlendFactor::eOne, BlendFactor dst_a = BlendFactor::eZero);

	void begin_frame();
	void end_frame();

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
};

} // namespace engine