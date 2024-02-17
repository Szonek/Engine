#include "gltf_parser.h"
#include "logger.h"
#include "logger.h"

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include <fmt/format.h>

#include "glm/glm.hpp"
#include <glm/gtc/type_ptr.hpp>


namespace
{
inline engine::GeometryInfo parse_mesh(const tinygltf::Mesh& mesh, const tinygltf::Model& model)
{
    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_mesh_primitive_mode

    engine::GeometryInfo ret{};
    ret.material_index = mesh.primitives.front().material;
    //assert(mesh.primitives.size() == 1 && "Not enabled path for primitives count > 1");

    for (std::size_t i = 0; i < mesh.primitives.size(); i++)
    {
        const auto& primitive = mesh.primitives[i];
        assert(ret.material_index  == primitive.material && "Currently not supporting multi-material meshes!");

        static const std::map<std::string, engine_vertex_attribute_type_t> expected_attrib_names = []()
        {
            std::map<std::string, engine_vertex_attribute_type_t> ret;
            ret["POSITION"]   = ENGINE_VERTEX_ATTRIBUTE_TYPE_POSITION;
            ret["TEXCOORD_0"] = ENGINE_VERTEX_ATTRIBUTE_TYPE_UV_0;
            ret["JOINTS_0"]   = ENGINE_VERTEX_ATTRIBUTE_TYPE_JOINTS_0;
            ret["WEIGHTS_0"]  = ENGINE_VERTEX_ATTRIBUTE_TYPE_WEIGHTS_0;
            return ret;
        }();

        static const std::map<engine_vertex_attribute_type_t, std::size_t> expected_num_components = []()
        {
            std::map<engine_vertex_attribute_type_t, std::size_t> ret;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_POSITION]  = 3;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_NORMALS]   = 3;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_UV_0]      = 2;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_JOINTS_0]  = 4;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_WEIGHTS_0] = 4;
            return ret;
        }();

        struct attrib_info
        {
            const float* data = nullptr;
            std::size_t count = 0;
            std::size_t num_components = 0;

            inline std::size_t get_single_vertex_size() const
            {
                return count * num_components * sizeof(data[0]);
            }
        };
        std::array<attrib_info, ENGINE_VERTEX_ATTRIBUTE_TYPE_COUNT> attribs{};
        
        auto& position_data  = attribs[ENGINE_VERTEX_ATTRIBUTE_TYPE_POSITION];
        auto& normals_data   = attribs[ENGINE_VERTEX_ATTRIBUTE_TYPE_NORMALS];
        auto& uv_0_data      = attribs[ENGINE_VERTEX_ATTRIBUTE_TYPE_UV_0];
        auto& joints_0_data  = attribs[ENGINE_VERTEX_ATTRIBUTE_TYPE_JOINTS_0];
        auto& weights_0_data = attribs[ENGINE_VERTEX_ATTRIBUTE_TYPE_WEIGHTS_0];

        for (const auto& attrib : primitive.attributes)
        {
            const auto& attrib_accesor = model.accessors[attrib.second];
            const auto& buffer_view = model.bufferViews[attrib_accesor.bufferView];
            const auto& buffer = model.buffers[buffer_view.buffer];

            const auto attrib_num_components = tinygltf::GetNumComponentsInType(attrib_accesor.type);
            const auto attrib_stride = attrib_accesor.ByteStride(model.bufferViews[attrib_accesor.bufferView]);


            if (expected_attrib_names.find(attrib.first) != expected_attrib_names.end())
            {
                const auto attrib_type = expected_attrib_names.at(attrib.first);
                attribs[attrib_type].data = reinterpret_cast<const float*>(buffer.data.data() + buffer_view.byteOffset);
                attribs[attrib_type].count = attrib_accesor.count;
                attribs[attrib_type].num_components = attrib_num_components;
                assert(attribs[attrib_type].num_components == expected_num_components.at(attrib_type));
            }
            else
            {
                engine::log::log(engine::log::LogLevel::eError, fmt::format("Unexpected vertex attrivute: {} for mesh: {} \n", attrib.first, mesh.name));
            }
        }
        // we can set number of vertices
        ret.vertex_count = position_data.count;

        // some validation
        if (position_data.count == 0)
        {
            engine::log::log(engine::log::LogLevel::eCritical, 
                fmt::format("Cant load mesh correctly. Count of positions: {}, count of uv: {} .\n",
                    position_data.count, uv_0_data.count));
        }

        if (joints_0_data.count != weights_0_data.count)
        {
            engine::log::log(engine::log::LogLevel::eError,
                fmt::format("Cant load vertex joints and weights correctly. Will not load them.\n"));
            joints_0_data = {};
            weights_0_data = {};
        }

        if (joints_0_data.count > 0 && position_data.count != joints_0_data.count)
        {
            engine::log::log(engine::log::LogLevel::eCritical,
                fmt::format("Cant load mesh correctly. Count of positions: {}, count of joints: {} .\n",
                    position_data.count, joints_0_data.count));
        }

        // Fill missing gaps  <- because we are using common shader
        // ToDo: remove this and make shaders detect attributs components (should have better performance)
        // This approach will have gaps in buffer -> waste of memory and performance
        for (auto i = 0; i < attribs.size(); i++)
        {
            auto& attrib = attribs[i];
            if (attrib.data)
            {
                continue;
            }
            attrib.num_components = expected_num_components.at(static_cast<engine_vertex_attribute_type_t>(i));
            attrib.count = ret.vertex_count;
        }

        // create layout returned to user
        {
            std::size_t attrib_added_idx = 0;
            for (std::size_t i = 0; i < ENGINE_VERTEX_ATTRIBUTE_TYPE_COUNT; i++)
            {
                if (attribs[i].num_components > 0)
                {
                    ret.vertex_laytout.attributes[attrib_added_idx].elements_data_type = ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_FLOAT;
                    ret.vertex_laytout.attributes[attrib_added_idx].elements_count = attribs[i].num_components;
                    ret.vertex_laytout.attributes[attrib_added_idx].type = static_cast<engine_vertex_attribute_type_t>(i);
                    attrib_added_idx++;
                }
            }
        }

        // verts buffer size
        const auto total_verts_buffer_size = [&attribs]()
        {
            std::size_t ret = 0;
            std::for_each(attribs.begin(), attribs.end(), [&ret](const auto& attrib) { ret += attrib.get_single_vertex_size(); });
            return ret;
        }();

        // copy data into buffer returned to user
        ret.vertex_data.resize(total_verts_buffer_size, std::byte(0));      
        float* verrts_ptr = reinterpret_cast<float*>(ret.vertex_data.data());
        for (auto i = 0; i < position_data.count; i++)
        {
            for (const auto& attrib : attribs)
            {
                for (auto c = 0; c < attrib.num_components; c++)
                {
                    *verrts_ptr = attrib.data ? attrib.data[c + i * attrib.num_components] : 0.0f;
                    verrts_ptr++;
                }
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

    //https://github.com/KhronosGroup/glTF-Tutorials/blob/main/gltfTutorial/gltfTutorial_019_SimpleSkin.md
    out.skins.resize(model.skins.size());
    for (std::size_t skin_idx = 0; const auto& skin : model.skins)
    {
        for (std::size_t i = 0; i < skin.joints.size(); i++)
        {
            SkinJointInfo joint_info{};
            joint_info.idx = skin.joints[i];
            joint_info.childrens = model.nodes[joint_info.idx].children;
            const auto inv_bind_mtx_accesor = model.accessors[skin.inverseBindMatrices];
            const auto inv_bind_mtx_buffer_view = model.bufferViews[model.accessors[skin.inverseBindMatrices].bufferView];
            const auto inv_bind_mtx_buffer = reinterpret_cast<float*>(model.buffers[inv_bind_mtx_buffer_view.buffer].data.data() + inv_bind_mtx_buffer_view.byteOffset + inv_bind_mtx_accesor.byteOffset);
            joint_info.inverse_bind_matrix = glm::make_mat4x4(inv_bind_mtx_buffer + i * 16);
        }
        skin_idx++;
    }
    //https://github.com/KhronosGroup/glTF-Tutorials/blob/main/gltfTutorial/gltfTutorial_007_Animations.md
    out.animations.resize(model.animations.size());
    for (std::size_t idx = 0; const auto& animation : model.animations)
    {
        AnimationInfo new_animation{};
        new_animation.name = animation.name;
        new_animation.clip.channels.resize(animation.channels.size());
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

            auto& new_channel = new_animation.clip.channels[ch_idx++];
            if (ch.target_path.compare("rotation") == 0)
            {
                new_channel.type = ENGINE_ANIMATION_CHANNEL_TYPE_ROTATION;
            }
            else if (ch.target_path.compare("translation") == 0)
            {
                new_channel.type = ENGINE_ANIMATION_CHANNEL_TYPE_TRANSLATION;
            }
            else if (ch.target_path.compare("scale") == 0)
            {
                new_channel.type = ENGINE_ANIMATION_CHANNEL_TYPE_SCALE;
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
