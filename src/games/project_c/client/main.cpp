#include <engine.h>

#include "scene_manager.h"
#include "iscene.h"
#include "utils.h"
#include "iscript.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <SDL_system.h>
#include <SDL_main.h>

#include <fmt/format.h>

#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <span>
#include <cassert>
#include <memory>
#include <unordered_map>
#include <deque>
#include <functional>
#define ASIO_NO_DEPRECATED 1
#include <asio.hpp>

namespace project_c
{

class FloorTile : public engine::IScript
{
public:
    FloorTile(engine::IScene* my_scene, std::int32_t x, std::int32_t y, std::int32_t z)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for floor script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.1f;
        tc.scale[2] = 0.5f;

        tc.position[0] = static_cast<float>(x);
        tc.position[1] = static_cast<float>(y) - (tc.scale[1] / 2.0f);
        tc.position[2] = static_cast<float>(z);
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.58f, 0.259f, 0.325f, 0.0f });
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    }

    void set_material(std::array<float, 4> color)
    {
        auto scene = my_scene_->get_handle();
        auto material_comp = engineSceneGetMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, color);
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);
    }
};

class ObstacleStaticObject : public engine::IScript
{
public:
    ObstacleStaticObject(engine::IScene* my_scene, std::int32_t x, std::int32_t y, std::int32_t z)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for floor script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.75f;
        tc.scale[2] = 0.5f;

        tc.position[0] = static_cast<float>(x);
        tc.position[1] = 0.5f + static_cast<float>(y);
        tc.position[2] = static_cast<float>(z);
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f });
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    }

    void set_material(std::array<float, 4> color)
    {
        auto scene = my_scene_->get_handle();
        auto material_comp = engineSceneGetMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, color);
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);
    }
};


enum class TileID : std::uint32_t
{
    eUnknown = 0,
    eGrass = 1,
    ePath = 2
};

enum ObstacleID : std::uint32_t
{
    eUnknown = 0,
    eBox = 1
};

class Tile
{
public:
    Tile(TileID id = TileID::eUnknown, ObstacleID obsctale = ObstacleID::eUnknown)
        : tile_id_(id), obstacle_id_(obsctale)
    {

    }

    TileID get_tile_id() const { return tile_id_; }

    void set_tile_id(TileID id)
    {
        tile_id_ = id;
    }

    ObstacleID get_obstacle_id() const { return obstacle_id_; }
    void set_obstacle_id(ObstacleID id)
    {
        obstacle_id_ = id;
    }

    bool can_walk_on() const
    {
        return obstacle_id_ == ObstacleID::eUnknown;
    }

private:
    TileID tile_id_;
    ObstacleID obstacle_id_;
};

template<std::uint32_t WIDTH, std::uint32_t HEIGHT>
class GameMapT
{
public:
    GameMapT(engine::IScene* parent_scene)
        : parent_scene_(parent_scene)
    {

        for (auto i = 0; i < HEIGHT; i++)
        {
            for (auto j = 0; j < WIDTH; j++)
            {
                auto& tile = map_[i][j];
                tile = Tile(TileID::eGrass);
                if (j == WIDTH / 2)
                {
                    tile.set_tile_id(TileID::ePath);
                }
            }
        }
        map_[HEIGHT / 2 - 1][WIDTH / 2 + 1].set_obstacle_id(ObstacleID::eBox);
        map_[HEIGHT / 2 - 1][WIDTH / 2 + 2].set_obstacle_id(ObstacleID::eBox);
        map_[HEIGHT / 2 - 1][WIDTH / 2 + 3].set_obstacle_id(ObstacleID::eBox);

        for (auto i = 0; i < HEIGHT; i++)
        {
            for (auto j = 0; j < WIDTH; j++)
            {
                auto& tile = map_[i][j];
                auto tile_id = tile.get_tile_id();
                auto obstacle_id = tile.get_obstacle_id();

                const auto coord_x = j - static_cast<std::int32_t>(WIDTH / 2);
                const auto coord_z = i - static_cast<std::int32_t>(HEIGHT / 2);

                switch (tile_id)
                {
                case TileID::eGrass:
                {
                    parent_scene->register_script<FloorTile>(coord_x, 0, coord_z);
                    break;
                }
                case TileID::ePath:
                {
                    auto* t = parent_scene->register_script<FloorTile>(coord_x, 0, coord_z);
                    t->set_material(std::array<float, 4>{0.8f, 0.8f, 0.1f, 0.0f});
                    break;
                }
                default:
                    assert(!"Unknown tileID!");
                }

                switch (obstacle_id)
                {
                case ObstacleID::eBox:
                {
                    parent_scene->register_script<ObstacleStaticObject>(coord_x, 0, coord_z);
                    break;
                }
                }
            }
        }
    }

