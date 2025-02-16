#include "engine.h"
#include "graphics.h"
#include "asset_store.h"
#include "logger.h"

#if __ANDROID__
#define GLAD_GLES2_IMPLEMENTATION
#include <glad/gles2.h>
#else
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#endif
#include "profiler.h"

#include "tracy/TracyOpenGL.hpp"
#define ENGINE_PROFILER_GPU_CONTEXT TracyGpuContext
#define ENGINE_PROFILER_GPU_SWAP_WINDOW TracyGpuCollect
#define ENGINE_PROFILER_GPU_SECTION(x) TracyGpuZone(x)

#include <SDL3/SDL.h>

#include <fmt/format.h>

#include <cassert>
#include <array>
#include <iostream>

class SystemInterface_SDL;
class RenderInterface_GL3;

namespace
{
inline std::uint32_t to_ogl_datatype(engine::DataLayout layout)
{
    switch (layout)
    {
    case engine::DataLayout::eRGBA_U8:
    case engine::DataLayout::eRGB_U8:
    case engine::DataLayout::eR_U8: 
        return GL_UNSIGNED_BYTE;
    case engine::DataLayout::eRGBA_FP32:
    case engine::DataLayout::eR_FP32:
        return GL_FLOAT;
    case engine::DataLayout::eDEPTH24_STENCIL8_U32:
        return GL_UNSIGNED_INT_24_8;
    default:
        assert(false && "Unknown texture data type.");
        break;
    }
    return GL_FALSE;
}

inline std::uint32_t to_ogl_format(engine::DataLayout layout)
{
    switch (layout)
    {
    case engine::DataLayout::eRGBA_U8:
    case engine::DataLayout::eRGBA_FP32:
        return GL_RGBA;
    case engine::DataLayout::eRGB_U8:
        return GL_RGB;
    case engine::DataLayout::eR_U8:
    case engine::DataLayout::eR_FP32:
        return GL_RED;
    case engine::DataLayout::eDEPTH24_STENCIL8_U32:
        return GL_DEPTH24_STENCIL8;
    default:
        assert(false && "Unknown OGL data type!");
        break;
    }
    return GL_FALSE;
}

inline std::uint32_t to_ogl_host_format(engine::DataLayout layout)
{
    if (layout == engine::DataLayout::eDEPTH24_STENCIL8_U32)
    {
        return GL_DEPTH_STENCIL;
    }
    else
    {
        return to_ogl_format(layout);
    }
}
inline std::uint8_t data_layout_bytes_width(engine::DataLayout layout)
{
    switch (layout)
    {
    case engine::DataLayout::eRGBA_FP32: return 4 * sizeof(float);
    case engine::DataLayout::eR_FP32: return 1 * sizeof(float);

    case engine::DataLayout::eRGBA_U8: return 4 * sizeof(unsigned char);
    case engine::DataLayout::eRGB_U8: return 3 * sizeof(unsigned char);
    case engine::DataLayout::eR_U8: return 1 * sizeof(unsigned char);
    case engine::DataLayout::eDEPTH24_STENCIL8_U32: return sizeof(std::uint32_t);
    default:
        assert(false && "Unknown texture data type.");
        break;
    }
    return GL_FALSE;
}

inline std::uint32_t to_ogl_texture_border_clamp_mode(engine::TextureAddressClampMode mode)
{
    switch (mode)
    {
#if defined(GL_CLAMP_TO_BORDER)
    case engine::TextureAddressClampMode::eClampToBorder: return GL_CLAMP_TO_BORDER;
#endif
    case engine::TextureAddressClampMode::eClampToEdge: return GL_CLAMP_TO_EDGE;
    default:
        assert(false && "Unknown TextureAddressClampMode!");
    }
    return GL_FALSE;
}

}


