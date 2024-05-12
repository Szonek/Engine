#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <span>
#include <vector>


#include <RmlUi/Core.h>
#include "RmlUI_backend/RmlUi_Platform_SDL.h"
#include "RmlUI_backend/RmlUi_Renderer_GL3.h"

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

    // depth and stencil formats
    eDEPTH24_STENCIL8_U32,
    eCount
};

enum class TextureAddressClampMode
{
    eClampToEdge = 0,
    eClampToBorder = 1,
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
	Shader(std::vector<std::string_view> vertex_shader_name, std::vector<std::string_view> fragment_shader_name);
	Shader(const Shader& rhs) = delete;
	Shader(Shader&& rhs) noexcept = default;
	Shader& operator=(const Shader& rhs) = delete;
	Shader& operator=(Shader&& rhs)  noexcept = default;

	~Shader();

	void bind() const;
	void set_uniform_f4(std::string_view name, std::span<const float> host_data);
	void set_uniform_f2(std::string_view name, std::span<const float> host_data);
	void set_uniform_f1(std::string_view name, const float host_data);
	void set_uniform_ui2(std::string_view name, std::span<const std::uint32_t> host_data);
	void set_uniform_mat_f4(std::string_view name, std::span<const float> host_data);

    void set_texture(std::string_view name, const class Texture2D* textur);
    void set_ssbo(std::string_view name, const class ShaderStorageBuffer* buffer);

private:
    std::int32_t get_resource_location(std::string_view name, std::int32_t resource_interface);
    std::int32_t get_uniform_location(std::string_view name);
	void compile_and_attach_to_program(std::uint32_t shader, std::span<const std::string> sources);

private:
	std::uint32_t vertex_shader_;
	std::uint32_t fragment_shader_;
	std::uint32_t program_;
};



class Texture2D
{
    friend class Framebuffer;
public:
	Texture2D() = default;
	Texture2D(std::uint32_t width, std::uint32_t height, bool generate_mipmaps, const void* data, DataLayout layout, TextureAddressClampMode clamp_mode);
	Texture2D(std::string_view texture_name, bool generate_mipmaps);  
    static Texture2D create_and_attach_to_frame_buffer(std::uint32_t width, std::uint32_t height, DataLayout layout, std::size_t idx);

	Texture2D(const Texture2D& rhs) = delete;
	Texture2D(Texture2D&& rhs) noexcept;
	Texture2D& operator=(const Texture2D& rhs) = delete;
	Texture2D& operator=(Texture2D&& rhs)  noexcept;

	~Texture2D();

    bool upload_region(std::uint32_t x_pos, std::uint32_t y_pos, std::uint32_t width, std::uint32_t height, const void* data, DataLayout layout);
    bool is_valid() const;
	void bind(std::uint32_t slot) const;

private:
	std::uint32_t texture_ = 0;
};

class Framebuffer
{
public:
    Framebuffer(std::uint32_t width, std::uint32_t height, std::uint32_t color_attachment_count, bool has_depth_attachment);
    Framebuffer(const Framebuffer& rhs) = delete;
    Framebuffer(Framebuffer&& rhs) noexcept;
    Framebuffer& operator=(const Framebuffer& rhs) = delete;
    Framebuffer& operator=(Framebuffer&& rhs)  noexcept;
    ~Framebuffer();

    void bind();
    void unbind();
    void resize(std::uint32_t width, std::uint32_t height);
    void clear();

    Texture2D* get_color_attachment(std::size_t idx);
    Texture2D* get_depth_attachment();

    std::pair<std::uint32_t, std::uint32_t> get_size() const;

private:
    std::uint32_t fbo_{0};
    std::vector<Texture2D> color_attachments_;
    Texture2D depth_attachment_;

    std::uint32_t width_;
    std::uint32_t height_;
};

class Geometry
{
public:
	enum Mode
	{
		eTriangles = 0,
        eLines,
		eCount
	};

	struct vertex_attribute_t
	{
		enum class Type
		{
			eFloat32 = 0,

			eUint32,
			eInt32,

			eUint16,
			eInt16,

            eUint8,
            eInt8,

			eCount
		};
		std::uint32_t index = 0;
		std::uint32_t size = 0;
		std::uint32_t stride = 0;
		std::size_t offset = 0;
		Type type;

