#include <engine.h>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <span>
#include <cassert>
#include <memory>

#include <SDL_system.h>
#include <SDL_main.h>

#include <fmt/format.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#if __ANDROID__
constexpr const bool K_IS_ANDROID = true;
#else  
constexpr const bool K_IS_ANDROID = false;
#endif

template<typename T>
inline void set_c_array(std::span<float> in, const T& data)
{
    assert(in.size() == data.size());
    std::memcpy(in.data(), data.data(), in.size_bytes());
}

template<typename T, std::size_t S>
inline std::array<T, S>  to_array(const T (&data)[S])
{
    std::array<T, S> ret{};
    std::memcpy(ret.data(), data, S * sizeof(T));
    return ret;
}

template<typename T>
inline auto get_spherical_coordinates(const T& cartesian)
{
	const float r = std::sqrt(
		std::pow(cartesian[0], 2) +
		std::pow(cartesian[1], 2) +
		std::pow(cartesian[2], 2)
	);


	float phi = std::atan2(cartesian[2] / cartesian[0], cartesian[0]);
	const float theta = std::acos(cartesian[1] / r);

	if (cartesian[0] < 0)
		phi += 3.1415f;

	std::array<float, 3> ret{ 0.0f };
	ret[0] = r;
	ret[1] = phi;
	ret[2] = theta;
	return ret;
}

template<typename T>
inline std::array<float, 3> get_cartesian_coordinates(const T& spherical)
{
	std::array<float, 3> ret{ 0.0f };

	ret[0] = spherical[0] * std::cos(spherical[2]) * std::cos(spherical[1]);
	ret[1] = spherical[0] * std::sin(spherical[2]);
	ret[2] = spherical[0] * std::cos(spherical[2]) * std::sin(spherical[1]);

	return ret;
}

class OribitingCamera
{
public:
	// https://nerdhut.de/2020/05/09/unity-arcball-camera-spherical-coordinates/
	void update(engine_game_object_t entt, engine_application_t app, float dt, engine_scene_t scene)
	{
		if (engineSceneHasCameraComponent(scene, entt))
		{
			auto tc = engineSceneGetTransformComponent(scene, entt);
			auto cc = engineSceneGetCameraComponent(scene, entt);

			if (engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT))
			{
				auto coords = engineApplicationGetMouseCoords(app);

				engine_mouse_coords_t d = coords;
				d.x -= prev_mouse_coords_.x;
				d.y -= prev_mouse_coords_.y;

				if (d.x != 0 || d.y != 0)
				{
					// Rotate the camera left and right
					sc_[1] += d.x * dt * 0.0001f;

					// Rotate the camera up and down
					// Prevent the camera from turning upside down (1.5f = approx. Pi / 2)
					sc_[2] = std::clamp(sc_[2] + d.y * dt * 0.0001f, -1.5f, 1.5f);

					const auto new_position = get_cartesian_coordinates(sc_);
					tc->position[0] = new_position[0] + cc->target[0];
					tc->position[1] = new_position[1] + cc->target[1];
					tc->position[2] = new_position[2] + cc->target[2];

				}


			}

			if (prev_mouse_coords_.x == -1)
			{
				sc_ = get_spherical_coordinates(tc->position);
			}
			prev_mouse_coords_ = engineApplicationGetMouseCoords(app);
		}
	}

private:
	std::array<float, 3> sc_;

	engine_mouse_coords_t prev_mouse_coords_{ -1, -1 };
};

struct aabb2d_t
{
    float x_min = 0;
    float y_min = 0;
    float x_max = 0;
    float y_max = 0;
};

enum class CollisionSide
{
    eNone = 0,
    eLeft,
    eRight,
    eTop,
    eBottom
};

inline CollisionSide detect_collision(aabb2d_t ball, aabb2d_t obstacle)
{
    if (ball.x_max < obstacle.x_min || ball.x_min > obstacle.x_max)
    {
        return CollisionSide::eNone;
    }
    if (ball.y_max < obstacle.y_min || ball.y_min > obstacle.y_max)
    {
        return CollisionSide::eNone;
    }

    if (ball.y_max > obstacle.y_min)
    {
        return CollisionSide::eBottom;
    }
    if (ball.y_min < obstacle.y_max)
    {
        return CollisionSide::eTop;
    }

    return CollisionSide::eLeft;
}