engine::Shader::Shader(const std::vector<std::string>& vertex_shader_name, const std::vector<std::string>& fragment_shader_name)
: vertex_shader_(0)
, fragment_shader_(0)
, program_(glCreateProgram())
{
    log::log(log::LogLevel::eTrace, fmt::format("[Trace][Program] Creating shaders: \t\n"));
	// compile shaders and link to program
	{
        for (const auto& s : vertex_shader_name)
        {
            log::log(log::LogLevel::eTrace, fmt::format("\t[Trace][Program] Vertex shader: {}\n", s));
        }
        std::vector<std::string> sources;
        sources.reserve(vertex_shader_name.size());
        std::for_each(vertex_shader_name.begin(), vertex_shader_name.end(), [&sources](const auto& s) { sources.push_back(AssetStore::get_instance().get_shader_source(s)); });
		vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
        compile_and_attach_to_program(vertex_shader_, sources);
	}
	{
        for (const auto& s : fragment_shader_name)
        {
            log::log(log::LogLevel::eTrace, fmt::format("\t[Trace][Program] Fragment shader: {}\n", s));
        }
        std::vector<std::string> sources;
        sources.reserve(fragment_shader_name.size());
        std::for_each(fragment_shader_name.begin(), fragment_shader_name.end(), [&sources](const auto& s) { sources.push_back(AssetStore::get_instance().get_shader_source(s)); });
		fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
        compile_and_attach_to_program(fragment_shader_, sources);
	}
	// link attached shaders
	glLinkProgram(program_);
	int32_t success = 0;
	glGetProgramiv(program_, GL_LINK_STATUS, &success);
	if (!success)
	{
		std::array<char, 512> info_log;
		glGetProgramInfoLog(program_, 512, nullptr, info_log.data());
		log::log(log::LogLevel::eCritical, fmt::format("[Error][Program] Failed program linking: \n\t {}", info_log.data()));
        assert(false && "Failed shader compilation!");
	}
}

engine::Shader::Shader(Shader&& rhs) noexcept
{
    std::swap(vertex_shader_, rhs.vertex_shader_);
    std::swap(fragment_shader_, rhs.fragment_shader_);
    std::swap(program_, rhs.program_);
}

engine::Shader& engine::Shader::operator=(Shader&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::swap(vertex_shader_, rhs.vertex_shader_);
        std::swap(fragment_shader_, rhs.fragment_shader_);
        std::swap(program_, rhs.program_);
    }
    return *this;
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

bool engine::Shader::is_valid() const
{
    return program_ != 0;
}

void engine::Shader::bind() const
{
    assert(is_valid() && "[ERROR] Invalid shader program.");
	glUseProgram(program_);
}

void engine::Shader::set_uniform_f4(std::string_view name, std::span<const float> host_data)
{
	assert(host_data.size() == 4 && "[ERROR] Wrong size of data");
    const auto loc = get_uniform_location(name);
	glUniform4f(loc, host_data[0], host_data[1], host_data[2], host_data[3]);
}

void engine::Shader::set_uniform_f3(std::string_view name, std::span<const float> host_data)
{
    assert(host_data.size() == 3 && "[ERROR] Wrong size of data");
    const auto loc = get_uniform_location(name);
    glUniform3f(loc, host_data[0], host_data[1], host_data[2]);
}


void engine::Shader::set_uniform_f2(std::string_view name, std::span<const float> host_data)
{
    assert(host_data.size() == 2 && "[ERROR] Wrong size of data.");
    const auto loc = get_uniform_location(name);
    glUniform2f(loc, host_data[0], host_data[1]);
}

void engine::Shader::set_uniform_f1(std::string_view name, const float host_data)
{
    const auto loc = get_uniform_location(name);
    glUniform1f(loc, host_data);
}

void engine::Shader::set_uniform_ui2(std::string_view name, std::span<const std::uint32_t> host_data)
{
    assert(host_data.size() == 2 && "[ERROR] Wrong size of data.");
    const auto loc = get_uniform_location(name);
    glUniform2ui(loc, host_data[0], host_data[1]);
}

void engine::Shader::set_uniform_block(std::string_view name, const UniformBuffer* buffer, std::uint32_t bind_index)
{
    const auto block_index = glGetUniformBlockIndex(program_, name.data());
    if (block_index != -1)
    {
        //assert(block_index != -1 && "[ERROR] Cant find uniform block index in the shader.");
        glUniformBlockBinding(program_, block_index, bind_index);
        buffer->bind(bind_index);
    }
}

void engine::Shader::set_uniform_mat_f4(std::string_view name, std::span<const float> host_data)
{
	assert(host_data.size() == 16 && "[ERROR] Wrong size of data");
    const auto loc = get_uniform_location(name);
	glUniformMatrix4fv(loc, 1, GL_FALSE, host_data.data());
}

void engine::Shader::set_texture(std::string_view name, const Texture2D* texture)
{
    assert(texture &&  "[ERROR] Nullptr texture ptr");
    const auto loc = get_uniform_location(name);
    std::int32_t bind_slot = 0;
    glGetUniformiv(program_, loc, &bind_slot);;
    texture->bind(static_cast<std::uint32_t>(bind_slot));
}


