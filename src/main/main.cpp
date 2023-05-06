#include <engine.h>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <span>
#include <cassert>
#include <memory>
#include <unordered_map>

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

constexpr const float K_WALL_Y_OFFSET = 8.0f;
constexpr const bool K_IS_GOAL_NET_DISABLE_RENDER = true;

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

inline void log(std::string_view str)
{
    engineLog(str.data());
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
					tc.position[0] = new_position[0] + cc.target[0];
					tc.position[1] = new_position[1] + cc.target[1];
					tc.position[2] = new_position[2] + cc.target[2];
                    engineSceneUpdateTransformComponent(scene, entt, &tc);
				}


			}

			if (prev_mouse_coords_.x == -1)
			{
				sc_ = get_spherical_coordinates(tc.position);
			}
			prev_mouse_coords_ = engineApplicationGetMouseCoords(app);
		}
	}

private:
	std::array<float, 3> sc_;

	engine_mouse_coords_t prev_mouse_coords_{ -1, -1 };
};


class IScript
{
public:
    struct contact_point_t
    {
        float point[3];
        std::int32_t lifetime;
    };
    struct collision_t
    {
        engine_game_object_t other;
        std::vector<contact_point_t> contact_points;
    };

public:
    IScript(engine_application_t& app, engine_scene_t& scene) 
        : app_(app)
        , scene_(scene)
        , go_(engineSceneCreateGameObject(scene))
    {}
    virtual ~IScript() = default;

    virtual void update(float dt) {}
    virtual void on_collision(const collision_t& info) {}

    virtual engine_game_object_t get_game_object() const { return go_; }

protected:
    engine_application_t& app_;
    engine_scene_t& scene_;
    const engine_game_object_t go_;
};

class BallScript : public IScript
{
public:
    BallScript(engine_application_t& app, engine_scene_t& scene)
        : IScript(app, scene)
    {
        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app_, "sphere");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Couldnt find geometry for ball script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.5f;
        tc.scale[2] = 0.5f;
        engineSceneUpdateTransformComponent(scene_, go_, &tc);

        auto rb = engineSceneAddRigidBodyComponent(scene, go_);
        rb.mass = 1.0f;
        engineSceneUpdateRigidBodyComponent(scene, go_, &rb);
        auto bc = engineSceneAddColliderComponent(scene, go_);
        bc.type = ENGINE_COLLIDER_TYPE_SPHERE;
        bc.collider.sphere.radius = 1.0f;
        //bc.type = ENGINE_COLLIDER_TYPE_BOX;
        bc.friction_static = 0.0f;
        bc.bounciness = 1.0f;
        engineSceneUpdateColliderComponent(scene, go_, &bc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.4f, 0.3f, 1.0f, 0.2f });
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

        auto nc = engineSceneAddNameComponent(scene, go_);
        const std::string name = fmt::format("ball");
        std::strcpy(nc.name, name.c_str());
        engineSceneUpdateNameComponent(scene, go_, &nc);

        reset_state();
    }

    void reset_state()
    {
        ball_speed_x_ = 20.0f;
        ball_speed_y_ = 20.0f;

        auto tc = engineSceneGetTransformComponent(scene_, go_);
        tc.position[0] = 0.0f;
        tc.position[1] = 0.0f;
        tc.position[2] = 0.0f;
        engineSceneUpdateTransformComponent(scene_, go_, &tc);

        update_linear_velocity(1.0f, 0.0f);
    }

    void update_linear_velocity(float dir_x, float dir_y)
    {
        auto rb = engineSceneGetRigidBodyComponent(scene_, go_);
        rb.linear_velocity[0] = dir_x * ball_speed_x_;
        rb.linear_velocity[1] = dir_y * ball_speed_y_;
        engineSceneUpdateRigidBodyComponent(scene_, go_, &rb);
    }
    void update_linear_velocity(std::span<const float> dir)
    {
        assert(dir.size() == 2);
        update_linear_velocity(dir[0], dir[1]);
    }

    std::array<float, 2> get_direction_vector() const
    {
        auto rb = engineSceneGetRigidBodyComponent(scene_, go_);
        return {
            rb.linear_velocity[0] / ball_speed_x_,
            rb.linear_velocity[1] / ball_speed_y_,
        };
    }

    void update(float dt) override
    {
        handle_input(dt);

        timer_accu += dt;

        // update speed every 2 seconds
        if(timer_accu >= 2000.0f)
        {
            ball_speed_x_ += 2.0f;
            ball_speed_y_ += 2.0f;
            timer_accu = 0.0f;
            update_linear_velocity(get_direction_vector());
        }
    }

