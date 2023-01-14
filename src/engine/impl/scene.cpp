#include "scene.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <GLFW/glfw3.h>

struct vertex_attribute_simple_t
{
	std::uint32_t size;
	engine::Geometry::vertex_attribute_t::Type type;
};
inline std::vector<engine::Geometry::vertex_attribute_t> create_tightly_packed_vertex_layout(std::span<const vertex_attribute_simple_t> simple_attribs)
{
	using namespace engine;
	std::uint32_t stride = 0;
	std::vector<std::uint32_t> offsets;
	offsets.reserve(simple_attribs.size()); // we will use push_back();

	std::ranges::for_each(simple_attribs, [&stride, &offsets](const vertex_attribute_simple_t& attrib)
		{
			offsets.push_back(stride);
	std::uint32_t bytes_size = 0;
	switch (attrib.type)
	{
	case Geometry::vertex_attribute_t::Type::eFloat:
		bytes_size = sizeof(float) * attrib.size;
	}
	stride += bytes_size;
		}
	);

	// calc total stride to the next vertex
	// construct layout
	std::vector<Geometry::vertex_attribute_t> vertex_layout;
	vertex_layout.resize(simple_attribs.size());
	for (std::uint32_t idx = 0; auto & attrib : vertex_layout)
	{
		attrib.index = idx;
		attrib.stride = stride;
		attrib.size = simple_attribs[idx].size;
		attrib.type = simple_attribs[idx].type;
		attrib.offset = offsets[idx];
		idx++;
	}
	return vertex_layout;

}

inline auto create_geometry()
{
	using namespace engine;
	std::vector< vertex_attribute_simple_t> vertex_layout_simple;
	vertex_layout_simple.push_back({ 3,  Geometry::vertex_attribute_t::Type::eFloat });
	vertex_layout_simple.push_back({ 2,  Geometry::vertex_attribute_t::Type::eFloat });
	float vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
	};
	//std::uint32_t indices[] =
	//{
	//	0, 1, 3,   // first triangle
	//	1, 2, 3    // second triangle
	//};
	return Geometry(create_tightly_packed_vertex_layout(vertex_layout_simple), vertices, {});
}

engine::Scene::Scene(engine_result_code_t& out_code)
    : shader_(Shader("simple.vs", "simple.fs"))
    , geometry_(create_geometry())
    , texture_container_(Texture2D("container.jpg", true))
    , texture_face_(Texture2D("awesomeface.png", true))
{
    out_code = ENGINE_RESULT_CODE_OK;
}

engine::Scene::~Scene()
{
}

engine_result_code_t engine::Scene::update(RenderContext& rdx, float dt)
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

	auto renderables_view = entity_registry_.view<const engine_tranform_component_t, const engine_mesh_component_t>();
    auto camera_view = entity_registry_.view<engine_camera_component_t>();
    for (auto [entity, camera] : camera_view.each()) 
    {
        glm::mat4 view = glm::mat4(1.0f);
        // note that we're translating the scene in the reverse direction of where we want to move
        view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

        std::int32_t width = -1;
        std::int32_t height = -1;
        glfwGetWindowSize(rdx.get_glfw_window(), &width, &height);
        glm::mat4 projection;
        const auto aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
        projection = glm::perspective(glm::radians(45.0f), aspect_ratio, 0.1f, 100.0f);

		shader_.bind();
        shader_.set_uniform_mat4f("view", { glm::value_ptr(view), sizeof(view) / sizeof(float) });
        shader_.set_uniform_mat4f("projection", { glm::value_ptr(projection), sizeof(projection) / sizeof(float) });

		renderables_view.each([this](const engine_tranform_component_t& transform, const engine_mesh_component_t&)
			{
				shader_.set_uniform_mat4f("model", transform.local_to_world);
				texture_container_.bind(0);
				texture_face_.bind(1);

				geometry_.bind();
				geometry_.draw(Geometry::Mode::eTriangles);
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

