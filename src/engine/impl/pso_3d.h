#pragma once

#include "graphics.h"

namespace engine
{

class Pso3D
{
public:
    Pso3D() = default;
    Pso3D(std::vector<std::string_view> vertex_shader_name, std::vector<std::string_view> fragment_shader_name);

    void bind();

    void set_uniform_buffer(std::string_view name, UniformBuffer& ubo, uint32_t binding_point);
    void set_texture(std::string_view name, Texture2D& texture);

private:
    Shader shader_;

    template<typename T>
    struct DefferedBinding
    {
        T* binding;
        std::string name;
    };
    using DefferedBindingTexture2D = DefferedBinding<Texture2D>;
    struct DefferedBidingUbo : DefferedBinding<UniformBuffer>
    {
        uint32_t binding_point;
    };

    std::vector<DefferedBidingUbo> uniform_buffers_;
    std::vector<DefferedBindingTexture2D> textures_;
};

}   // namespace engine