private:
    void handle_input(float dt)
    {
        auto tc = engineSceneGetTransformComponent(scene_, go_);

        const engine_finger_info_t* finger_infos = nullptr;
        std::size_t fingers_info_count = 0;
        const auto has_finger_info = engineApplicationGetFingerInfo(app_, &finger_infos, &fingers_info_count);

        bool should_reset_state = false;

        if constexpr (K_IS_ANDROID)
        {
            if(has_finger_info)
            {
                for(std::size_t i = 0; i < fingers_info_count; i++)
                {
                    const auto f = finger_infos[i];
                    if(f.x > 0.35f && f.x < 0.65 && f.y < 0.2f)
                    {
                        should_reset_state = true;
                    }
                }
            }
        }
        else
        {
            if (engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_SPACE))
            {
                should_reset_state = true;
            }
        }

        if(should_reset_state)
        {
            reset_state();
        }
    }

private:
    float ball_speed_x_ = 0.0f;
    float ball_speed_y_ = 0.0f;
    float timer_accu = 0.0f;
};

class PlayerPaddleScript : public IScript
{
public:
    BallScript* ball_script_ = nullptr;

public:
    PlayerPaddleScript(engine_application_t& app, engine_scene_t& scene, float init_pos_x, float score_init_pos_x, const char* name)
        : IScript(app, scene)
        , score_(0)
        , score_str_(std::to_string(score_))
    {
        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app_, "cube");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Couldnt find geometry for player paddle script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = init_pos_x;
        tc.position[1] = 0.0f;
        tc.position[2] = 0.0f;
        
        tc.scale[0] = 0.5f;
        tc.scale[1] = 2.5f;
        tc.scale[2] = 1.0f;
        engineSceneUpdateTransformComponent(scene_, go_, &tc);

        auto bc = engineSceneAddColliderComponent(scene, go_);
        bc.type = ENGINE_COLLIDER_TYPE_BOX;
        bc.bounciness = 1.0f;
        bc.friction_static = 0.0f;
        engineSceneUpdateColliderComponent(scene, go_, &bc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.4f, 0.3f, 1.0f, 0.2f });
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

        auto nc = engineSceneAddNameComponent(scene, go_);
        std::strcpy(nc.name, name);
        engineSceneUpdateNameComponent(scene, go_, &nc);

        // text component for the SCORE
        {
            score_go_ = engineSceneCreateGameObject(scene);
            auto text_component = engineSceneAddTextComponent(scene, score_go_);
            text_component.font_handle = engineApplicationGetFontByName(app_, "tahoma_font");
            assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
            text_component.text = score_str_.c_str();
            set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
            engineSceneUpdateTextComponent(scene, score_go_, &text_component);

            auto tc = engineSceneAddRectTransformComponent(scene, score_go_);
            tc.position[0] = score_init_pos_x;
            tc.position[1] = 0.85f;

            tc.scale[0] = 1.5f;
            tc.scale[1] = 1.5f;
            engineSceneUpdateRectTransformComponent(scene, score_go_, &tc);
        }
    }

    void on_collision(const collision_t& info) override
    {
        assert(ball_script_);

        if (info.other == ball_script_->get_game_object())
        {            
            const auto paddle_tc = engineSceneGetTransformComponent(scene_, go_);
            const auto paddle_current_y = paddle_tc.position[1];

            auto ball_tc = engineSceneGetTransformComponent(scene_, info.other);
            const auto ball_current_y = ball_tc.position[1];

            const auto interct_pos = -1.0f * ((paddle_current_y - ball_current_y)) / 2.5f;
            //std::cout << interct_pos << std::endl;
            engineLog(fmt::format("{} \n", interct_pos).c_str());
            auto ball_dir = ball_script_->get_direction_vector();
            ball_dir[1] = interct_pos;
            const auto ball_dir_normalized = glm::normalize(glm::make_vec2(ball_dir.data()));
            ball_script_->update_linear_velocity(ball_dir_normalized[0], ball_dir_normalized[1]);
            //auto rb = engineSceneGetRigidBodyComponent(scene_, info.other);
            //rb->linear_velocity[0] = 0.0f;
            //rb.linear_velocity[1] = 10.0f * interct_pos;
            //engineSceneUpdateRigidBodyComponent(scene_, info.other, &rb);
        }
    }

    void update(float dt) override
    {
        handle_input(dt);
        score_str_ = std::to_string(score_);
    }

    virtual void set_score(std::size_t new_score)
    {
        score_ = new_score;
    }

    virtual std::size_t get_score() const
    {
        return score_;
    }