    bool player_wants_to_move(std::int32_t x, std::int32_t y) const
    {
        // return check index!!
        return map_[HEIGHT / 2 + y][WIDTH / 2 + x].can_walk_on();
    }

private:
    std::array<std::array<Tile, WIDTH>, HEIGHT> map_;
    engine::IScene* parent_scene_ = nullptr;
};
using GameMap = GameMapT<20, 20>;



class PlayerScript : public  engine::IScript
{
public:
    PlayerScript(engine::IScene* my_scene, const GameMap& map)
        : IScript(my_scene)
        , map_(map)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        //mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
        //mesh_comp.geometry = engineApplicationGetGeometryByName(app, "table");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for cat script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = 0.0f;
        tc.position[1] = 0.5f;
        tc.position[2] = 0.0f;

        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.5f;
        tc.scale[2] = 0.5f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 0.0f });
        material_comp.diffuse_texture = 0;
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

    }

    void update(float dt) override
    {

        auto app = my_scene_->get_app_handle();
        auto scene = my_scene_->get_handle();
        //ToDo rotate object around the mouse  
        // helper unity forum post: https://discussions.unity.com/t/make-a-player-model-rotate-towards-mouse-location/125354/2 
        const auto mose_pos = engineApplicationGetMouseCoords(app);
     
        // move
        move(dt);
    }

private:
    void move(const float dt)
    {
        //ToDo: going in diagnol directions is 2x faster than going single direction at one time
        next_move_counter_ += dt;
        auto app = my_scene_->get_app_handle();
        auto scene = my_scene_->get_handle();

        const auto move_right = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_D);
        const auto move_left = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_A);
        const auto move_top = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_W);
        const auto move_bottom = engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_S);

        const bool move_requested = move_right || move_left || move_top || move_bottom;
        if (!move_requested || next_move_counter_ <= next_move_limit_time_)
        {
            return;
        }
        next_move_counter_ = 0.0f;

        const auto move_speed_factor = 1.0f;
        auto tc = engineSceneGetTransformComponent(scene, go_);

        auto& [coord_x, coord_y] = position_;

        if (move_right && map_.player_wants_to_move(coord_x + 1, coord_y))
        {
            tc.position[0] += move_speed_factor;
            coord_x += 1;
        }
        if (move_left && map_.player_wants_to_move(coord_x - 1, coord_y))
        {
            tc.position[0] -= move_speed_factor;
            coord_x -= 1;
        }

        if (move_top && map_.player_wants_to_move(coord_x, coord_y - 1))
        {
            tc.position[2] -= move_speed_factor;
            coord_y -= 1;
        }
        if (move_bottom && map_.player_wants_to_move(coord_x, coord_y + 1))
        {
            tc.position[2] += move_speed_factor;
            coord_y += 1;
        }

        engineSceneUpdateTransformComponent(scene, go_, &tc);
    }

private:
    float next_move_counter_ = 0.0f;
    const float next_move_limit_time_ = 500.0f;  // in miliseconds

    std::pair<std::int32_t, std::int32_t> position_ = { 0, 0 };
    const GameMap& map_;
};