struct collision_t
{
    engine_game_object_t object;
    glm::vec3 hitpoint_orientation;
};

class IScript
{
public:
    virtual void update(float dt) = 0;
    virtual ~IScript() {}
};

class PlayersScript : public IScript
{
public:


    PlayersScript(engine_application_t app, engine_scene_t scene, engine_geometry_t geometry, engine_font_t font)
        : scene_(scene)
        , app_(app)
        , velocity_left_({0.0f, 0.0f, 0.0f})
        , velocity_right_({0.0f, 0.0f, 0.0f})
    {
        {
            player_left_go_ = engineSceneCreateGameObject(scene);
            auto mesh_comp = engineSceneAddMeshComponent(scene, player_left_go_);
            mesh_comp->geometry = geometry;

            auto tc = engineSceneAddTransformComponent(scene, player_left_go_);
            tc->position[0] = -3.0f;
            tc->position[1] = 0.0f;
            tc->position[2] = 0.0f;

            tc->scale[0] = 0.1f;
            tc->scale[1] = 0.1f;
            tc->scale[2] = 1.1f;

            auto material_comp = engineSceneAddMaterialComponent(scene, player_left_go_);
            set_c_array(material_comp->diffuse_color, std::array<float, 4>{ 0.4f, 0.3f, 1.0f, 0.2f });

            auto nc = engineSceneAddNameComponent(scene, player_left_go_);
            const std::string name = fmt::format("player_left"); 
            std::strcpy(nc->name, name.c_str());
        }
        {
            player_right_go_ = engineSceneCreateGameObject(scene);
            auto mesh_comp = engineSceneAddMeshComponent(scene, player_right_go_);
            mesh_comp->geometry = geometry;

            auto tc = engineSceneAddTransformComponent(scene, player_right_go_);
            tc->position[0] = 3.0f;
            tc->position[1] = 0.0f;
            tc->position[2] = 0.0f;

            tc->scale[0] = 0.1f;
            tc->scale[1] = 0.1f;
            tc->scale[2] = 1.1f;

            auto material_comp = engineSceneAddMaterialComponent(scene, player_right_go_);
            set_c_array(material_comp->diffuse_color, std::array<float, 4>{ 0.4f, 0.3f, 1.0f, 0.2f });

            auto nc = engineSceneAddNameComponent(scene, player_right_go_);
            const std::string name = fmt::format("player_right");
            std::strcpy(nc->name, name.c_str());
        }


        // plaer 1 text
        if(font)
        {
            // names
            const auto obj_go = engineSceneCreateGameObject(scene);
            auto text_component = engineSceneAddTextComponent(scene, obj_go);
            text_component->font_handle = font;
            text_component->text = "Player 1";

            auto tc = engineSceneAddRectTransformComponent(scene, obj_go);
            tc->position[0] = 0.15f;
            tc->position[1] = 0.15f;

            tc->scale[0] = 0.5f;
            tc->scale[1] = 0.5f;

            auto material_comp = engineSceneAddMaterialComponent(scene, obj_go);
            set_c_array(material_comp->diffuse_color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
        }

        // player 2 text
        if(font)
        {
            // names
            const auto obj_go = engineSceneCreateGameObject(scene);
            auto text_component = engineSceneAddTextComponent(scene, obj_go);
            text_component->font_handle = font;
            text_component->text = "Player 2";

            auto tc = engineSceneAddRectTransformComponent(scene, obj_go);
            tc->position[0] = 0.75f;
            tc->position[1] = 0.15f;

            tc->scale[0] = 0.5f;
            tc->scale[1] = 0.5f;

            auto material_comp = engineSceneAddMaterialComponent(scene, obj_go);
            set_c_array(material_comp->diffuse_color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
        }
    }

    void update(float dt) override
    {
        process_keyboard_inputs(dt);
    }


    collision_t is_colliding(const aabb2d_t& ball_aabb, std::array<float, 3> ball_pos) const
    {
        const float angle_coeff = 3.5f;
        auto get_z_dim_normalized_intersect = [](const auto obj_pos, const auto player_tc, const auto paddel_height)
        {
            const auto intersect_pos = player_tc->position[2] - obj_pos;
            const auto normalized_intersect_pos = (player_tc->position[2] - obj_pos) / (paddel_height / 2.0f);
            return normalized_intersect_pos;
        };

        auto get_hitpoint_orientation = [&angle_coeff](const float x_dir, const float normalized_hit_z)
        {
            const auto bottom_part_of_paddle = std::signbit(normalized_hit_z);
            const auto z_scaled = normalized_hit_z / angle_coeff;
            const auto x_speed = 2.0f;
            return  glm::normalize(glm::vec3(x_dir, 0.0, -1.0f * z_scaled));
        };

        collision_t ret;
        ret.object = ENGINE_INVALID_GAME_OBJECT_ID;

        auto tc_pl0 = engineSceneGetTransformComponent(scene_, player_left_go_);
        aabb2d_t palyer_left_aabb{};
        palyer_left_aabb.x_min = tc_pl0->position[0] - 0.5f * tc_pl0->scale[0];
        palyer_left_aabb.x_max = tc_pl0->position[0] + 0.5f * tc_pl0->scale[0];
        palyer_left_aabb.y_min = tc_pl0->position[2] - 0.5f * tc_pl0->scale[2];
        palyer_left_aabb.y_max = tc_pl0->position[2] + 0.5f * tc_pl0->scale[2];

        const auto collision_left = detect_collision(ball_aabb, palyer_left_aabb);
        if (collision_left != CollisionSide::eNone)
        {
            const auto hit_pos = get_z_dim_normalized_intersect(ball_pos[2], tc_pl0, tc_pl0->scale[2]);
            ret.object = player_left_go_;
            ret.hitpoint_orientation = get_hitpoint_orientation(1.0f, hit_pos);

        }
        else
        {
            auto tc_pl1 = engineSceneGetTransformComponent(scene_, player_right_go_);
            aabb2d_t palyer_right_aabb{};
            palyer_right_aabb.x_min = tc_pl1->position[0] - 0.5f * tc_pl1->scale[0];
            palyer_right_aabb.x_max = tc_pl1->position[0] + 0.5f * tc_pl1->scale[0];
            palyer_right_aabb.y_min = tc_pl1->position[2] - 0.5f * tc_pl1->scale[2];
            palyer_right_aabb.y_max = tc_pl1->position[2] + 0.5f * tc_pl1->scale[2];
            const auto collision_right = detect_collision(ball_aabb, palyer_right_aabb);
            if (collision_right != CollisionSide::eNone)
            {
                const auto hit_pos = get_z_dim_normalized_intersect(ball_pos[2], tc_pl1, tc_pl1->scale[2]);
                ret.object = player_right_go_;
                ret.hitpoint_orientation = get_hitpoint_orientation(-1.0f, hit_pos);
            }
        }
        return ret;
    }

private:
    void process_keyboard_inputs(float dt)
    {
        auto tc_pl0 = engineSceneGetTransformComponent(scene_, player_left_go_);
        auto tc_pl1 = engineSceneGetTransformComponent(scene_, player_right_go_);

        const auto pos_mod_max = 0.0065f;
        const auto pos_mod = 0.000001f;

        if (engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_W))
        {
            //tc_pl0->position[2] -= pos_mod;
            velocity_left_[2] = std::min(velocity_left_[2] - pos_mod, -pos_mod_max);
        }
        else if (engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_S))
        {
            //tc_pl0->position[2] += pos_mod;
            velocity_left_[2] = std::max(velocity_left_[2] + pos_mod, pos_mod_max);
        }
        else
        {
            velocity_left_[2] = 0.0f;
        }

        if (engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_UP))
        {
            //tc_pl0->position[2] -= pos_mod;
            velocity_right_[2] = std::min(velocity_right_[2] - pos_mod, -pos_mod_max);
        }
        else if (engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_DOWN))
        {
            //tc_pl0->position[2] += pos_mod;
            velocity_right_[2] = std::max(velocity_right_[2] + pos_mod, pos_mod_max);
        }
        else
        {
            velocity_right_[2] = 0.0f;
        }

        tc_pl0->position[0] += velocity_left_[0] * dt;
        tc_pl0->position[1] += velocity_left_[1] * dt;
        tc_pl0->position[2] += velocity_left_[2] * dt;

        tc_pl1->position[0] += velocity_right_[0] * dt;
        tc_pl1->position[1] += velocity_right_[1] * dt;
        tc_pl1->position[2] += velocity_right_[2] * dt;
    }

