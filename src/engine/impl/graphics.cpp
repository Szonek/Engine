#include "graphics.h"
#include "asset_store.h"

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cassert>
#include <array>
#include <iostream>

engine::Shader::Shader(std::string_view vertex_shader_name, std::string fragment_shader_name)
: vertex_shader_(0)
, fragment_shader_(0)
, program_(glCreateProgram())
{
	// compile shaders and link to program
	{
		const auto vertex_shader_source = AssetStore::get_instance().get_shader_source(vertex_shader_name);
		vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
		compile_and_attach_to_program(vertex_shader_, vertex_shader_source);
	}
	{
		const auto fragment_shader_source = AssetStore::get_instance().get_shader_source(fragment_shader_name);
		fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
		compile_and_attach_to_program(fragment_shader_, fragment_shader_source);
	}
	// link attached shaders
	glLinkProgram(program_);
	int32_t success = 0;
	glGetProgramiv(program_, GL_LINK_STATUS, &success);
	if (!success)
	{
		std::array<char, 512> info_log;
		glGetProgramInfoLog(program_, 512, nullptr, info_log.data());
		std::cout << "[Error][Program] Failed program linking: \n\t" <<
			info_log.data();
	}
}

engine::Shader::~Shader()
{
	if (vertex_shader_)
	{
		glDeleteShader(vertex_shader_);
	}
	if (fragment_shader_)
	{
		glDeleteShader(fragment_shader_);
	}
	if (program_)
	{
		glDeleteProgram(program_);
	}
}

void engine::Shader::bind() const
{
	glUseProgram(program_);
}

void engine::Shader::set_uniform_f4(std::string_view name, std::span<const float> host_data)
{
	assert(host_data.size() == 4 && "[ERROR] Not enough data.");
	const auto loc = get_uniform_location(name);
	glUniform4f(loc, host_data[0], host_data[1], host_data[2], host_data[3]);
}

void engine::Shader::set_uniform_mat4f(std::string_view name, std::span<const float> host_data)
{
	assert(host_data.size() == 16 && "[ERROR] Not enough data.");
	const auto loc = get_uniform_location(name);
	glUniformMatrix4fv(loc, 1, GL_FALSE, host_data.data());
}

std::int32_t engine::Shader::get_uniform_location(std::string_view name)
{
	std::int32_t uniform_location = -1;
	if (auto find_itr = uniforms_locations_.find(name.data()); find_itr != uniforms_locations_.end())
	{
		uniform_location = find_itr->second;
	}
	else
	{
		uniform_location = glGetUniformLocation(program_, name.data());
		uniforms_locations_.insert({ name.data(), uniform_location });
	}
	assert(uniform_location != -1 && "[ERROR] Cant find uniform location in the shader.");
	return uniform_location;
}

void engine::Shader::compile_and_attach_to_program(std::uint32_t shader, std::string_view source)
{
	// set source
	const char* source_cstr = source.data();
	glShaderSource(shader, 1, &source_cstr, nullptr);

	// compile
	glCompileShader(shader);

	// validate compialtion
	int32_t success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		std::array<char, 512> info_log;
		glGetShaderInfoLog(shader, info_log.size(), nullptr, info_log.data());
		std::cout << "[Error][Shader] Failed compilation: \n\t" <<
			info_log.data();
	}
	// attach to program
	glAttachShader(program_, shader);
}


inline auto generate_opengl_texture(std::uint32_t width, std::uint32_t height, std::uint32_t channels, GLenum gl_data_type, bool generate_mipmaps, const void* data)
{
	assert(width != 0);
	assert(height != 0);
	assert(channels != 0);
	assert(data != nullptr);


	std::int32_t gl_internal_format = 0;
	std::int32_t gl_host_format = 0;
	if (channels == 4)
	{
		gl_host_format = GL_RGBA;
		gl_internal_format = GL_RGBA;
	}
	else if (channels == 3)
	{
		gl_host_format = GL_RGB;
		gl_internal_format = GL_RGB;
	}
	else
	{
		assert(false && "Not supported number of texture channels.");
	}

	std::uint32_t tex_id{ 0 };
	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_2D, tex_id);

	glTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, width, height,
		0, gl_host_format, gl_data_type, data);

	if (generate_mipmaps)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return tex_id;
}


engine::Texture2D::Texture2D(std::uint32_t width, std::uint32_t height, std::uint32_t channels, bool generate_mipmaps, const void* data)
	: texture_(generate_opengl_texture(width, height, channels, GL_UNSIGNED_BYTE, generate_mipmaps, data))
{
	
}