class CameraScript : public engine::IScript
{
public:
    PlayerScript* player_script = nullptr;

public:
    CameraScript(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto camera_comp = engineSceneAddCameraComponent(scene, go_);
        camera_comp.enabled = true;
        camera_comp.clip_plane_near = 0.1f;
        camera_comp.clip_plane_far = 100.0f;
        camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
        camera_comp.type_union.perspective_fov = 45.0f;
        engineSceneUpdateCameraComponent(scene, go_, &camera_comp);

        auto camera_transform_comp = engineSceneAddTransformComponent(scene, go_);
        camera_transform_comp.position[1] = 11.0f;
        camera_transform_comp.position[2] = z_base_offset;
        engineSceneUpdateTransformComponent(scene, go_, &camera_transform_comp);
    }

    void update(float dt) override
    {
        debug_zoom_in_out(dt);
        set_target_to_perfect_follow_player_position();
    }

private:
    void set_target_to_perfect_follow_player_position()
    {
        auto app = my_scene_->get_app_handle();
        auto scene = my_scene_->get_handle();

        assert(player_script);
        const auto player_tc = engineSceneGetTransformComponent(scene, player_script->get_game_object());

        auto camera_comp = engineSceneGetCameraComponent(scene, go_);
        camera_comp.target[0] = player_tc.position[0];
        camera_comp.target[1] = player_tc.position[1];
        camera_comp.target[2] = player_tc.position[2];
        engineSceneUpdateCameraComponent(scene, go_, &camera_comp);

        auto tc = engineSceneGetTransformComponent(scene, go_);
        tc.position[0] = player_tc.position[0];  // x
        tc.position[2] = player_tc.position[2] + z_base_offset;  // z
        engineSceneUpdateTransformComponent(scene, go_, &tc);
    }

    void debug_zoom_in_out(float dt)
    {
        auto app = my_scene_->get_app_handle();
        auto scene = my_scene_->get_handle();


        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_N))
        {
            auto camera_transform_comp = engineSceneGetTransformComponent(scene, go_);
            camera_transform_comp.position[1] += 0.005f * dt;
            engineSceneUpdateTransformComponent(scene, go_, &camera_transform_comp);
        }

        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_M))
        {
            auto camera_transform_comp = engineSceneGetTransformComponent(scene, go_);
            camera_transform_comp.position[1] -= 0.005f * dt;
            engineSceneUpdateTransformComponent(scene, go_, &camera_transform_comp);
        }
    }

private:
    inline static const float z_base_offset = 8.0f;
};


class TestObstacle : public engine::IScript
{
public:
    PlayerScript* player_script = nullptr;

public:
    TestObstacle(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "cube");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for table script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = 1.0f;
        tc.position[1] = 0.2f;
        tc.position[2] = 0.0f;

        tc.scale[0] = 0.1f;
        tc.scale[1] = 0.4f;
        tc.scale[2] = 0.1f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto rb = engineSceneAddRigidBodyComponent(scene, go_);
        rb.mass = 15.0f;
        engineSceneUpdateRigidBodyComponent(scene, go_, &rb);

        auto bc = engineSceneAddColliderComponent(scene, go_);
        bc.type = ENGINE_COLLIDER_TYPE_BOX;
        bc.bounciness = 0.0f;
        bc.friction_static = 1.0f;
        engineSceneUpdateColliderComponent(scene, go_, &bc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.1f, 0.1f, 0.1f, 0.0f });
        //material_comp.diffuse_texture = 1;
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);
    }

    void update(float dt) override
    {
        auto scene = my_scene_->get_handle();
        assert(player_script);
        const auto player_go = player_script->get_game_object();
        const auto player_tc = engineSceneGetTransformComponent(scene, player_go);
        auto my_tc = engineSceneGetTransformComponent(scene, go_);


        const auto distance = glm::distance(glm::vec3(player_tc.position[0], player_tc.position[1], player_tc.position[2]),
            glm::vec3(my_tc.position[0], my_tc.position[1], my_tc.position[2]));    


        auto rb = engineSceneGetRigidBodyComponent(scene, go_);
        rb.linear_velocity[0] = -0.1f;
        engineSceneUpdateRigidBodyComponent(scene, go_, &rb);
        //engineLog(fmt::format("Distnace: {}\n", distance).c_str());

    }
};