        std::vector<float> range_max;
        std::vector<float> range_min;
	};

public:
	Geometry() = default;
	Geometry(std::span<const vertex_attribute_t> vertex_layout, std::span<const std::byte> vertex_data, std::int32_t vertex_count, std::span<const std::uint32_t> index_data = {});
    Geometry(std::uint32_t vertex_count); // empty geometry, no data (used for full screen quad rendering when vertex data is already present in the shader)
	Geometry(const Geometry& rhs) = delete;
	Geometry(Geometry&& rhs) noexcept;
	Geometry& operator=(const Geometry& rhs) = delete;
	Geometry& operator=(Geometry&& rhs) noexcept;

	~Geometry();

	void bind() const;
	void draw(Mode mode) const;
    void draw_instances(Mode mode, std::uint32_t instance_count) const;

    vertex_attribute_t get_vertex_attribute(std::size_t idx) const;

private:
    std::vector<vertex_attribute_t> attribs_{};
	std::uint32_t vbo_{0}; // vertex buffer
	std::uint32_t ibo_{0}; // index buffer
	std::uint32_t vao_{0};
	std::uint32_t vertex_count_{0};
	std::uint32_t index_count_{0};
};

class UniformBuffer
{
public:
    UniformBuffer() = default;
    UniformBuffer(std::size_t size);
    UniformBuffer(const UniformBuffer& rhs) = delete;
    UniformBuffer(UniformBuffer&& rhs) noexcept;
    UniformBuffer& operator=(const UniformBuffer& rhs) = delete;
    UniformBuffer& operator=(UniformBuffer&& rhs) noexcept;
    ~UniformBuffer();

    inline bool is_valid() const { return ubo_ != 0; }
    inline std::size_t get_size() const { return size_; }

    void bind(std::uint32_t slot) const;

    void* map(bool read, bool write);
    void unmap();

private:
    void bind() const;
    void unbind() const;

private:
    std::size_t size_{ 0 };
    std::uint32_t ubo_{ 0 };
};

class ShaderStorageBuffer
{
public:
    ShaderStorageBuffer() = default;
    ShaderStorageBuffer(std::size_t size);
    ShaderStorageBuffer(const ShaderStorageBuffer& rhs) = delete;
    ShaderStorageBuffer(ShaderStorageBuffer&& rhs) noexcept;
    ShaderStorageBuffer& operator=(const ShaderStorageBuffer& rhs) = delete;
    ShaderStorageBuffer& operator=(ShaderStorageBuffer&& rhs) noexcept;
    ~ShaderStorageBuffer();

    inline bool is_valid() const { return ssbo_ != 0; }
    inline std::size_t get_size() const { return size_; }

    void bind(std::uint32_t slot) const;

    void* map(bool read, bool write);
    void unmap();

private:
    void bind() const;
    void unbind() const;

private:
    std::size_t size_{ 0 };
    std::uint32_t ssbo_{ 0 };
};


template<typename DataT, typename BufferT>
struct BufferMapContext
{
private:
    BufferT& buffer_;
public:
    DataT* data = nullptr;

    BufferMapContext(BufferT& buffer, bool read, bool write)
        : buffer_(buffer)
        , data(reinterpret_cast<DataT*>(buffer_.map(read, write)))
    {
    }
    // delete copy constructor
    BufferMapContext(const BufferMapContext& rhs) = delete;
    // delete copy assignment
    BufferMapContext& operator=(const BufferMapContext& rhs) = delete;
    // default move constructor
    BufferMapContext(BufferMapContext&& rhs) noexcept = default;
    // default move assignment
    BufferMapContext& operator=(BufferMapContext&& rhs) noexcept = default;
    ~BufferMapContext()
    {
        if (data)
        {
            unmap();
        }
    }

    void unmap()
    {
        buffer_.unmap();
        data = nullptr;
    }

};

class RenderContext
{
public:
	enum class PolygonFaceType
	{
		eFrontAndBack = 0,
        eFront,
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

    struct limits_t
    {
        std::int32_t vertex_attributes_limit{ 0 };
        std::int32_t ubo_max_size{ 0 };
        std::int32_t ssbo_max_size{ 0 };
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

    void begin_frame_ui_rendering();
    void end_frame_ui_rendering();

    SDL_Window* get_sdl_window() { return window_; }
    SDL_GLContext get_sdl_gl_context() { return context_; }

    const limits_t& get_limits() const { return limits_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;

    // this 2 are used for UI render
    SystemInterface_SDL* ui_rml_sdl_interface_ = nullptr;
    RenderInterface_GL3* ui_rml_gl3_renderer_ = nullptr;

    limits_t limits_;
};

} // namespace engine