engine::Texture2D::Texture2D(std::string_view texture_name, bool generate_mipmaps)
	: texture_(0)
{
	const auto texture_data = AssetStore::get_instance().get_texture_data(texture_name);
	assert(texture_data.get_width() != 0);
	assert(texture_data.get_height() != 0);
	assert(texture_data.get_channels() != 0);
	assert(texture_data.get_data_ptr() != nullptr);

	std::int32_t gl_data_type = 0;
	switch (texture_data.get_type())
	{
	case TextureAssetContext::Type::eUchar8:
		gl_data_type = GL_UNSIGNED_BYTE;
		break;
	default:
		assert(false && "Unknown texture data type.");
		break;
	}
	texture_ = generate_opengl_texture(texture_data.get_width(), texture_data.get_height(), texture_data.get_channels(),
		gl_data_type, generate_mipmaps, texture_data.get_data_ptr());
}

engine::Texture2D::Texture2D(Texture2D&& rhs) noexcept
{
	std::swap(texture_, rhs.texture_);
}

engine::Texture2D& engine::Texture2D::operator=(Texture2D&& rhs) noexcept
{
	if (this != &rhs)
	{
		std::swap(texture_, rhs.texture_);
	}
	return *this;
}

engine::Texture2D::~Texture2D()
{
	if (texture_)
	{
		glDeleteTextures(1, &texture_);
	}
}

void engine::Texture2D::bind(std::uint32_t slot) const
{
	assert(texture_ != 0);
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, texture_);
}

engine::Geometry::Geometry(std::span<const vertex_attribute_t> vertex_layout, std::span<const std::byte> vertex_data, std::size_t vertex_count, std::span<const std::uint32_t> index_data)
	: vbo_(0)
	, vao_(0)
	, ibo_(0)
	, vertex_count_(vertex_count)
	, index_count_(0)
{
	// vertex array object (buffer)
	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);
	glBindVertexArray(vao_);

	// vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, vertex_data.size_bytes(), vertex_data.data(), GL_STATIC_DRAW);

	// vertex layout 
	std::ranges::for_each(vertex_layout, [](const vertex_attribute_t& vl)
		{
			std::uint32_t gl_type = 0;
			switch (vl.type)
			{
			case vertex_attribute_t::Type::eFloat:
				gl_type = GL_FLOAT;
				break;
			default:
				assert("Unknown vertex attirubute tpye!");
			}
			glVertexAttribPointer(vl.index, vl.size, gl_type, GL_FALSE, vl.stride, (void*)vl.offset);
			glEnableVertexAttribArray(vl.index);
		}
	);

	// index buffer
	if (!index_data.empty())
	{
		glGenBuffers(1, &ibo_);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_data.size_bytes(), index_data.data(), GL_STATIC_DRAW);
		index_count_ = index_data.size();
	}

	// unbind at the end so both vbo_ and ibo_ are part of VAO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

engine::Geometry::Geometry(Geometry&& rhs) noexcept
{
	std::swap(vbo_, rhs.vbo_);
	std::swap(ibo_, rhs.ibo_);
	std::swap(vao_, rhs.vao_);
	std::swap(vertex_count_, rhs.vertex_count_);
	std::swap(index_count_, rhs.index_count_);
}

engine::Geometry& engine::Geometry::operator=(Geometry&& rhs) noexcept
{
	if (this != &rhs)
	{
		std::swap(vbo_, rhs.vbo_);
		std::swap(ibo_, rhs.ibo_);
		std::swap(vao_, rhs.vao_);
		std::swap(vertex_count_, rhs.vertex_count_);
		std::swap(index_count_, rhs.index_count_);
	}
	return *this;
}

engine::Geometry::~Geometry()
{
	if (vbo_)
	{
		glDeleteVertexArrays(1, &vao_);
	}
	if (ibo_)
	{
		glDeleteBuffers(1, &ibo_);
	}
	if (vao_)
	{
		glDeleteBuffers(1, &vbo_);
	}
}

void engine::Geometry::bind() const
{
	glBindVertexArray(vao_);
}

void engine::Geometry::draw(Mode mode) const
{
	std::uint32_t gl_mode = 0;
	switch (mode)
	{
	case Geometry::Mode::eTriangles:
		gl_mode = GL_TRIANGLES;
		break;
	default:
		assert("[OpenGl Render Context] Unknown draw mode.");
	}

	if (ibo_)
	{
		glDrawElements(gl_mode, index_count_, GL_UNSIGNED_INT, nullptr);
	}
	else
	{
		glDrawArrays(gl_mode, 0, vertex_count_);
	}
}

inline void framebuffer_size_callback(struct GLFWwindow* window, std::int32_t width, std::int32_t height)
{
	glViewport(0, 0, width, height);
}