std::int32_t engine::Shader::get_resource_location(std::string_view name, std::int32_t resource_interface)
{
    // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGetProgramResourceIndex.xhtml
    const auto location = glGetProgramResourceIndex(program_, resource_interface, name.data());
	assert(location != -1 && "[ERROR] Cant find uniform location in the shader.");
	return location;
}

std::int32_t engine::Shader::get_uniform_location(std::string_view name)
{
    const auto location = glGetUniformLocation(program_, name.data());
    assert(location != -1 && "[ERROR] Cant find uniform location in the shader.");
    return location;
}

void engine::Shader::compile_and_attach_to_program(std::uint32_t shader, std::span<const std::string> sources)
{
	// set source
    std::vector<const char*> sources_ptrs;
    sources_ptrs.reserve(sources.size());
    std::for_each(sources.begin(), sources.end(), [&sources_ptrs](const auto& s) {sources_ptrs.push_back(s.data()); });

	glShaderSource(shader, static_cast<std::int32_t>(sources.size()), sources_ptrs.data(), nullptr);

	// compile
	glCompileShader(shader);

	// validate compialtion
	int32_t success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		std::array<char, 512> info_log;
		glGetShaderInfoLog(shader, static_cast<std::int32_t>(info_log.size()), nullptr, info_log.data());
        log::log(log::LogLevel::eError, fmt::format("[Error][Shader] Failed compilation: \n\t {}", info_log.data()));
	}
	// attach to program
	glAttachShader(program_, shader);
}


inline auto generate_opengl_texture(std::uint32_t width, std::uint32_t height, engine::DataLayout layout, bool generate_mipmaps, const void* data, engine::TextureAddressClampMode clamp_mode)
{
	assert(width != 0);
	assert(height != 0);

    if (data)
    {
        const auto rows_width = width * data_layout_bytes_width(layout);
        const std::array<std::uint32_t, 4> rows_alignemnts = { 8, 4, 2, 1 };
        for (const auto& ra : rows_alignemnts)
        {
            if (rows_width % ra == 0)
            {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            }
        }
    }

    const auto gl_internal_format = to_ogl_format(layout);
	const auto gl_host_format = to_ogl_host_format(layout);

	std::uint32_t tex_id{ 0 };
	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_2D, tex_id);

	glTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, width, height,
		0, gl_host_format, to_ogl_datatype(layout), data);

	if (generate_mipmaps)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, to_ogl_texture_border_clamp_mode(clamp_mode));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, to_ogl_texture_border_clamp_mode(clamp_mode));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
	return tex_id;
}


engine::Texture2D::Texture2D(std::uint32_t width, std::uint32_t height, bool generate_mipmaps, const void* data, DataLayout layout, TextureAddressClampMode clamp_mode)
	: texture_(generate_opengl_texture(width, height, layout, generate_mipmaps, data, clamp_mode))
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

    DataLayout dt = DataLayout::eCount;
    if (texture_data.get_type() == TextureAssetContext::TextureAssetDataType::eUchar8)
    {
        switch (texture_data.get_channels())
        {
        case 4:
            dt = DataLayout::eRGBA_U8;
            break;
        case 3:
            dt = DataLayout::eRGB_U8;
            break;
        case 1 :
            dt = DataLayout::eR_U8;
             break;
        default:
            assert("Unsupported number of channels for U8 texture!");
        }
    }
    else
    {
        assert("Unsupported texture data type!");
    }

	texture_ = generate_opengl_texture(texture_data.get_width(), texture_data.get_height(), dt, generate_mipmaps, texture_data.get_data_ptr(), TextureAddressClampMode::eClampToEdge);
}

