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

    for (std::size_t prim_idx = 0; prim_idx < mesh.primitives.size(); prim_idx++)
    {
        const auto& primitive = mesh.primitives[prim_idx];
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

        static const std::map<engine_vertex_attribute_type_t, std::uint32_t> expected_num_components = []()
        {
            std::map<engine_vertex_attribute_type_t, std::uint32_t> ret;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_POSITION]  = 3u;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_NORMALS]   = 3u;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_UV_0]      = 2u;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_JOINTS_0]  = 4u;
            ret[ENGINE_VERTEX_ATTRIBUTE_TYPE_WEIGHTS_0] = 4u;
            return ret;
        }();

        struct attrib_info
        {
            const std::byte* data = nullptr;
            std::uint32_t param_type = 0;
            std::size_t count = 0;
            std::uint32_t num_components = 0;
            std::uint32_t data_stride = 0;

            inline std::size_t get_size() const
            {
                return count * num_components * tinygltf::GetComponentSizeInBytes(param_type);
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

            //ToDo: this is not correct, bone indcies are 2 bytes (unsigned shorts)

            if (expected_attrib_names.find(attrib.first) != expected_attrib_names.end())
            {
                const auto attrib_type = expected_attrib_names.at(attrib.first);
                attribs[attrib_type].data = reinterpret_cast<const std::byte*>(buffer.data.data() + buffer_view.byteOffset + attrib_accesor.byteOffset);
                attribs[attrib_type].count = attrib_accesor.count;
                attribs[attrib_type].num_components = attrib_num_components;
                attribs[attrib_type].param_type = attrib_accesor.componentType;
                attribs[attrib_type].data_stride = attrib_stride;
                assert(attribs[attrib_type].num_components == expected_num_components.at(attrib_type));
            }
            else
            {
                engine::log::log(engine::log::LogLevel::eError, fmt::format("Unexpected vertex attrivute: {} for mesh: {} \n", attrib.first, mesh.name));
            }
        }
        // we can set number of vertices
        ret.vertex_count = static_cast<std::int32_t>(position_data.count);

        // some validation
        if (position_data.count == 0)
        {
            engine::log::log(engine::log::LogLevel::eCritical, 
                fmt::format("Cant load mesh correctly. Count of positions: {}.\n", position_data.count));
        }

        if (normals_data.count > 0 && normals_data.count != position_data.count)
        {
            engine::log::log(engine::log::LogLevel::eCritical,
                fmt::format("Cant load mesh correctly. Count of positions: {}, count of uv: {}.\n",
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
            attrib.param_type = TINYGLTF_PARAMETER_TYPE_FLOAT;
        }

        // create layout returned to user
        {
            auto to_engine_vertex_data_type = [](std::size_t tinygltf_param_type)
            {
                switch (tinygltf_param_type)
                {
                case TINYGLTF_PARAMETER_TYPE_FLOAT:          return ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_FLOAT32;
                case TINYGLTF_PARAMETER_TYPE_INT:            return ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT32;
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:   return ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT32;
                case TINYGLTF_PARAMETER_TYPE_SHORT:          return ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_INT16;
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: return ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UINT16;
                default:
                    assert(false && !"Unknown conversion of param type to engine vertex attribute data type!");
                }
                return ENGINE_VERTEX_ATTRIBUTE_DATA_TYPE_UNKNOWN;
            };

            std::size_t attrib_added_idx = 0;
            for (std::size_t i = 0; i < ENGINE_VERTEX_ATTRIBUTE_TYPE_COUNT; i++)
            {
                if (attribs[i].num_components > 0)
                {
                    ret.vertex_laytout.attributes[attrib_added_idx].elements_data_type = to_engine_vertex_data_type(attribs[i].param_type);
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
            std::for_each(attribs.begin(), attribs.end(), [&ret](const auto& attrib) { ret += attrib.get_size(); });
            return ret;
        }();
        const auto single_vertex_size = total_verts_buffer_size / position_data.count;

        // copy data into buffer returned to user
        ret.vertex_data.resize(total_verts_buffer_size, std::byte(0));    
        auto* verts_ptr = ret.vertex_data.data();
        for (auto i = 0; i < position_data.count; i++)
        {
            const std::size_t vertex_offset = i * single_vertex_size;
            std::size_t attrib_offset = 0;
            for (auto j = 0; j < attribs.size(); j++)
            {
                const auto& attrib = attribs[j];
                const auto attrib_packed_stride = attrib.num_components * tinygltf::GetComponentSizeInBytes(attrib.param_type);
                if (attrib.data)
                {
                    assert(attrib.data_stride != 0);
                    std::memcpy(verts_ptr + vertex_offset + attrib_offset, attrib.data + i * attrib.data_stride, attrib_packed_stride);
                }
                assert(attrib.num_components != 0);
                assert(attrib.param_type != 0);
                attrib_offset += attrib_packed_stride;
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
            data.data(), static_cast<std::uint32_t>(data.size_bytes()), "");
    }
    else
    {
        load_success = loader.LoadASCIIFromString(&model, &load_error_msg, &load_warning_msg, reinterpret_cast<const char*>(data.data()), static_cast<std::uint32_t>(data.size()), {});
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
        auto& new_skin = out.skins[skin_idx];
        new_skin.joints.reserve(skin.joints.size());
        for (std::size_t i = 0; i < skin.joints.size(); i++)
        {
            SkinJointDesc joint_info{};
            joint_info.idx = static_cast<std::int32_t>(i);
            for (const auto& c : model.nodes[skin.joints[i]].children)
            {
                joint_info.childrens.push_back(static_cast<int32_t>(std::distance(std::find(skin.joints.begin(), skin.joints.end(), c), skin.joints.end())));
            }
            /*
                https://lisyarus.github.io/blog/graphics/2023/07/03/gltf-animation.html
                However, the vertices of the model are in, well, the model’s coordinate system (that’s the definition of this coordinate system).
                So, we need a way to transform the vertices into the local coordinate system of the bone first.
                This is called an inverse bind matrix, because it sounds really cool.
            */
            const auto inv_bind_mtx_accesor = model.accessors[skin.inverseBindMatrices];
            const auto inv_bind_mtx_buffer_view = model.bufferViews[model.accessors[skin.inverseBindMatrices].bufferView];
            const auto inv_bind_mtx_buffer = reinterpret_cast<float*>(model.buffers[inv_bind_mtx_buffer_view.buffer].data.data() + inv_bind_mtx_buffer_view.byteOffset + inv_bind_mtx_accesor.byteOffset);
            joint_info.inverse_bind_matrix = glm::make_mat4x4(inv_bind_mtx_buffer + i * 16);

            new_skin.joints.push_back(std::move(joint_info));
        }
        skin_idx++;
    }
    //https://github.com/KhronosGroup/glTF-Tutorials/blob/main/gltfTutorial/gltfTutorial_007_Animations.md
    out.animations.resize(model.animations.size());
    for (std::size_t idx = 0; const auto& animation : model.animations)
    {
        AnimationClipInfo new_animation{};
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

            auto& new_channel = new_animation.channels[ch_idx++];
            new_channel.target_node_idx = 0;
            if (model.skins.size() > 0)
            {
                const auto& skin = model.skins[0];
                new_channel.target_node_idx = static_cast<int32_t>(std::distance(std::find(skin.joints.begin(), skin.joints.end(), ch.target_node), skin.joints.end()));
            }
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