inline void GLAPIENTRY
message_callback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	std::string type_str = "";
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR: type_str = "GL_DEBUG_TYPE_ERROR";  break;
	case GL_DEBUG_TYPE_MARKER: type_str = "GL_DEBUG_TYPE_MARKER";  break;
	case GL_DEBUG_TYPE_OTHER: type_str = "GL_DEBUG_TYPE_OTHER";  break;
	case GL_DEBUG_TYPE_PERFORMANCE: type_str = "GL_DEBUG_TYPE_PERFORMANCE";  break;
	case GL_DEBUG_TYPE_POP_GROUP: type_str = "GL_DEBUG_TYPE_POP_GROUP";  break;
	case GL_DEBUG_TYPE_PORTABILITY: type_str = "GL_DEBUG_TYPE_PORTABILITY";  break;
	case GL_DEBUG_TYPE_PUSH_GROUP: type_str = "GL_DEBUG_TYPE_PUSH_GROUP";  break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: type_str = "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR";  break;
	default:
		type_str = "";
	}

	std::string severity_str = "";
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH: severity_str = "GL_DEBUG_SEVERITY_HIGH";  break;
	case GL_DEBUG_SEVERITY_LOW: severity_str = "GL_DEBUG_SEVERITY_LOW";  break;
	case GL_DEBUG_SEVERITY_MEDIUM: severity_str = "GL_DEBUG_SEVERITY_MEDIUM";  break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: severity_str = "GL_DEBUG_SEVERITY_NOTIFICATION";  break;
	default:
		severity_str = "";
	}
	std::cerr << std::format("[OpenGL] Type: {}, severity: {}, message: {} \n",
		type_str, severity_str, message);
}

engine::RenderContext::RenderContext(std::string_view window_name, viewport_t init_size)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if _DEBUG

	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
	window_ = glfwCreateWindow(init_size.width, init_size.height, window_name.data(), nullptr, nullptr);
	if (!window_)
	{
		std::cout << "Failed to create GLFW window\n";
		return;
	}

	glfwMakeContextCurrent(window_);
	glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);

	//vsync
	glfwSwapInterval(1);

	const auto gl_version = gladLoadGL(glfwGetProcAddress);
	if (gl_version == 0)
	{
		std::cout << "Failed to initialize GLAD\n";
		return;
	}
	else
	{
		std::cout << std::format("Sucesfully loaded Opengl ver: {0}, {1}.\n",
			GLAD_VERSION_MAJOR(gl_version), GLAD_VERSION_MINOR(gl_version));
	}
#if _DEBUG
	std::cout << "[INFO] Debug build. Building with debug context.\n";
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(message_callback, 0);
#endif

	int32_t vertex_attributes_limit = 0;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &vertex_attributes_limit);
	std::cout << "Maximum nr of vertex attributes supported: " << vertex_attributes_limit << std::endl;

	// enable depth test
	glEnable(GL_DEPTH_TEST);
}

engine::RenderContext::RenderContext(RenderContext&& rhs) noexcept
{
	std::swap(window_, rhs.window_);
}

engine::RenderContext& engine::RenderContext::operator=(RenderContext&& rhs) noexcept
{
	if (this != &rhs)
	{
		std::swap(window_, rhs.window_);
	}
	return *this;
}

engine::RenderContext::~RenderContext()
{
	if (window_)
	{
		glfwTerminate();
	}
}

struct GLFWwindow* engine::RenderContext::get_glfw_window()
{
	return window_;
}

void engine::RenderContext::set_viewport(const viewport_t& viewport)
{
	glViewport(0, 0, viewport.width, viewport.height);
}

void engine::RenderContext::set_clear_color(float r, float g, float b, float a)
{
	glClearColor(r, g, b, a);
}

void engine::RenderContext::set_polygon_mode(PolygonFaceType face, PolygonMode mode)
{
	std::uint32_t gl_face = 0;
	switch (face)
	{
	case PolygonFaceType::eFrontAndBack:
		gl_face = GL_FRONT_AND_BACK;
		break;
	default:
		assert("Unknown polygon face type");
	}

	std::uint32_t gl_mode = 0;
	switch (mode)
	{
	case PolygonMode::eLine:
		gl_mode = GL_LINE;
		break;
	case PolygonMode::eFill:
		gl_mode = GL_FILL;
		break;
	default:
		assert("Unknown polygon mode");
	}

	glPolygonMode(gl_face, gl_mode);
}

void engine::RenderContext::begin_frame()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void engine::RenderContext::end_frame()
{
	glfwSwapBuffers(window_);

	// process errors
#if _DEBUG
	//GLenum err;
	//while ((err = glGetError()) != GL_NO_ERROR)
	//{
	//	// Process/log the error.
	//}
#endif
}