protected:
    void handle_input(float dt)
    {
        auto tc = engineSceneGetTransformComponent(scene_, go_);
        const engine_finger_info_t* finger_infos = nullptr;
        std::size_t fingers_info_count = 0;
        const auto has_finger_info = engineApplicationGetFingerInfo(app_, &finger_infos, &fingers_info_count);

        bool update_component = false;
        if constexpr (K_IS_ANDROID)
        {
            if(has_finger_info)
            {
                for(std::size_t i = 0; i < fingers_info_count; i++)
                {
                    const auto f = finger_infos[i];
                    if((f.event_type_flags & ENGINE_FINGER_DOWN || f.event_type_flags & ENGINE_FINGER_MOTION)
                        && is_finger_in_controller_area_impl(f))
                    {
                        const auto y_delta = -1.0f * ((f.y * 2.0f) - 1.0f);
                        tc.position[1] = y_delta * 8.0f;
                        update_component = true;
                    }
                }
            }
        }
        else
        {
            // KEYBOARD
            if(engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_W))
            {
                tc.position[1] += 0.05f * dt;
                update_component = true;
            }
            if(engineApplicationIsKeyboardButtonDown(app_, ENGINE_KEYBOARD_KEY_S))
            {
                tc.position[1] -= 0.05f * dt;
                update_component = true;
            }
        }
        if (update_component)
        {
            engineSceneUpdateTransformComponent(scene_, go_, &tc);
        }
    }

    virtual bool is_finger_in_controller_area_impl(const engine_finger_info_t& f) = 0;

protected:
    engine_game_object_t score_go_;
    std::size_t score_ = 0;
    std::string score_str_ = "";
};

class RightPlayerPaddleScript : public PlayerPaddleScript
{
public:
    RightPlayerPaddleScript(engine_application_t& app, engine_scene_t& scene)
        : PlayerPaddleScript(app, scene, 12.0f, 0.75f, "right_player")
    {
        // text component
        {
            const auto text_go = engineSceneCreateGameObject(scene);
            auto text_component = engineSceneAddTextComponent(scene, text_go);
            text_component.font_handle = engineApplicationGetFontByName(app_, "tahoma_font");
            assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
            text_component.text = "Player 2";
            set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
            engineSceneUpdateTextComponent(scene, text_go, &text_component);

            auto tc = engineSceneAddRectTransformComponent(scene, text_go);
            tc.position[0] = 0.75f;
            tc.position[1] = 0.15f;

            tc.scale[0] = 0.5f;
            tc.scale[1] = 0.5f;
            engineSceneUpdateRectTransformComponent(scene, text_go, &tc);
        }

        // touchable area component
        {
            const auto touch_area_go = engineSceneCreateGameObject(scene);
            auto tc = engineSceneAddRectTransformComponent(scene, touch_area_go);
            tc.position[0] = 0.8f;
            tc.position[1] = 0.0f;

            tc.scale[0] = 1.0f;
            tc.scale[1] = 1.0f;
            engineSceneUpdateRectTransformComponent(scene, touch_area_go, &tc);

            auto ic = engineSceneAddImageComponent(scene, touch_area_go);
            set_c_array(ic.color, std::array<float, 4>{0.0f, 0.3f, 0.8f, 0.0f});
            engineSceneUpdateImageComponent(scene, touch_area_go, &ic);
        }
    }


protected:
    bool is_finger_in_controller_area_impl(const engine_finger_info_t& f) override
    {
        return (f.x > 0.8f && f.x <= 1.0f);
    }

};