private:
    engine_application_t app_;
    engine_scene_t scene_;

    engine_game_object_t player_left_go_;
    engine_game_object_t player_left_score_text_go_;
    std::uint32_t player_left_score_;

    engine_game_object_t player_right_go_;
    engine_game_object_t player_right_score_text_go_;
    std::uint32_t player_right_score_;

    std::array<float, 3> velocity_left_;
    std::array<float, 3> velocity_right_;
};


class ObsctaleScript : public IScript
{
public:
    ObsctaleScript() = default;
    ObsctaleScript(engine_application_t app, engine_scene_t scene, engine_geometry_t geometry, std::array<float, 3> position, std::array<float, 3> scale)
        : scene_(scene)
        , app_(app)
        , obstacle_go_(engineSceneCreateGameObject(scene))
    {
        assert(position.size() == 3);
        assert(scale.size() == 3);

        auto mesh_comp = engineSceneAddMeshComponent(scene, obstacle_go_);
        mesh_comp->geometry = geometry;

        auto tc = engineSceneAddTransformComponent(scene, obstacle_go_);
        tc->position[0] = position[0];
        tc->position[1] = position[1];
        tc->position[2] = position[2];

        tc->scale[0] = scale[0];
        tc->scale[1] = scale[1];
        tc->scale[2] = scale[2];

        auto material_comp = engineSceneAddMaterialComponent(scene, obstacle_go_);
        set_c_array(material_comp->diffuse_color, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f });
        auto nc = engineSceneAddNameComponent(scene, obstacle_go_);
        const std::string name = fmt::format("obstacle_{}", obstacle_go_);
        std::strcpy(nc->name, name.c_str());
    }

    collision_t is_colliding(const aabb2d_t& ball_aabb) const
    {
        const auto tc = engineSceneGetTransformComponent(scene_, obstacle_go_);
        aabb2d_t obsctale_aabb{};
        obsctale_aabb.x_min = tc->position[0] - 0.5f * tc->scale[0];
        obsctale_aabb.x_max = tc->position[0] + 0.5f * tc->scale[0];
        obsctale_aabb.y_min = tc->position[2] - 0.5f * tc->scale[2];
        obsctale_aabb.y_max = tc->position[2] + 0.5f * tc->scale[2];

        collision_t ret;
        ret.object = ENGINE_INVALID_GAME_OBJECT_ID;

        const auto collision = detect_collision(ball_aabb, obsctale_aabb);
        if (collision != CollisionSide::eNone)
        {
            ret.object = obstacle_go_;
            ret.hitpoint_orientation = glm::normalize(glm::vec3(0.0f, 0.0f, tc->position[2]));
        }

        return ret;
    }

    void update(float dt) override
    {

    }

