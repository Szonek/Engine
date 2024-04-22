#include <engine.h>

#include <vector>
#include <string_view>

namespace project_c
{
struct ModelInfo
{
    engine_application_t& app;
    engine_model_desc_t model_info{};
    std::vector<engine_geometry_t> geometries;
    std::vector<engine_texture2d_t> textures;
    std::vector<engine_material_t> materials;

    ModelInfo(engine_result_code_t& engine_error_code, engine_application_t& app, std::string_view model_file_name)
        : app(app)
    {

        engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, model_file_name.data(), &model_info);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed loading TABLE model. Exiting!\n");
            return;
        }

        geometries = std::vector(model_info.geometries_count, ENGINE_INVALID_OBJECT_HANDLE);
        for (std::uint32_t i = 0; i < model_info.geometries_count; i++)
        {
            const auto& geo = model_info.geometries_array[i];
            const auto name = "unnamed_geometry__" + std::to_string(i);
            engine_error_code = engineApplicationAddGeometryFromDesc(app, &geo, name.c_str(), &geometries[i]);
            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating geometry for loaded model. Exiting!\n");
                return;
            }
        }

        textures = std::vector<engine_texture2d_t>(model_info.textures_count, ENGINE_INVALID_OBJECT_HANDLE);
        for (std::uint32_t i = 0; i < model_info.textures_count; i++)
        {
            const auto name = "unnamed_texture_" + std::to_string(i);
            engine_error_code = engineApplicationAddTexture2DFromDesc(app, &model_info.textures_array[i], name.c_str(), &textures[i]);
            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating texture for loaded model. Exiting!\n");
                return;
            }
        }

        materials = std::vector<engine_material_t>(model_info.materials_count, ENGINE_INVALID_OBJECT_HANDLE);
        for (std::uint32_t i = 0; i < model_info.materials_count; i++)
        {
            const auto& mat = model_info.materials_array[i];
            engine_material_create_desc_t mat_create_desc{};
            set_c_array(mat_create_desc.diffuse_color, mat.diffuse_color);
            if (mat.diffuse_texture_index != -1)
            {
                mat_create_desc.diffuse_texture = textures.at(mat.diffuse_texture_index);
            }
            engine_error_code = engineApplicationAddMaterialFromDesc(app, &mat_create_desc, mat.name, &materials[i]);

            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating textured for loaded model. Exiting!\n");
                return;
            }
        }
    }

    ~ModelInfo()
    {
        if (model_info.nodes_count > 0)
        {
            engineApplicationReleaseModelDesc(app, &model_info);
        }
    }
};
}