#include "scene.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <GLFW/glfw3.h>


engine::Scene::Scene(engine_result_code_t& out_code)
    : shader_(Shader("simple.vs", "simple.fs"))
{
    out_code = ENGINE_RESULT_CODE_OK;
}

engine::Scene::~Scene()
{
}

engine_result_code_t engine::Scene::update(RenderContext& rdx, float dt, std::span<const Texture2D> textures, std::span<const Geometry> geometries)
{
    // TRANSFORM SYSTEM
    auto transform_view = entity_registry_.view<engine_tranform_component_t>();
    transform_view.each([](engine_tranform_component_t& transform)
        {
            auto model_identity = glm::mat4{ 1.0f };

            const auto glm_pos = glm::make_vec3(transform.position);
            const auto glm_rot = glm::make_vec3(transform.rotation);
            const auto glm_scl = glm::make_vec3(transform.scale);

            auto translation = glm::translate(model_identity, glm_pos);
            translation *= glm::toMat4(glm::quat(glm_rot));
			//translation = glm::rotate(translation, glm::radians(0.0f), glm::vec3(1.0f, 0.3f, 0.5f));
            translation = glm::scale(translation, glm_scl);
            std::memcpy(transform.local_to_world, &translation, sizeof(translation));
        }
    );

	auto renderables_view = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t, const engine_material_component_t>();
    auto camera_view = entity_registry_.view<const engine_camera_component_t, const engine_tranform_component_t>();
    for (auto [entity, camera, transform] : camera_view.each()) 
    {
        if (!camera.enabled)
        {
            continue;
        }

        shader_.bind();
        // update camera: view and projection
        {
            const auto z_near = camera.clip_plane_near;
            const auto z_far = camera.clip_plane_far;

            std::int32_t width = -1;
            std::int32_t height = -1;
            glfwGetWindowSize(rdx.get_glfw_window(), &width, &height);

            const auto adjusted_width = width * (camera.viewport_rect.width - camera.viewport_rect.x);
            const auto adjusted_height = height * (camera.viewport_rect.height - camera.viewport_rect.y);
            const float aspect = adjusted_width / adjusted_height;
            glm::mat4 projection;
            if (camera.type == ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC)
            {
                const float scale = camera.type_union.orthographics_scale;
                projection = glm::ortho(-aspect * scale, aspect * scale, -scale, scale, z_near, z_far);
            }
            else
            {
                projection = glm::perspective(glm::radians(camera.type_union.perspective_fov), aspect, z_near, z_far);
            }
            const auto eye_position = glm::make_vec3(transform.position);
            const auto up = glm::make_vec3(camera.direction.up);
            const auto target = glm::make_vec3(camera.target);
            const auto view = glm::lookAt(eye_position, target, up);

            shader_.set_uniform_mat4f("view", { glm::value_ptr(view), sizeof(view) / sizeof(float) });
            shader_.set_uniform_mat4f("projection", { glm::value_ptr(projection), sizeof(projection) / sizeof(float) });
        }

		renderables_view.each([this, &textures, &geometries](const engine_tranform_component_t& transform, const engine_mesh_component_t& mesh, const engine_material_component_t& material)
			{
				shader_.set_uniform_mat4f("model", transform.local_to_world);
				textures[material.diffuse_texture].bind(0); // bind at diffuse slot

                geometries[mesh.geometry].bind();
                geometries[mesh.geometry].draw(Geometry::Mode::eTriangles);
			}
		);
    }

    return ENGINE_RESULT_CODE_OK;
}

entt::entity engine::Scene::create_new_entity()
{
    return entity_registry_.create();
}

void engine::Scene::destroy_entity(entt::entity entity)
{
    entity_registry_.destroy(entity);
}