class TestScene_0 : public engine::IScene
{
public:
    TestScene_0(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
        : IScene(app_handle, scn_mgn, engine_error_code)
        , map_(this)
    {
        const float gravity_vec[3] = { 0.0f, -10.0f, 0.0f };
        engineSceneSetGravityVector(scene_, gravity_vec);
        
        auto player_script = register_script<PlayerScript>(map_);

        auto camera_script = register_script<CameraScript>();
        camera_script->player_script = player_script;
    }

    ~TestScene_0() = default;
    static constexpr const char* get_name() { return "TestScene_0"; }

private:
    GameMap map_;
};
}

int main(int argc, char** argv)
{
	std::string assets_path = "";
	if (argc > 1)
	{
		assets_path = argv[1];
        log(fmt::format("Reading assets from path: {}\n", assets_path));
	}

	engine_application_t app{};
	engine_application_create_desc_t app_cd{};
	app_cd.name = "Pong";
	app_cd.asset_store_path = assets_path.c_str();
    app_cd.width = K_IS_ANDROID ? 0 : 2280 / 2;
    app_cd.height = K_IS_ANDROID ? 0 : 1080 / 2;
    app_cd.fullscreen = K_IS_ANDROID;


	auto engine_error_code = engineApplicationCreate(&app, app_cd);
	if (engine_error_code != ENGINE_RESULT_CODE_OK)
	{
		engineApplicationDestroy(app);
        log(fmt::format("Couldnt create engine application!\n"));
		return -1;
	}

    // cube
    engine_model_info_t model_info{};
    engine_result_code_t model_info_result = ENGINE_RESULT_CODE_FAIL;

    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "cube.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading CUBE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t cube_geometry{};
    engineApplicationAddGeometryFromMemory(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "cube", &cube_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);

    // sphere
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

    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "table.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading TABLE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t table_geometry{};
    engineApplicationAddGeometryFromMemory(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "table", &table_geometry);


    engine_texture2d_t table_diffuse_texture{};
    if (engineApplicationAddTexture2DFromMemory(app, &model_info.materials_array[0].diffuse_texture_info, "table_diffuse_texture", &table_diffuse_texture) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt create diffuse texture for geometry!!\n"));
        return -1;
    }
    engineApplicationReleaseModelInfo(app, &model_info);

    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "y_bot.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading TABLE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t ybot_geometry{};
    engineApplicationAddGeometryFromMemory(app, model_info.geometries_array[1].verts, model_info.geometries_array[1].verts_count,
        model_info.geometries_array[1].inds, model_info.geometries_array[1].inds_count, "y_bot", &ybot_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);

    engine_font_t font_handle{};
    if (engineApplicationAddFontFromFile(app, "tahoma.ttf", "tahoma_font", &font_handle) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt load font!\n"));
        return -1;
    }

    engine::SceneManager scene_manager(app);
    scene_manager.register_scene<project_c::TestScene_0>();

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
            log(fmt::format("Engine requested app quit. Exiting.\n"));
            break;
        }

		if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_ESCAPE))
		{
			log(fmt::format("User pressed ESCAPE key. Exiting.\n"));
			break;
		}

        fps_counter.frames_count += 1;
        fps_counter.frames_total_time += frame_begin.delta_time;
        if (fps_counter.frames_total_time > 1000.0f)
        {
            log(fmt::format("FPS: {}, latency: {} ms. \n",
                fps_counter.frames_count, fps_counter.frames_total_time / fps_counter.frames_count));
            fps_counter = {};
        }

        scene_manager.update(frame_begin.delta_time);

		const auto frame_end = engineApplicationFrameEnd(app);
		if (!frame_end.success)
		{
			log(fmt::format("Frame not finished sucesfully. Exiting.\n"));
			break;
		}
	}

	engineApplicationDestroy(app);

	return 0;
}