private:
    engine_application_t app_;
    engine_scene_t scene_;
    engine_game_object_t obstacle_go_;
};

class BallScript : public IScript
{
public:
    BallScript(engine_application_t app, engine_scene_t scene, engine_geometry_t geometry, const PlayersScript& ps, const ObsctaleScript& ots, const ObsctaleScript& obs)
        : scene_(scene)
        , app_(app)
        , direction_({ 0.0f, 0.0f, 0.0f })
        , speed_(0.0f)
        , players_script_view_(ps)
        , obsctale_top_view_(ots)
        , obsctale_bottom_view_(obs)
    {
        {
            ball_go_ = engineSceneCreateGameObject(scene);;
            auto mesh_comp = engineSceneAddMeshComponent(scene, ball_go_);
            mesh_comp->geometry = geometry;

            auto tc = engineSceneAddTransformComponent(scene, ball_go_);
            tc->scale[0] = 0.1f;
            tc->scale[1] = 0.1f;
            tc->scale[2] = 0.1f;

            auto material_comp = engineSceneAddMaterialComponent(scene, ball_go_);
            set_c_array(material_comp->diffuse_color, std::array<float, 4>{ 0.4f, 0.3f, 1.0f, 0.2f });

            auto nc = engineSceneAddNameComponent(scene, ball_go_);
            const std::string name = fmt::format("ball");
            std::strcpy(nc->name, name.c_str());
        }

        restart_ball_state();
    }

