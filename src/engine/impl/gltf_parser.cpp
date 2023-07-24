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

        if (position_data.count == 0 || uv_0_data.count == 0 || (position_data.count != uv_0_data.count))
        {
            engine::log::log(engine::log::LogLevel::eCritical, 
                fmt::format("Cant load mesh correctly. Count of positions: {}, count of uv: {} \n. \
                    One of the attrib is 0, or cound do not equal to each other.\n",
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
    const auto load_success = loader.LoadBinaryFromMemory(&model, &load_error_msg, &load_warning_msg,
                                                          data.data(), data.size_bytes(), "");

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

    for (std::size_t idx = 0; auto& mesh : model.meshes)
    {
        out.geometries[idx] = parse_mesh(mesh, model);
        idx++;
    }

    //https://github.com/syoyo/tinygltf/blob/release/examples/basic/main.cpp
    for(auto& scene : model.scenes)
    {
        for (std::size_t i = 0; i < scene.nodes.size(); ++i)
        {
            assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
            const auto& scene_node = model.nodes[scene.nodes[i]];
            //parse_model_nodes(out, model, scene_node);
        }
    }
    return out;
}