class LeftPlayerPaddleScript : public PlayerPaddleScript
{
public:
    LeftPlayerPaddleScript(engine_application_t& app, engine_scene_t& scene)
        : PlayerPaddleScript(app, scene, -12.0f, 0.25f, "left_player")
    {
        // text component for the NAME
        {
            const auto text_go = engineSceneCreateGameObject(scene);
            auto text_component = engineSceneAddTextComponent(scene, text_go);
            text_component.font_handle = engineApplicationGetFontByName(app_, "tahoma_font");
            assert(text_component.font_handle != ENGINE_INVALID_OBJECT_HANDLE && "Cant find font for player name text render");
            text_component.text = "Player 1";
            set_c_array(text_component.color, std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f});
            engineSceneUpdateTextComponent(scene, text_go, &text_component);

            auto tc = engineSceneAddRectTransformComponent(scene, text_go);
            tc.position[0] = 0.25f;
            tc.position[1] = 0.15f;

            tc.scale[0] = 0.5f;
            tc.scale[1] = 0.5f;
            engineSceneUpdateRectTransformComponent(scene, text_go, &tc);
        }

        // touchable area component
        {
            const auto touch_area_go = engineSceneCreateGameObject(scene);
            auto tc = engineSceneAddRectTransformComponent(scene, touch_area_go);
            tc.position[0] = 0.0f;
            tc.position[1] = 0.0f;

            tc.scale[0] = 0.2f;
            tc.scale[1] = 1.0f;
            engineSceneUpdateRectTransformComponent(scene, touch_area_go, &tc);

            auto ic = engineSceneAddImageComponent(scene, touch_area_go);
            set_c_array(ic.color, std::array<float, 4>{0.0f, 0.3f, 0.8f, 0.0f});
            engineSceneUpdateImageComponent(scene, touch_area_go, &ic);
        }
    }

protected:
    bool is_finger_in_controller_area_impl(const engine_finger_info_t& f) override
    {
        return (f.x < 0.2f && f.x >= 0.0f);
    }
};

class GoalNetScript : public IScript
{
public:
    BallScript* ball_script_ = nullptr;
    PlayerPaddleScript* player_paddel_script_ = nullptr;

public:
    GoalNetScript(engine_application_t& app, engine_scene_t& scene, float init_pos_x, const char* name)
        : IScript(app, scene)
    {
        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app_, "cube");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Couldnt find geometry for player goal net script!");
        mesh_comp.disable = K_IS_GOAL_NET_DISABLE_RENDER;
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = init_pos_x;
        tc.position[1] = 0.0f;
        tc.position[2] = 0.0f;

        tc.scale[0] = 0.5f;
        tc.scale[1] = 25.0f;
        tc.scale[2] = 1.0f;
        engineSceneUpdateTransformComponent(scene_, go_, &tc);

