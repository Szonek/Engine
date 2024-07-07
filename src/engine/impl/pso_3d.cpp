#include "pso_3d.h"

engine::Pso3D::Pso3D(std::vector<std::string_view> vertex_shader_name, std::vector<std::string_view> fragment_shader_name)
    : shader_(vertex_shader_name, fragment_shader_name)
{
}

void engine::Pso3D::bind()
{
    shader_.bind();

    for (const auto& ub : uniform_buffers_)
    {
        shader_.set_uniform_block(ub.name, ub.binding, ub.binding_point);
    }

    for (const auto& tex : textures_)
    {
        shader_.set_texture(tex.name, tex.binding);
    }
}

void engine::Pso3D::set_uniform_buffer(std::string_view name, UniformBuffer& ubo, uint32_t binding_point)
{
    uniform_buffers_.push_back({ &ubo, std::string(name), binding_point });
}

void engine::Pso3D::set_texture(std::string_view name, Texture2D& texture)
{
    textures_.push_back({ &texture, std::string(name) });
}
