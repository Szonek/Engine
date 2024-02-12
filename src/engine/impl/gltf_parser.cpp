#include "gltf_parser.h"
#include "logger.h"
#include "logger.h"

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include <fmt/format.h>

namespace
{
inline engine::GeometryInfo parse_mesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_mesh_primitive_mode

    engine::GeometryInfo ret{};
    ret.material_index = mesh.primitives.front().material;
    //assert(mesh.primitives.size() == 1 && "Not enabled path for primitives count > 1");

    std::size_t total_verts_count = 0;
    std::size_t total_inds_count = 0;
    for (std::size_t i = 0; i < mesh.primitives.size(); i++)
    {
        const auto& primitive = mesh.primitives[i];
        auto verts_count = 0;
        for (const auto& attrib : primitive.attributes)
        {
            const auto& attrib_accesor = model.accessors[attrib.second];
            if (attrib.first.compare("POSITION") == 0)
            {
                total_verts_count += attrib_accesor.count;
            }
        }

        const auto& index_accessor = model.accessors[primitive.indices];
        total_inds_count +=  index_accessor.count;
    }

    ret.verticies.reserve(total_verts_count);
    ret.indicies.reserve(total_inds_count);

    for (std::size_t i = 0; i < mesh.primitives.size(); i++)
    {
        const auto& primitive = mesh.primitives[i];
        assert(ret.material_index  == primitive.material && "Currently not supporting multi-material meshes!");
        struct attrib_info
        {
            const float* data = nullptr;
            std::size_t count = 0;
            std::size_t num_components = 0;
        };
        attrib_info position_data;
        attrib_info uv_0_data;
        attrib_info normals_data;

        for (const auto& attrib : primitive.attributes)
        {
            const auto& attrib_accesor = model.accessors[attrib.second];
            const auto& buffer_view = model.bufferViews[attrib_accesor.bufferView];
            const auto& buffer = model.buffers[buffer_view.buffer];

            const auto attrib_num_components = tinygltf::GetNumComponentsInType(attrib_accesor.type);
            const auto attrib_stride = attrib_accesor.ByteStride(model.bufferViews[attrib_accesor.bufferView]);

            if (attrib.first.compare("POSITION") == 0)
            {
                position_data.data = reinterpret_cast<const float*>(buffer.data.data() + buffer_view.byteOffset);
                position_data.count = attrib_accesor.count;
                position_data.num_components = attrib_num_components;
                assert(attrib_num_components == 3); //xyz
                assert(attrib_stride == 12); // 3 floats
            }
            if (attrib.first.compare("NORMAL") == 0)
            {
                normals_data.data = reinterpret_cast<const float*>(buffer.data.data() + buffer_view.byteOffset);
                normals_data.count = attrib_accesor.count;
                normals_data.num_components = attrib_num_components;
            }
            if (attrib.first.compare("TEXCOORD_0") == 0)
            {
                uv_0_data.data = reinterpret_cast<const float*>(buffer.data.data() + buffer_view.byteOffset);
                uv_0_data.count = attrib_accesor.count;
                uv_0_data.num_components = attrib_num_components;
                assert(attrib_num_components == 2); //xy
                assert(attrib_stride == 8); // 2 floats
            }
        }

        if (position_data.count == 0)
        {
            engine::log::log(engine::log::LogLevel::eCritical, 
                fmt::format("Cant load mesh correctly. Count of positions: {}, count of uv: {} .\n",
                    position_data.count, uv_0_data.count));
        }

        // verts
        const auto verts_offset = ret.verticies.size();
        ret.verticies.resize(verts_offset + position_data.count);
        for (auto i = 0; i < position_data.count; i++)
        {
            ret.verticies[verts_offset + i].position[0] = position_data.data[0 + i * position_data.num_components];
            ret.verticies[verts_offset + i].position[1] = position_data.data[1 + i * position_data.num_components];
            ret.verticies[verts_offset + i].position[2] = position_data.data[2 + i * position_data.num_components];

            if (uv_0_data.count != 0)
            {
                ret.verticies[verts_offset + i].uv[0] = uv_0_data.data[0 + i * 2];
                ret.verticies[verts_offset + i].uv[1] = uv_0_data.data[1 + i * 2];
            }
        }

        // inds
        auto copy_inds_data = [](auto& out, const auto out_base_offset, const auto& typed_gltf_buffer)
        {
            for (auto i = 0; i < out.size() - out_base_offset; i++)
            {
                out[i + out_base_offset] = typed_gltf_buffer[i];
            }
        };
        const auto& index_accessor = model.accessors[primitive.indices];
        const auto indicies_offset = ret.indicies.size();
        ret.indicies.resize(indicies_offset + index_accessor.count);

        const auto& index_buffer_view = model.bufferViews[index_accessor.bufferView];
        const auto& index_buffer = model.buffers[index_buffer_view.buffer];
        const auto index_buffer_data = index_buffer.data.data() + index_buffer_view.byteOffset;
        if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        {
            copy_inds_data(ret.indicies, indicies_offset, reinterpret_cast<const std::uint16_t*>(index_buffer_data));
        }
        else if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        {
            copy_inds_data(ret.indicies, indicies_offset, reinterpret_cast<const std::uint32_t*>(index_buffer_data));
        }
        else
        {
            assert(false && "Unsupported indicies data type!");
        }
    }