engine::Texture2D engine::Texture2D::create_and_attach_to_frame_buffer(std::uint32_t width, std::uint32_t height, DataLayout layout, std::size_t idx)
{
    Texture2D ret(width, height, false, nullptr, layout, engine::TextureAddressClampMode::eClampToEdge);
    const auto attachment_idx = layout == DataLayout::eDEPTH24_STENCIL8_U32 ? GL_DEPTH_STENCIL_ATTACHMENT : GL_COLOR_ATTACHMENT0 + idx;
    glBindTexture(GL_TEXTURE_2D, ret.texture_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_idx, GL_TEXTURE_2D, ret.texture_, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return std::move(ret);
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

bool engine::Texture2D::upload_region(std::uint32_t x_pos, std::uint32_t y_pos, std::uint32_t width, std::uint32_t height, const void* data, DataLayout layout)
{
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x_pos, y_pos, width, height, to_ogl_format(layout), to_ogl_datatype(layout), data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return false;
}

bool engine::Texture2D::is_valid() const
{
    return texture_ != 0;
}

void engine::Texture2D::bind(std::uint32_t slot) const
{
	assert(texture_ != 0);
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, texture_);
}

engine::Geometry::Geometry(std::span<const vertex_attribute_t> vertex_layout, std::span<const std::byte> vertex_data, std::int32_t vertex_count, std::span<const std::uint32_t> index_data)
	: vbo_(0)
	, vao_(0)
	, ibo_(0)
	, vertex_count_(vertex_count)
	, index_count_(0)
{
    if (vertex_layout.empty())
    {
        log::log(log::LogLevel::eCritical, "Empty vertex layout!");
        return;
    }
    if(vertex_data.empty())
    {
        log::log(log::LogLevel::eCritical, "Empty vertex data!");
        return;
    }
	// vertex array object (buffer)
	glGenVertexArrays(1, &vao_);
	glBindVertexArray(vao_);

	// vertex buffer
    glGenBuffers(1, &vbo_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, vertex_data.size_bytes(), vertex_data.data(), GL_STATIC_DRAW);

	// vertex layout 
    std::for_each(vertex_layout.begin(), vertex_layout.end(), [this](const vertex_attribute_t& vl)
        {
            bool use_float_attrib = false;  // we dont want to cast ints to floats with glVertexAttribPointer
			std::uint32_t gl_type = 0;
			switch (vl.type)
			{
			case vertex_attribute_t::Type::eFloat32:
				gl_type = GL_FLOAT;
                use_float_attrib = true;
				break;
            case vertex_attribute_t::Type::eUint32:
                gl_type = GL_UNSIGNED_INT;
                break;
            case vertex_attribute_t::Type::eInt32:
                gl_type = GL_INT;
                break;
            case vertex_attribute_t::Type::eUint16:
                gl_type = GL_UNSIGNED_SHORT;
                break;
            case vertex_attribute_t::Type::eInt16:
                gl_type = GL_SHORT;
                break;
            case vertex_attribute_t::Type::eUint8:
                gl_type = GL_UNSIGNED_BYTE;
                break;
            case vertex_attribute_t::Type::eInt8:
                gl_type = GL_BYTE;
                break;
			default:
				assert("Unknown vertex attirubute tpye!");
			}

            if (use_float_attrib)
            {
                // if intiget is used with this function than it will be implictly casted to float
                glVertexAttribPointer(vl.index, vl.size, gl_type, GL_FALSE, vl.stride, (void*)vl.offset);
            }
            else
            {
                glVertexAttribIPointer(vl.index, vl.size, gl_type, vl.stride, (void*)vl.offset);
            }

			glEnableVertexAttribArray(vl.index);
            attribs_.push_back(vl);
		}
	);

	// index buffer
	if (!index_data.empty())
	{
		glGenBuffers(1, &ibo_);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_data.size_bytes(), index_data.data(), GL_STATIC_DRAW);
		index_count_ = static_cast<std::uint32_t>(index_data.size());
	}

	// unbind at the end so both vbo_ and ibo_ are part of VAO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

engine::Geometry::Geometry(std::uint32_t vertex_count)
    : vertex_count_(vertex_count)
{
    glGenVertexArrays(1, &vao_);
}

engine::Geometry::Geometry(Geometry&& rhs) noexcept
{
	std::swap(vbo_, rhs.vbo_);
	std::swap(ibo_, rhs.ibo_);
	std::swap(vao_, rhs.vao_);
	std::swap(vertex_count_, rhs.vertex_count_);
	std::swap(index_count_, rhs.index_count_);
	std::swap(attribs_, rhs.attribs_);
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
		std::swap(attribs_, rhs.attribs_);
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
    ENGINE_PROFILER_GPU_SECTION("Draw geometry");
	std::uint32_t gl_mode = 0;
	switch (mode)
	{
	case Geometry::Mode::eTriangles:
		gl_mode = GL_TRIANGLES;
		break;
    case Geometry::Mode::eLines:
        gl_mode = GL_LINES;
        break;
	default:
		assert(!"[OpenGl Render Context] Unknown draw mode.");
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

void engine::Geometry::draw_instances(Mode mode, std::uint32_t instance_count) const
{
    ENGINE_PROFILER_GPU_SECTION("Draw geometry instances");
    std::uint32_t gl_mode = 0;
    switch (mode)
    {
    case Geometry::Mode::eTriangles:
        gl_mode = GL_TRIANGLES;
        break;
    case Geometry::Mode::eLines:
        gl_mode = GL_LINES;
        break;
    default:
        assert(!"[OpenGl Render Context] Unknown draw mode.");
    }

    if (ibo_)
    {
        glDrawElementsInstanced(gl_mode, index_count_, GL_UNSIGNED_INT, nullptr, instance_count);
    }
    else
    {
        glDrawArraysInstanced(gl_mode, 0, vertex_count_, instance_count);
    }
}

engine::Geometry::vertex_attribute_t engine::Geometry::get_vertex_attribute(std::size_t idx) const
{
    return attribs_.at(idx);
}
#if defined(GLAD_GL_IMPLEMENTATION)
inline void GLAPIENTRY
message_callback(GLenum /*source*/,
	GLenum type,
	GLuint /*id*/,
	GLenum severity,
	GLsizei /*length*/,
	const GLchar* message,
	const void* /*userParam*/)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        // filter out notifactions
        return;
    }
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
	engine::log::log(engine::log::LogLevel::eCritical, fmt::format("[OpenGL] Type: {}, severity: {}, message: {} \n",
		type_str, severity_str, message));
}
#endif

engine::RenderContext::RenderContext(std::string_view window_name, viewport_t init_size, bool init_fullscreen)
{
    std::int32_t result_code = 0;
    result_code = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    if(result_code < 0)
    {
        const auto err_msg = SDL_GetError();
        log::log(log::LogLevel::eCritical, fmt::format("Cant init sdl: %s\n", err_msg));
        return;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#if __ANDROID__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif 

    const auto displays = []()
    {
        std::int32_t displays_count = 0;
        const auto displays = SDL_GetDisplays(&displays_count);
        std::vector<SDL_DisplayID> ret(displays_count);
        for(std::int32_t i = 0; i < displays_count; i++)
        {
            ret[i] = displays[i];
        }
        SDL_free(displays);
        return ret;
    }();

    if(displays.empty())
    {
        const auto err_msg = SDL_GetError();
        log::log(log::LogLevel::eCritical, fmt::format("Cant create sdl window: %s\n", err_msg));
        return;
    }

    const auto display_mode = SDL_GetCurrentDisplayMode(displays.front());
    if(!display_mode)
    {
        const auto err_msg = SDL_GetError();
        log::log(log::LogLevel::eCritical, fmt::format("Cant create sdl window: %s\n", err_msg));
        return;
    }

    if(init_size.width == 0 || init_size.height == 0)
    {
        init_size.width = display_mode->w;
        init_size.height = display_mode->h;
    }

    auto window_init_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (init_fullscreen)
    {
        window_init_flags |= SDL_WINDOW_FULLSCREEN;
    }
    window_ = SDL_CreateWindow(window_name.data(),
        init_size.width, init_size.height, window_init_flags);
    if(!window_)
    {
        const auto err_msg = SDL_GetError();
        log::log(log::LogLevel::eCritical, fmt::format("Cant create sdl window: %s\n", err_msg));
        return;
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
#if _DEBUG
    result_code = SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    if (result_code < 0)
    {
        const auto err_msg = SDL_GetError();
        log::log(log::LogLevel::eCritical, fmt::format("Cant init sdl: %s\n", err_msg));
        return;
    }
#endif
    context_ = SDL_GL_CreateContext(window_);

    if (!context_)
    {
        log::log(log::LogLevel::eCritical, fmt::format("Failed to create OGL context: Error: {}\n", SDL_GetError()));
        return;
    }
    SDL_GL_MakeCurrent(window_, context_);
    const auto set_swap_result = SDL_GL_SetSwapInterval(1);
    if (!set_swap_result)
    {
        log::log(log::LogLevel::eCritical, "Failed to set swap interval\n");
        return;
    }

#if defined(GLAD_GLES2_IMPLEMENTATION)
    const auto gl_version = gladLoadGLES2(SDL_GL_GetProcAddress);
#elif defined(GLAD_GL_IMPLEMENTATION)
    const auto gl_version = gladLoadGL(SDL_GL_GetProcAddress);
#endif
	if (gl_version == 0)
	{
        log::log(log::LogLevel::eCritical, "Failed to initialize GLAD\n");
		return;
	}
	else
	{
        log::log(log::LogLevel::eTrace, fmt::format("Sucesfully loaded Opengl ver: {}, {}.\n",
            GLAD_VERSION_MAJOR(gl_version), GLAD_VERSION_MINOR(gl_version)));
	}

    std::int32_t gl_context_flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &gl_context_flags);
#if _DEBUG
    if (gl_context_flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        log::log(log::LogLevel::eCritical, "[INFO] Debug build. Building with debug context.\n");
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(message_callback, nullptr);
    }
#endif

    auto fetch_and_print_limit = [](std::uint32_t ogl_type, auto& limit, const auto& name)
    {
            glGetIntegerv(ogl_type, &limit);
            log::log(log::LogLevel::eTrace, fmt::format("Max {} size: {}\n", name, limit));
    };

    // initalize limits
    fetch_and_print_limit(GL_MAX_VERTEX_ATTRIBS, limits_.vertex_attributes_limit, "vertex attributes");
    fetch_and_print_limit(GL_MAX_UNIFORM_BLOCK_SIZE, limits_.ubo_max_size, "uniform block");
    fetch_and_print_limit(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, limits_.ssbo_max_size, "shader storage block");
    
    // UI stuff 
    ui_rml_sdl_interface_ = new SystemInterface_SDL;
    ui_rml_gl3_renderer_ = new RenderInterface_GL3;
    ui_rml_sdl_interface_->SetWindow(window_);
    const auto window_size = get_window_size_in_pixels();
    set_viewport(viewport_t{0, 0, (uint32_t)window_size.width, (uint32_t)window_size.height});

    Rml::SetSystemInterface(ui_rml_sdl_interface_);
    Rml::SetRenderInterface(ui_rml_gl3_renderer_);


    // enable depth test and face culling
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    //glEnable(GL_BLEND);


    // profiler init
    ENGINE_PROFILER_GPU_CONTEXT;
}

engine::RenderContext::RenderContext(RenderContext&& rhs) noexcept
{
	std::swap(window_, rhs.window_);
	std::swap(context_, rhs.context_);
	std::swap(ui_rml_sdl_interface_, rhs.ui_rml_sdl_interface_);
	std::swap(ui_rml_gl3_renderer_, rhs.ui_rml_gl3_renderer_);
}

engine::RenderContext& engine::RenderContext::operator=(RenderContext&& rhs) noexcept
{
	if (this != &rhs)
	{
		std::swap(window_, rhs.window_);
		std::swap(context_, rhs.context_);
        std::swap(ui_rml_sdl_interface_, rhs.ui_rml_sdl_interface_);
        std::swap(ui_rml_gl3_renderer_, rhs.ui_rml_gl3_renderer_);
	}
	return *this;
}

engine::RenderContext::~RenderContext()
{
    if (ui_rml_sdl_interface_)
    {
        delete ui_rml_sdl_interface_;
    }
    if (ui_rml_gl3_renderer_)
    {
        delete ui_rml_gl3_renderer_;
    }


    if (context_)
    {
        SDL_GL_DestroyContext(context_);
    }
    if (window_)
    {
        SDL_DestroyWindow(window_);
    }
}

engine::RenderContext::window_size_t engine::RenderContext::get_window_size_in_pixels() const
{
    window_size_t ret{};
    //SDL_GetWindowSizeInPixels(window_, &ret.width, &ret.height);

	int w = 0;
	int h = 0;
	SDL_GetWindowSize(window_, &w, &h);
	ret.width = w;
	ret.height = h;
    return ret;
}

void engine::RenderContext::set_viewport(const viewport_t& viewport)
{
	glViewport(0, 0, viewport.width, viewport.height);
    ui_rml_gl3_renderer_->SetViewport(viewport.width, viewport.height);
}

void engine::RenderContext::set_clear_color(float r, float g, float b, float a)
{
	glClearColor(r, g, b, a);
}

void engine::RenderContext::set_depth_test(bool flag)
{
    if (flag)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
}

void engine::RenderContext::set_polygon_mode(PolygonFaceType face, PolygonMode mode)
{
#if defined(GLAD_GL_IMPLEMENTATION)
	std::uint32_t gl_face = 0;
	switch (face)
	{
	case PolygonFaceType::eFrontAndBack:
		gl_face = GL_FRONT_AND_BACK;
		break;
    case PolygonFaceType::eFront:
        gl_face = GL_FRONT;
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
#endif
}

void engine::RenderContext::set_blend_mode(bool enable, BlendFactor src_rgb, BlendFactor dst_rgb, BlendFactor src_a, BlendFactor dst_a)
{
    if (!enable)
    {
        glDisable(GL_BLEND);
        return;
    }


    auto to_gl_blend = [](const BlendFactor& bf)
    {
        switch (bf)
        {
        case BlendFactor::eZero: return GL_ZERO;
        case BlendFactor::eOne: return GL_ONE;
        case BlendFactor::eSrcAlpha: return GL_SRC_ALPHA;
        case BlendFactor::eOneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        default:
            assert("Unknown blond factor!");
        }
        return GL_FALSE;
    };

    glEnable(GL_BLEND);
    glBlendFuncSeparate(to_gl_blend(src_rgb), to_gl_blend(dst_rgb), to_gl_blend(src_a), to_gl_blend(dst_a));
}

void engine::RenderContext::begin_frame()
{
    glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    //ui_rml_gl3_renderer_->Clear();
    //ui_rml_gl3_renderer_->BeginFrame();
}

void engine::RenderContext::end_frame()
{
    //ui_rml_gl3_renderer_->EndFrame();
    SDL_GL_SwapWindow(window_);
    ENGINE_PROFILER_GPU_SWAP_WINDOW;
	// process errors
#if _DEBUG
	//GLenum err;
	//while ((err = glGetError()) != GL_NO_ERROR)
	//{
	//	// Process/log the error.
	//}
#endif
}

void engine::RenderContext::begin_frame_ui_rendering()
{
    //glClearStencil(0);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    //ui_rml_gl3_renderer_->Clear();
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ui_rml_gl3_renderer_->BeginFrame();
}

void engine::RenderContext::end_frame_ui_rendering()
{
    ui_rml_gl3_renderer_->EndFrame();
}

engine::Framebuffer::Framebuffer(std::uint32_t width, std::uint32_t height, std::uint32_t color_attachment_count, bool has_depth_attachment)
    : fbo_(0)
    , depth_attachment_()
    , color_attachments_()
    , width_(width)
    , height_(height)
{
    glGenFramebuffers(1, &fbo_);
    bind();
    //ToDo: investigare framebuffers which may have better performance, (but cant read from them directly!!)
    for (std::uint32_t i = 0; i < color_attachment_count; i++)
    {
        color_attachments_.push_back(Texture2D::create_and_attach_to_frame_buffer(width, height, DataLayout::eRGBA_U8, i));
    }

    if (has_depth_attachment)
    {
        depth_attachment_ = Texture2D::create_and_attach_to_frame_buffer(width, height, DataLayout::eDEPTH24_STENCIL8_U32, 0ull);
    }
    const auto framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
    {
        log::log(log::LogLevel::eCritical, "ERROR::FRAMEBUFFER:: Framebuffer is not complete!");
    }
    unbind();
}

engine::Framebuffer::Framebuffer(Framebuffer&& rhs) noexcept
    : fbo_(rhs.fbo_)
    , depth_attachment_(std::move(rhs.depth_attachment_))
    , color_attachments_(std::move(rhs.color_attachments_))
    , height_(rhs.height_)
    , width_(rhs.width_)
{
    rhs.fbo_ = 0;
    rhs.width_ = 0;
    rhs.height_ = 0;
    rhs.depth_attachment_ = Texture2D();
    color_attachments_.clear();
}

engine::Framebuffer& engine::Framebuffer::operator=(Framebuffer&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::swap(fbo_, rhs.fbo_);
        std::swap(width_, rhs.width_);
        std::swap(height_, rhs.height_);

        std::swap(depth_attachment_, rhs.depth_attachment_);
        std::swap(color_attachments_, rhs.color_attachments_);
    }
    return *this;
}

engine::Framebuffer::~Framebuffer()
{
    if (fbo_)
    {
        glDeleteFramebuffers(1, &fbo_);
    }
}

void engine::Framebuffer::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void engine::Framebuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void engine::Framebuffer::resize(std::uint32_t width, std::uint32_t height)
{
    width_ = width;
    height_ = height;

    const auto color_attachments_count = color_attachments_.size();
    const auto has_depth_attachment = depth_attachment_.is_valid();
    color_attachments_.clear();
    depth_attachment_ = Texture2D();

    for (std::uint32_t i = 0; i < color_attachments_count; i++)
    {
        color_attachments_.push_back(Texture2D::create_and_attach_to_frame_buffer(width, height, DataLayout::eRGBA_U8, i));
    }

    if (has_depth_attachment)
    {
        depth_attachment_ = Texture2D::create_and_attach_to_frame_buffer(width, height, DataLayout::eDEPTH24_STENCIL8_U32, 0ull);
    }
}

void engine::Framebuffer::clear()
{
    GLbitfield mask = 0;
    if (!color_attachments_.empty())
    {
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (depth_attachment_.is_valid())
    {
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    glClear(mask);
}

engine::Texture2D* engine::Framebuffer::get_color_attachment(std::size_t idx)
{
    assert(idx < color_attachments_.size());
    Texture2D& tex = color_attachments_[idx];
    return tex.is_valid() ? &tex : nullptr;
}

engine::Texture2D* engine::Framebuffer::get_depth_attachment()
{
    return depth_attachment_.is_valid()? &depth_attachment_ : nullptr;
}

std::pair<std::uint32_t, std::uint32_t> engine::Framebuffer::get_size() const
{
    return { width_, height_ };
}

engine::UniformBuffer::UniformBuffer(std::size_t size)
    : size_(size)
{
    if (size == 0)
    {
        log::log(log::LogLevel::eCritical, "Uniform buffer size cant be 0!");
        return;
    }

    glGenBuffers(1, &ubo_);
    bind();
    // GL_STATIC_DRAW? 
    glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    unbind();
}

engine::UniformBuffer::UniformBuffer(UniformBuffer&& rhs) noexcept
{
    std::swap(ubo_, rhs.ubo_);
    std::swap(size_, rhs.size_);
}

engine::UniformBuffer& engine::UniformBuffer::operator=(UniformBuffer&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::swap(ubo_, rhs.ubo_);
        std::swap(size_, rhs.size_);
    }
    return *this;
}

engine::UniformBuffer::~UniformBuffer()
{
    if (ubo_)
    {
        glDeleteBuffers(1, &ubo_);
    }
}

void engine::UniformBuffer::bind(std::uint32_t slot) const
{
    assert(is_valid() && "Invalid uniform buffer object");
    glBindBufferBase(GL_UNIFORM_BUFFER, slot, ubo_);
}

void* engine::UniformBuffer::map(bool read, bool write)
{
    bind();
    std::uint32_t flags = 0;
    if (read && write)
    {
        flags = GL_READ_WRITE;
    }
    else if (read)
    {
        flags = GL_READ_ONLY;
    }
    else if (write)
    {
        flags = GL_WRITE_ONLY;
    }
    void* ret =  glMapBuffer(GL_UNIFORM_BUFFER, flags);
    //unbind();
    return ret;
}

void engine::UniformBuffer::unmap()
{
    //bind();
    glUnmapBuffer(GL_UNIFORM_BUFFER);
    unbind();
}

void engine::UniformBuffer::bind() const
{
    assert(is_valid() && "Invalid uniform buffer object");
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
}

void engine::UniformBuffer::unbind() const
{
    assert(is_valid() && "Invalid uniform buffer object");
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

engine::ShaderStorageBuffer::ShaderStorageBuffer(std::size_t size)
    : size_(size)
{
    if (size == 0)
    {
        log::log(log::LogLevel::eCritical, "Shader storage buffer size cant be 0!");
        return;
    }

    glGenBuffers(1, &ssbo_);
    bind();
    // GL_STATIC_DRAW? 
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    unbind();
}

engine::ShaderStorageBuffer::ShaderStorageBuffer(ShaderStorageBuffer&& rhs) noexcept
{
    std::swap(ssbo_, rhs.ssbo_);
    std::swap(size_, rhs.size_);
}

engine::ShaderStorageBuffer& engine::ShaderStorageBuffer::operator=(ShaderStorageBuffer&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::swap(ssbo_, rhs.ssbo_);
        std::swap(size_, rhs.size_);
    }
    return *this;
}

engine::ShaderStorageBuffer::~ShaderStorageBuffer()
{
    if (ssbo_)
    {
        glDeleteBuffers(1, &ssbo_);
    }
}

void engine::ShaderStorageBuffer::bind(std::uint32_t slot) const
{
    assert(is_valid() && "Invalid uniform buffer object");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, ssbo_);
}

void* engine::ShaderStorageBuffer::map(bool read, bool write)
{
    bind();
    std::uint32_t flags = 0;
    if (read && write)
    {
        flags = GL_READ_WRITE;
    }
    else if (read)
    {
        flags = GL_READ_ONLY;
    }
    else if (write)
    {
        flags = GL_WRITE_ONLY;
    }

    void* ret = glMapBuffer(GL_SHADER_STORAGE_BUFFER, flags);
    unbind();
    return ret;
}

void engine::ShaderStorageBuffer::unmap()
{
    bind();
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    unbind();
}

void engine::ShaderStorageBuffer::bind() const
{
    assert(is_valid() && "Invalid uniform buffer object");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
}

void engine::ShaderStorageBuffer::unbind() const
{
    assert(is_valid() && "Invalid uniform buffer object");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