    void update(float dt) override
    {
        process_keyboard_inputs(dt);

        auto tc = engineSceneGetTransformComponent(scene_, ball_go_);
        aabb2d_t ball_aabb{};
        ball_aabb.x_min = tc->position[0] - 0.5f * tc->scale[0];
        ball_aabb.x_max = tc->position[0] + 0.5f * tc->scale[0];
        ball_aabb.y_min = tc->position[2] - 0.5f * tc->scale[2];
        ball_aabb.y_max = tc->position[2] + 0.5f * tc->scale[2];

        collision_t collision{};
        collision = obsctale_bottom_view_.is_colliding(ball_aabb);
        if (collision.object == ENGINE_INVALID_GAME_OBJECT_ID)
        {
            collision = obsctale_top_view_.is_colliding(ball_aabb);
            if (collision.object == ENGINE_INVALID_GAME_OBJECT_ID)
            {
                collision = players_script_view_.is_colliding(ball_aabb, to_array(tc->position));
            }
        }

        if (collision.object != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            const auto new_dir = glm::reflect(glm::make_vec3(direction_.data()), collision.hitpoint_orientation);

            direction_[0] = new_dir.x;
            direction_[1] = new_dir.y;
            direction_[2] = new_dir.z;
        }
        tc->position[0] += direction_[0] * dt * speed_;
        tc->position[1] += direction_[1] * dt * speed_;
        tc->position[2] += direction_[2] * dt * speed_;

    }

private:
    void process_keyboard_inputs(float dt)
    {
#if _DEBUG
        if (engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_SPACE))
        {

            restart_ball_state();
        }
#endif
    }

    void restart_ball_state()
    {
        auto tc = engineSceneGetTransformComponent(scene_, ball_go_);
        tc->position[0] = 0;
        tc->position[1] = 0;
        tc->position[2] = 0;
        direction_ = { 1.0f, 0.0f, 0.0f };
        speed_ = 0.005f;
    }

private:
    engine_application_t app_;
    engine_scene_t scene_;
    engine_game_object_t ball_go_;

    std::array<float, 3> direction_;
    float speed_;
    const PlayersScript& players_script_view_;
    const ObsctaleScript& obsctale_top_view_;
    const ObsctaleScript& obsctale_bottom_view_;
};