        auto bc = engineSceneAddColliderComponent(scene, go_);
        bc.type = ENGINE_COLLIDER_TYPE_BOX;
        bc.is_trigger = true;
        engineSceneUpdateColliderComponent(scene, go_, &bc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 0.2f });
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

        auto nc = engineSceneAddNameComponent(scene, go_);
        std::strcpy(nc.name, name);
        engineSceneUpdateNameComponent(scene, go_, &nc);
    }

    void update(float dt) override
    {
        if (score_fence_.was_score)
        {
            score_fence_.frame_counter++;
        }
        if (score_fence_.frame_counter == score_collision_fence::COUNTER_LIMIT)
        {
            score_fence_.frame_counter = 0;
            score_fence_.was_score = false;
        }
    }

    void on_collision(const collision_t& info) override
    {
        assert(ball_script_ != nullptr);
        assert(player_paddel_script_ != nullptr);

        if (info.other == ball_script_->get_game_object() && score_fence_.frame_counter == 0)
        {
            ball_script_->reset_state();
            player_paddel_script_->set_score(player_paddel_script_->get_score() + 1);
            score_fence_.was_score = true;
        }

    }

protected:
    struct score_collision_fence
    {
        bool was_score = false;
        std::int32_t frame_counter = 0;
        constexpr static const std::int32_t COUNTER_LIMIT = 10;
    };

protected:
    std::uint32_t score_;
    score_collision_fence score_fence_;

};

class LeftGoalNetScript : public GoalNetScript
{
public:
    LeftGoalNetScript(engine_application_t& app, engine_scene_t& scene)
        : GoalNetScript(app, scene, -14.0f, "left_goal_net")
    {
    }  
};

class RightGoalNetScript : public GoalNetScript
{
public:
    RightGoalNetScript(engine_application_t& app, engine_scene_t& scene)
        : GoalNetScript(app, scene, 14.0f, "right_goal_net")
    {
    }
};

class WallScript : public IScript
{
public:
    WallScript(engine_application_t& app, engine_scene_t& scene, float init_pos_y, const char* name)
        : IScript(app, scene)
    {
        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app_, "cube");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Couldnt find geometry for player goal net script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = 0.0f;
        tc.position[1] = init_pos_y;
        tc.position[2] = 0.0f;

        tc.scale[0] = 12.0f;
        tc.scale[1] = 0.1f;
        tc.scale[2] = 1.0f;
        engineSceneUpdateTransformComponent(scene_, go_, &tc);

        auto bc = engineSceneAddColliderComponent(scene, go_);
        bc.type = ENGINE_COLLIDER_TYPE_BOX;
        bc.bounciness = 1.0f;
        bc.friction_static = 0.0f;
        engineSceneUpdateColliderComponent(scene, go_, &bc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 0.2f });
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

        auto nc = engineSceneAddNameComponent(scene, go_);
        std::strcpy(nc.name, name);
        engineSceneUpdateNameComponent(scene, go_, &nc);
    }
};

class WallTopScript : public WallScript
{
public:
    WallTopScript(engine_application_t& app, engine_scene_t& scene)
        : WallScript(app, scene, K_WALL_Y_OFFSET, "top_wall")
    {

    }
};

class BottomTopScript : public WallScript
{
public:
    BottomTopScript(engine_application_t& app, engine_scene_t& scene)
        : WallScript(app, scene, -1.0f * K_WALL_Y_OFFSET, "bottom_wall")
    {

    }
};

