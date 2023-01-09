#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <span>

struct GLFWwindow;

namespace engine
{
struct viewport_t
{
	std::uint32_t x;
	std::uint32_t y;
	std::uint32_t width;
	std::uint32_t height;
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
	void set_uniform_mat4f(std::string_view name, std::span<const float> host_data);

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
	Texture2D(std::string_view texture_name, bool generate_mipmaps);

	Texture2D(const Texture2D& rhs) = delete;
	Texture2D(Texture2D&& rhs) noexcept = default;
	Texture2D& operator=(const Texture2D& rhs) = delete;
	Texture2D& operator=(Texture2D&& rhs)  noexcept = default;

	~Texture2D();

	void bind(std::uint32_t slot);

private:
	std::uint32_t texture_;
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
	Geometry(std::span<vertex_attribute_t> vertex_layout, std::span<float> vertex_data, std::span<std::uint32_t> index_data = {});
	Geometry(const Geometry& rhs) = delete;
	Geometry(Geometry&& rhs) noexcept = default;
	Geometry& operator=(const Geometry& rhs) = delete;
	Geometry& operator=(Geometry&& rhs)  noexcept = default;

	~Geometry();

	void bind() const;
	void draw(Mode mode) const;

private:
	std::uint32_t vbo_; // vertex buffer
	std::uint32_t ibo_; // index buffer
	std::uint32_t vao_;
	std::uint32_t vertex_count_;
	std::uint32_t index_count_;
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
public:
	RenderContext(std::string_view window_name, viewport_t init_size);
	
	RenderContext(const RenderContext&) = delete;
	RenderContext(RenderContext&& rhs) noexcept;
	RenderContext& operator=(const RenderContext&) = delete;
	RenderContext& operator=(RenderContext&& rhs) noexcept;
	~RenderContext();

	GLFWwindow* get_glfw_window();

	void set_viewport(const viewport_t& viewport);
	void set_clear_color(float r, float g, float b, float a);
	void set_polygon_mode(PolygonFaceType face, PolygonMode mode);
	void begin_frame();
	void end_frame();

private:
	GLFWwindow* window_ = nullptr;
};

} // namespace engine