    return ret;
}

inline void parse_model_nodes(engine_model_info_t& model_info, const tinygltf::Model& model, const tinygltf::Node& node)
{
    //if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
    //    parse_mesh(model_info, model, model.meshes[node.mesh]);
    //}

    //for (std::size_t i = 0; i < node.children.size(); i++) 
    //{
    //    assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
    //    parse_model_nodes(model_info, model, model.nodes[node.children[i]]);
    //}
}
}  // namespace anonymous

engine::ModelInfo engine::parse_gltf_data_from_memory(std::span<const std::uint8_t> data)
{
    assert(!data.empty());

    std::string load_warning_msg = "";
    std::string load_error_msg = "";
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    // check magic to detect binary vs ascii
    bool load_success = false;
    const auto is_binary = data.size() > 4 && data[0] == 'g' && data[1] == 'l' && data[2] == 'T' && data[3] == 'F';
    if(is_binary)
    {
        load_success = loader.LoadBinaryFromMemory(&model, &load_error_msg, &load_warning_msg,
            data.data(), data.size_bytes(), "");
    }
    else
    {
        load_success = loader.LoadASCIIFromString(&model, &load_error_msg, &load_warning_msg, reinterpret_cast<const char*>(data.data()), data.size(), {});
    }
    if (!load_warning_msg.empty())
    {
        log::log(log::LogLevel::eTrace, load_warning_msg);
    }

    if (!load_error_msg.empty())
    {
        log::log(log::LogLevel::eCritical, load_error_msg);
    }

    if (!load_success)
    {
        log::log(log::LogLevel::eCritical, fmt::format("Failed to parse glTF from memory! \n").c_str());
        return {};
    }

    if (model.scenes.size() > 1)
    {
        log::log(log::LogLevel::eCritical, fmt::format("ToDo: Add support for multi-scene gltf parsing! \n").c_str());
        return {};
    }

    engine::ModelInfo out{};
    out.geometries.resize(model.meshes.size());

    for (std::size_t idx = 0; const auto& mesh : model.meshes)
    {
        out.geometries[idx] = parse_mesh(mesh, model);
        idx++;
    }

    out.materials.resize(model.materials.size());
    for (std::size_t idx = 0; const auto& material : model.materials)
    {
        MaterialInfo new_material{};
        new_material.name = material.name;
        // copy diffuse color
        for (std::size_t c = 0; c < 4; c++)
        {
            new_material.diffuse_factor[c] = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[c]);
        }
        // copy diffuse texture
        auto diffuse_texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
        if (diffuse_texture_index >= 0)
        {
            const auto& tex = model.images[diffuse_texture_index];
            TextureInfo tex_info{};
            tex_info.name = tex.name;
            tex_info.width = tex.width;
            tex_info.height = tex.height;
            tex_info.layout = ENGINE_DATA_LAYOUT_COUNT;
            if (tex.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            {
                if (tex.component == 4)
                {
                    tex_info.layout = ENGINE_DATA_LAYOUT_RGBA_U8;
                }
                else if (tex.component == 3)
                {
                    tex_info.layout = ENGINE_DATA_LAYOUT_RGB_U8;
                }
            }
            assert(tex_info.layout != ENGINE_DATA_LAYOUT_COUNT);
            tex_info.data.resize(tex.image.size());
            std::memcpy(tex_info.data.data(), tex.image.data(), tex_info.data.size());
            
            // attach texture to new material;
            new_material.diffuse_texture = std::move(tex_info);
        }
        out.materials[idx] = std::move(new_material);
    }

    //https://github.com/KhronosGroup/glTF-Tutorials/blob/main/gltfTutorial/gltfTutorial_007_Animations.md
    out.animations.resize(model.animations.size());
    for (std::size_t idx = 0; const auto& animation : model.animations)
    {
        AnimationInfo new_animation{};
        new_animation.name = animation.name;
        new_animation.channels.resize(animation.channels.size());
        for (std::size_t ch_idx = 0; const auto& ch : animation.channels)
        {
            const auto& sampler = animation.samplers[ch.sampler];
            assert(sampler.interpolation == "LINEAR"); // ToDo: add support for other interploation types
            
            const auto& accessor_timestamps = model.accessors[sampler.input];
            const auto& buffer_view_timestamps = model.bufferViews[accessor_timestamps.bufferView];
            assert(accessor_timestamps.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(accessor_timestamps.type == TINYGLTF_TYPE_SCALAR);

            const auto& accessor_data = model.accessors[sampler.output];
            const auto& buffer_view_data = model.bufferViews[accessor_data.bufferView];
            assert(accessor_data.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(accessor_data.type == TINYGLTF_TYPE_VEC3 || accessor_data.type == TINYGLTF_TYPE_VEC4);

            // some additional validation
            {
                const auto stride_timestamps = accessor_timestamps.ByteStride(buffer_view_timestamps);
                assert(stride_timestamps == sizeof(float));
                const auto stride_data = accessor_data.ByteStride(buffer_view_timestamps);
                assert(stride_data == 3 * sizeof(float) || stride_data == 4 * sizeof(float));
            }

            auto& new_channel = new_animation.channels[ch_idx];
            if (ch.target_path.compare("rotation") == 0)
            {
                new_channel.type = ENGINE_ANIMATION_PROPERTY_TYPE_ROTATION;
            }
            else if (ch.target_path.compare("translation") == 0)
            {
                new_channel.type = ENGINE_ANIMATION_PROPERTY_TYPE_TRANSLATION;
            }
            else if (ch.target_path.compare("scale") == 0)
            {
                new_channel.type = ENGINE_ANIMATION_PROPERTY_TYPE_SCALE;
            }
            else
            {
                assert(false && "Unknown target path for animation!");
            }

            assert(accessor_timestamps.count == accessor_data.count);
            new_channel.timestamps.resize(accessor_timestamps.count * tinygltf::GetNumComponentsInType(accessor_timestamps.type));
            const auto& buffer_timings = reinterpret_cast<const float*>(model.buffers[buffer_view_timestamps.buffer].data.data() + accessor_timestamps.byteOffset + buffer_view_timestamps.byteOffset);
            std::memcpy(new_channel.timestamps.data(), buffer_timings, new_channel.timestamps.size() * sizeof(float));
            // go over each element and scale by 1000 to get miliseconds
            std::for_each(new_channel.timestamps.begin(), new_channel.timestamps.end(), [](auto& v) {v *= 1000.0f; });

            new_channel.data.resize(accessor_data.count * tinygltf::GetNumComponentsInType(accessor_data.type));
            const auto& buffer_data = reinterpret_cast<const float*>(model.buffers[buffer_view_data.buffer].data.data() + accessor_data.byteOffset + buffer_view_data.byteOffset);
            std::memcpy(new_channel.data.data(), buffer_data, new_channel.data.size() * sizeof(float));

        }
        out.animations[idx] = std::move(new_animation);
    }

    return out;
}