int main(int argc, char** argv)
{

	std::string assets_path = "";
	if (argc > 1)
	{
		assets_path = argv[1];
	}
    log(fmt::format("Reading assets from path: {}\n", assets_path));

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
        log(fmt::format("Couldnt create engine application!\n"));
		return -1;
	}

	
	engine_scene_t scene{};
	engine_error_code = engineSceneCreate(&scene);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineSceneDestroy(scene);
		engineApplicationDestroy(app);
        log(fmt::format("Couldnt create scene!\n"));
		return -1;
	}
    const float gravity[3] = { 0.0f, 0.0f, 0.0f };
    engineSceneSetGravityVector(scene, gravity);

    engine_model_info_t model_info{};
    auto model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "cube.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading CUBE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t cube_geometry{};
    engineApplicationAddGeometryFromMemory(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "cube", &cube_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);

    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "sphere_uv.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading SPHERE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t sphere_geometry{};
    engineApplicationAddGeometryFromMemory(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "sphere", &sphere_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);

	auto camera_go = engineSceneCreateGameObject(scene);
	{
		auto camera_comp = engineSceneAddCameraComponent(scene, camera_go);
		camera_comp.enabled = true;
		camera_comp.clip_plane_near = 0.1f;
		camera_comp.clip_plane_far = 100.0f;
        //camera_comp->type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
        //camera_comp->type_union.perspective_fov = 45.0f;
        camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC;
        camera_comp.type_union.orthographics_scale = 16.0f;
		//camera_transform_comp->position[0] = 0.0f;
		//camera_transform_comp->position[1] = 10.0f;
		//camera_transform_comp->position[1] = 3.0f;
        engineSceneUpdateCameraComponent(scene, camera_go, &camera_comp);

        auto camera_transform_comp = engineSceneAddTransformComponent(scene, camera_go);
        camera_transform_comp.position[2] = 15.0f;
        engineSceneUpdateTransformComponent(scene, camera_go, &camera_transform_comp);
	}
    
    engine_font_t font_handle{};
    if (engineApplicationAddFontFromFile(app, "tahoma.ttf", "tahoma_font", &font_handle) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt load font!\n"));
        return -1;
    }

    struct fps_counter_t
    {
        float frames_total_time = 0.0f;
        std::uint32_t frames_count = 0;
    };
    fps_counter_t fps_counter{};


    BallScript ball_script(app, scene);
    LeftPlayerPaddleScript right_player_script(app, scene);
    RightPlayerPaddleScript left_player_script(app, scene);

    right_player_script.ball_script_ = &ball_script;
    left_player_script.ball_script_ = &ball_script;

    LeftGoalNetScript left_goal_net_script(app, scene);
    RightGoalNetScript right_goal_net_script(app, scene);
    left_goal_net_script.ball_script_ = &ball_script;
    left_goal_net_script.player_paddel_script_ = &left_player_script;
    right_goal_net_script.ball_script_ = &ball_script;
    right_goal_net_script.player_paddel_script_ = &right_player_script;

    WallTopScript top_wall(app, scene);
    BottomTopScript bottom_wall(app, scene);

    std::unordered_map<engine_game_object_t, IScript*> scene_manager;
    scene_manager.reserve(1024);
    scene_manager[ball_script.get_game_object()] = &ball_script;
    scene_manager[right_player_script.get_game_object()] = &right_player_script;
    scene_manager[left_player_script.get_game_object()] = &left_player_script;
    scene_manager[left_goal_net_script.get_game_object()] = &left_goal_net_script;
    scene_manager[right_goal_net_script.get_game_object()] = &right_goal_net_script;
    scene_manager[top_wall.get_game_object()] = &top_wall;
    scene_manager[bottom_wall.get_game_object()] = &bottom_wall;


    engine_component_view_t rect_tranform_view{};
    engineSceneCreateComponentView(scene, &rect_tranform_view);
    engineSceneComponentViewAttachRectTransformComponent(scene, rect_tranform_view);

    engine_component_iterator_t begin_it{};
    engineComponentViewCreateBeginComponentIterator(rect_tranform_view, &begin_it);
    engine_component_iterator_t end_it{};
    engineComponentViewCreateEndComponentIterator(rect_tranform_view, &end_it);

    size_t idx = 0;
    while (engineComponentIteratorCheckEqual(begin_it, end_it) == false)
    {
        std::cout << idx++ << std::endl;
        const auto game_obj = engineComponentIteratorGetGameObject(begin_it);
        const auto rect_transform = engineSceneGetRectTransformComponent(scene, game_obj);
        std::cout << rect_transform.position[0] << ", " << rect_transform.position[1] << ", " << rect_transform.position[2] << std::endl;
        engineComponentIteratorNext(begin_it);
    }
	while (true)
	{
		const auto frame_begin = engineApplicationFrameBegine(app);

        if (frame_begin.events & ENGINE_EVENT_QUIT)
        {
            log(fmt::format("Engine requested app quit. Exiting.\n"));
            break;
        }

		if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_ESCAPE))
		{
			log(fmt::format("User pressed ESCAPE key. Exiting.\n"));
			break;
		}

		if (engineApplicationIsMouseButtonDown(app, ENGINE_MOUSE_BUTTON_LEFT))
		{
			const auto mouse_coords = engineApplicationGetMouseCoords(app);
			log(fmt::format("User pressed LEFT mouse button."
				"Mouse position [x,y]: {},{}\n", mouse_coords.x, mouse_coords.y));
		}

        const float dt = frame_begin.delta_time;
        //const float dt = 50.0f;
        fps_counter.frames_count += 1;
        fps_counter.frames_total_time += dt;



        if (fps_counter.frames_total_time > 1000.0f)
        {
            log(fmt::format("FPS: {}, latency: {} ms. \n", 
                fps_counter.frames_count, fps_counter.frames_total_time / fps_counter.frames_count));
            fps_counter = {};

            const auto log_score = fmt::format("Left: {}, Right: {}\n", left_player_script.get_score(), right_player_script.get_score());
            engineLog(log_score.c_str());
        }

		engine_error_code = engineApplicationFrameSceneUpdatePhysics(app, scene, frame_begin.delta_time);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            log(fmt::format("Scene physcis update failed. Exiting.\n"));
            break;
        }



        std::size_t num_collisions = 0;
        const engine_collision_info_t* collisions_list = nullptr;
        engineSceneGetCollisions(scene, &num_collisions, &collisions_list);
        for(std::size_t i = 0; i < num_collisions; i++)
        {
            const auto& col = collisions_list[i];
            IScript::collision_t collision{};
            collision.contact_points.resize(col.contact_points_count);
            for (std::size_t j = 0; j < col.contact_points_count; j++)
            {
                collision.contact_points[j].lifetime = col.contact_points[j].lifetime;
                collision.contact_points[j].point[0] = col.contact_points[j].point_object_a[0];
                collision.contact_points[j].point[1] = col.contact_points[j].point_object_a[1];
                collision.contact_points[j].point[2] = col.contact_points[j].point_object_a[2];
            }

            collision.other = col.object_b;
            scene_manager[col.object_a]->on_collision(collision);

            collision.other = col.object_a;
            scene_manager[col.object_b]->on_collision(collision);

            //log(fmt::format("Num collisions: {}, num cp: {} \n", num_collisions, collisions_list[0].contact_points_count));
            //log(fmt::format("Pt_a: {}, {}, {}\n", collisions_list[0].contact_points->point_object_a[0], collisions_list[0].contact_points->point_object_a[1], collisions_list[0].contact_points->point_object_a[2]));
            //log(fmt::format("Pt_b: {}, {}, {}\n", collisions_list[0].contact_points->point_object_b[0], collisions_list[0].contact_points->point_object_b[1], collisions_list[0].contact_points->point_object_b[2]));
        }

        for (auto& [go, script] : scene_manager)
        {
            script->update(dt);
        }

		engine_error_code = engineApplicationFrameSceneUpdateGraphics(app, scene, dt);
		if (engine_error_code != ENGINE_RESULT_CODE_OK)
		{
			log(fmt::format("Scene update failed. Exiting.\n"));
			break;
		}
		const auto frame_end = engineApplicationFrameEnd(app);
		if (!frame_end.success)
		{
			log(fmt::format("Frame not finished sucesfully. Exiting.\n"));
			break;
		}
	}

	engineSceneDestroy(scene);
	engineApplicationDestroy(app);

	return 0;
}