int main(int argc, char** argv)
{

	std::string assets_path = "";
	if (argc > 1)
	{
		assets_path = argv[1];
	}
	std::cout << fmt::format("Reading assets from path: {}\n", assets_path);

	engine_application_t app{};
	engine_application_create_desc_t app_cd{};
	app_cd.name = "LeanrOpengl";
	app_cd.asset_store_path = assets_path.c_str();
    app_cd.width = K_IS_ANDROID ? 0 : 800;
    app_cd.height = K_IS_ANDROID ? 0 : 600;
    app_cd.fullscreen = K_IS_ANDROID;


	auto engine_error_code = engineApplicationCreate(&app, app_cd);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineApplicationDestroy(app);
		std::cout << fmt::format("Couldnt create engine application!\n");
		return -1;
	}

	
	engine_scene_t scene{};
	engine_error_code = engineSceneCreate(&scene);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineSceneDestroy(scene);
		engineApplicationDestroy(app);
		std::cout << fmt::format("Couldnt create scene!\n");
		return -1;
	}

	const float cube_vertices[] = {
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
	engine_geometry_t cube_geometry{};
	engineApplicationAddGeometryFromMemory(app, reinterpret_cast<const engine_vertex_attribute_t*>(cube_vertices), 36, nullptr, 0, "cube", &cube_geometry);

	auto camera_go = engineSceneCreateGameObject(scene);
	{
		auto camera_comp = engineSceneAddCameraComponent(scene, camera_go);
		auto camera_transform_comp = engineSceneAddTransformComponent(scene, camera_go);
		camera_comp->enabled = true;
		camera_comp->clip_plane_near = 0.1f;
		camera_comp->clip_plane_far = 100.0f;
        //camera_comp->type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
        //camera_comp->type_union.perspective_fov = 45.0f;
        camera_comp->type = ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC;
        camera_comp->type_union.orthographics_scale = 4.0f;
		camera_transform_comp->position[0] = 0.0f;
		camera_transform_comp->position[1] = 10.0f;
		camera_transform_comp->position[2] = 2.0f;
	}
    
    engine_font_t font_handle{};
    if (engineApplicationAddFontFromFile(app, "tahoma.ttf", &font_handle) != ENGINE_RESULT_CODE_OK)
    {
        std::cout << fmt::format("Couldnt load font!\n");
    }

    std::array<ObsctaleScript, 2> obstacles;
    obstacles.at(0) = ObsctaleScript(app, scene, cube_geometry, {0.0f, 0.0f, 2.0f}, {6.0f, 0.1, 0.05f});
    obstacles.at(1) = ObsctaleScript(app, scene, cube_geometry, {0.0f, 0.0f, -2.0f}, {6.0f, 0.1, 0.05f});
    auto players = PlayersScript(app, scene, cube_geometry, font_handle);
    auto balls = BallScript(app, scene, cube_geometry, players, obstacles[0], obstacles[1]);

    struct fps_counter_t
    {
        float frames_total_time = 0.0f;
        std::uint32_t frames_count = 0;
    };
    fps_counter_t fps_counter{};

	while (true)
	{
		const auto frame_begin = engineApplicationFrameBegine(app);

        if (frame_begin.events & ENGINE_EVENT_QUIT)
        {
            std::cout << fmt::format("Engine requested app quit. Exiting.\n");
            break;
        }

		if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_ESCAPE))
		{
			std::cout << fmt::format("User pressed ESCAPE key. Exiting.\n");
			break;
		}

		if (engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT))
		{
			const auto mouse_coords = engineApplicationGetMouseCoords(app);
			std::cout << fmt::format("User pressed LEFT mouse button."
				"Mouse position [x,y]: {},{}\n", mouse_coords.x, mouse_coords.y);
		}

        const float dt = frame_begin.delta_time;
        //const float dt = 16.0f;
        fps_counter.frames_count += 1;
        fps_counter.frames_total_time += dt;

        //if (fixed_update_counter >= fixed_update_interval)
        {
            //printf("%f  --- %f\n", fixed_update_counter, dt);
            players.update(dt);
            balls.update(dt);
            for (auto& o : obstacles)
            {
                o.update(dt);
            }
            //fixed_update_counter = 0.0f;
        }

        if (fps_counter.frames_total_time > 1000.0f)
        {
            std::cout << fmt::format("FPS: {}, latency: {} ms. \n", fps_counter.frames_count, fps_counter.frames_total_time / fps_counter.frames_count);
            fps_counter = {};
        }

		engine_error_code = engineApplicationFrameRunScene(app, scene, frame_begin.delta_time);
		if (engine_error_code != ENGINE_RESULT_CODE_OK)
		{
			std::cout << fmt::format("Scene update failed. Exiting.\n");
			break;
		}
		const auto frame_end = engineApplicationFrameEnd(app);
		if (!frame_end.success)
		{
			std::cout << fmt::format("Frame not finished sucesfully. Exiting.\n");

			break;
		}
	}

	engineSceneDestroy(scene);
	engineApplicationDestroy(app);

	return 0;
}