#include <engine.h>

#include "scene_manager.h"
#include "iscene.h"
#include "utils.h"
#include "iscript.h"
#include <network/net_client.h>

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

inline engine_model_desc_t model_info{};

namespace project_c
{
inline std::uint32_t health = 100;

class FloorTile : public engine::IScript
{
public:
    FloorTile(engine::IScene* my_scene, std::int32_t x, std::int32_t y, std::int32_t z)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "plane");
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
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.259f, 0.554f, 0.125f, 1.0f });
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
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });
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

template<std::uint32_t WIDTH, std::uint32_t HEIGHT>
class GameMapT
{
private:
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

public:
    GameMapT(engine::IScene* parent_scene)
        : parent_scene_(parent_scene)
    {
        return;

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

        {
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
                        parent_scene_->register_script<FloorTile>(coord_x, 0, coord_z);
                        break;
                    }
                    case TileID::ePath:
                    {
                        auto* t = parent_scene_->register_script<FloorTile>(coord_x, 0, coord_z);
                        t->set_material(std::array<float, 4>{0.8f, 0.8f, 0.1f, 0.2f});
                        break;
                    }
                    default:
                        assert(!"Unknown tileID!");
                    }

                    switch (obstacle_id)
                    {
                    case ObstacleID::eBox:
                    {
                        parent_scene_->register_script<ObstacleStaticObject>(coord_x, 0, coord_z);
                        break;
                    }
                    }
                }
            }
        }
    }


public:
    bool player_wants_to_move(std::int32_t x, std::int32_t y) const
    {
        y += HEIGHT / 2;
        x += WIDTH / 2;

        // validate out of bounds
        if (x < 0 || x >= WIDTH)
        {
            return false;
        }
        else if (y < 0 || y >= HEIGHT)
        {
            return false;
        }

        // validate tile
        return map_[y][x].can_walk_on();
    }

private:
    std::array<std::array<Tile, WIDTH>, HEIGHT> map_;
    engine::IScene* parent_scene_ = nullptr;
};
using GameMap = GameMapT<20, 20>;


class ControllablePlayerScript : public engine::IScript
{
public:
    ControllablePlayerScript(engine::IScene* my_scene, const GameMap& map)
        : IScript(my_scene)
        , map_(map)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "y_bot");
        mesh_comp.skin = engineApplicationGetSkinByName(app, "skin");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for ybot script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = 0.0f;
        tc.position[1] = 0.0f;
        tc.position[2] = 0.0f;

        //tc.scale[0] = 2.00f;//0.5f;
        //tc.scale[1] = 2.00f;//0.5f;
        //tc.scale[2] = 2.00f;//0.5f;

        tc.scale[0] = 1.0f;//0.01f;
        tc.scale[1] = 1.0f;//0.01f;
        tc.scale[2] = 1.0f;//0.01f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f });
        material_comp.diffuse_texture = engineApplicationGetTextured2DByName(app, "diffuse");
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);

        // animations
        auto anim_comp = engineSceneAddAnimationComponent(scene, go_);
        anim_comp.animations_array[0] = engineApplicationGetAnimationClipByName(app, "animation");
        engineSceneUpdateAnimationComponent(scene, go_, &anim_comp);
    }


    void update(float dt) override
    {
        auto app = my_scene_->get_app_handle();
        auto scene = my_scene_->get_handle();
        next_move_counter_ += dt;

        if (engineSceneHasAnimationComponent(scene, go_))
        {
            auto anim_comp = engineSceneGetAnimationComponent(scene, go_);
            if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_F) && anim_comp.animations_state[0] == ENGINE_ANIMATION_CLIP_STATE_NOT_PLAYING)
            {
                anim_comp.animations_state[0] = ENGINE_ANIMATION_CLIP_STATE_PLAYING;
                engineSceneUpdateAnimationComponent(scene, go_, &anim_comp);
            }
        }

        constexpr const std::int32_t tile_distance = 1;
        struct MoveDir
        {
            std::int32_t x = 0;
            std::int32_t z = 0;
        };
        MoveDir move_dir{};

        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_D))
        {
            move_dir.x = tile_distance;
        }
        else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_A))
        {
            move_dir.x = -tile_distance;
        }
        else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_W))
        {
            move_dir.z = -tile_distance;
        }
        else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_S))
        {
            move_dir.z = tile_distance;
        }
        else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_E))
        {
            move_dir.x = tile_distance;
            move_dir.z = -tile_distance;
        }
        else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_Q))
        {
            move_dir.x = -tile_distance;
            move_dir.z = -tile_distance;
        }
        else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_C))
        {
            move_dir.x = tile_distance;
            move_dir.z = tile_distance;
        }
        else if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_Z))
        {
            move_dir.x = -tile_distance;
            move_dir.z = tile_distance;
        }

        if (engineApplicationIsKeyboardButtonDown(app, ENGINE_KEYBOARD_KEY_SPACE))
        {
            health -= 10;
            log(fmt::format("Health: {}\n", health));
        }
        const bool move_requested = move_dir.x != 0 || move_dir.z != 0;
        //if (!move_requested || next_move_counter_ <= next_move_limit_time_)
        if (!move_requested)
        {
            return;
        }
        //next_move_counter_ = 0.0f;

        auto tc = engineSceneGetTransformComponent(scene, go_);

        //if (map_.player_wants_to_move(tc.position[0] + move_dir.x, tc.position[2] + move_dir.z))
        {
            tc.position[0] += (move_dir.x/1000.0f) * dt;
            tc.position[2] += (move_dir.z/1000.0f) * dt;
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }

        const auto tiles_moved = std::abs(move_dir.x) + std::abs(move_dir.z);
        //next_move_limit_time_ = 500.0f * tiles_moved;
    }

private:
    const GameMap& map_;
    float next_move_counter_ = 0.0f;
    float next_move_limit_time_ = 500.0f;  // in miliseconds

    std::pair<std::int32_t, std::int32_t> position_ = { 0, 0 };
};

class BaseEnemyNPC : public engine::IScript
{
public:
    class ControllablePlayerScript* player_script = nullptr;
public:
    BaseEnemyNPC(engine::IScene* my_scene, std::int32_t x, std::int32_t y, std::int32_t z)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto mesh_comp = engineSceneAddMeshComponent(scene, go_);
        mesh_comp.geometry = engineApplicationGetGeometryByName(app, "sphere");
        assert(mesh_comp.geometry != ENGINE_INVALID_OBJECT_HANDLE && "Cant find geometry for cat script!");
        engineSceneUpdateMeshComponent(scene, go_, &mesh_comp);

        auto tc = engineSceneAddTransformComponent(scene, go_);
        tc.position[0] = static_cast<float>(x);
        tc.position[1] = static_cast<float>(y) + 0.5f;
        tc.position[2] = static_cast<float>(z);

        tc.scale[0] = 0.5f;
        tc.scale[1] = 0.5f;
        tc.scale[2] = 0.5f;
        engineSceneUpdateTransformComponent(scene, go_, &tc);

        auto material_comp = engineSceneAddMaterialComponent(scene, go_);
        set_c_array(material_comp.diffuse_color, std::array<float, 4>{ 0.7f, 0.1f, 0.1f, 1.0f });
        material_comp.diffuse_texture = 0;
        engineSceneUpdateMaterialComponent(scene, go_, &material_comp);
    }

    void update(float dt) override
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();
        move_timer_ += dt;

        // ToDo: replace it with real pathfinding algorthim
        if (move_timer_ >= move_limit_time_)
        {
            move_timer_ = 0.0f;
            const auto players_tc = engineSceneGetTransformComponent(scene, player_script->get_game_object());
            auto tc = engineSceneGetTransformComponent(scene, go_);

            const auto diff_x = players_tc.position[0] - tc.position[0];
            const auto diff_y = players_tc.position[1] - tc.position[1];
            const auto diff_z = players_tc.position[2] - tc.position[2];

            if (diff_x > 1)
            {
                tc.position[0] += 1.0f;
            }
            else if (diff_x < -1)
            {
                tc.position[0] -= 1.0f;
            }
            else
            {
                if (diff_z > 1)
                {
                    tc.position[2] += 1.0f;
                }
                else if (diff_z < -1)
                {
                    tc.position[2] -= 1.0f;
                }
            }
            engineSceneUpdateTransformComponent(scene, go_, &tc);
        }
    }

private:
    float move_timer_ = 0.0f;
    const float move_limit_time_ = 750.0f; // 500ms
};

class CameraScript : public engine::IScript
{
public:
    ControllablePlayerScript* player_script = nullptr;

public:
    CameraScript(engine::IScene* my_scene)
        : IScript(my_scene)
    {
        auto scene = my_scene_->get_handle();
        auto app = my_scene_->get_app_handle();

        auto camera_comp = engineSceneAddCameraComponent(scene, go_);
        camera_comp.enabled = true;
        camera_comp.clip_plane_near = 0.1f;
        camera_comp.clip_plane_far = 1000.0f;
        camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
        camera_comp.type_union.perspective_fov = 45.0f;
        //camera_comp.type = ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC;
        //camera_comp.type_union.orthographics_scale = 5.0f;

        camera_comp.target[0] = 0.0f;
        camera_comp.target[1] = 1.0f;
        camera_comp.target[2] = 0.0f;

        engineSceneUpdateCameraComponent(scene, go_, &camera_comp);

        auto camera_transform_comp = engineSceneAddTransformComponent(scene, go_);
        camera_transform_comp.position[0] = 0.0f;
        camera_transform_comp.position[1] = 1.0f;
        camera_transform_comp.position[2] = 500.0f;
        engineSceneUpdateTransformComponent(scene, go_, &camera_transform_comp);
    }

    void update(float dt) override
    {
        debug_zoom_in_out(dt);
        //set_target_to_perfect_follow_player_position(dt);
    }

private:
    void set_target_to_perfect_follow_player_position(float dt)
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
    inline static const float z_base_offset = 30.0f;
};

class Overworld : public engine::IScene
{
public:
    Overworld(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code)
        : IScene(app_handle, scn_mgn, engine_error_code)
        , map_(this)
    {
        auto player = register_script<ControllablePlayerScript>(map_);
        auto camera_script = register_script<CameraScript>();
        camera_script->player_script = player;
#if 0
        auto bs = register_script<BaseEnemyNPC>(-5, 0, -5);
        bs->player_script = player;
#endif
    }


    ~Overworld() = default;
    static constexpr const char* get_name() { return "Overworld"; }

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

    bool run_test_model = false;
    if (argc > 2)
    {
        std::string str = argv[2];
        if (str == "test")
        {
            run_test_model = true;
        }
    }
    log(fmt::format("Running test model: {}\n", run_test_model));

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

#if 0
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
    engineApplicationAddGeometryFromDesc(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "cube", &cube_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);
    
    // plane
    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "plane.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading PLANE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t plane_geometry{};
    engineApplicationAddGeometryFromDesc(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "plane", &cube_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);


    // sphere
    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "sphere_uv.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading SPHERE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t sphere_geometry{};
    engineApplicationAddGeometryFromDesc(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "sphere", &sphere_geometry);
    engineApplicationReleaseModelInfo(app, &model_info);

    model_info_result = engineApplicationAllocateModelInfoAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "table.glb", &model_info);
    if (model_info_result != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading TABLE model. Exiting!\n");
        return -1;
    }
    engine_geometry_t table_geometry{};
    engineApplicationAddGeometryFromDesc(app, model_info.geometries_array[0].verts, model_info.geometries_array[0].verts_count,
        model_info.geometries_array[0].inds, model_info.geometries_array[0].inds_count, "table", &table_geometry);


    engine_texture2d_t table_diffuse_texture{};
    if (engineApplicationAddTexture2DFromDesc(app, &model_info.materials_array[0].diffuse_texture_info, "table_diffuse_texture", &table_diffuse_texture) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt create diffuse texture for geometry!!\n"));
        return -1;
    }
    engineApplicationReleaseModelInfo(app, &model_info);
#endif

    //engine_model_info_t model_info{};
    //engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, run_test_model ? "test_skin.gltf" : "test2.glb", &model_info);
    engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "riverdance_dance_free_animation.glb", &model_info);
    //engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "CesiumMan.gltf", &model_info);
    //engine_error_code = engineApplicationAllocateModelDescAndLoadDataFromFile(app, ENGINE_MODEL_SPECIFICATION_GLTF_2, "stag.gltf", &model_info);
    if (engine_error_code != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed loading TABLE model. Exiting!\n");
        return -1;
    }

    if (model_info.geometries_count > 0)
    {
        assert(model_info.geometries_count == 1);
        const auto& geo = model_info.geometries_array[0];
        engine_error_code = engineApplicationAddGeometryFromDesc(app, &geo, "y_bot", nullptr);
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed creating geometry for loaded model. Exiting!\n");
            return -1;
        }
    }

    if (model_info.materials_count > 0)
    {
        assert(model_info.materials_count == 1);
        const auto& mat = model_info.materials_array[0];
        if (mat.diffuse_texture_info.data)
        {
            engine_error_code = engineApplicationAddTexture2DFromDesc(app, &mat.diffuse_texture_info, "diffuse", nullptr);
        }
        if (engine_error_code != ENGINE_RESULT_CODE_OK)
        {
            engineLog("Failed creating textured for loaded model. Exiting!\n");
            return -1;
        }
    }

    if (model_info.skins_counts > 0)
    {
        assert(model_info.skins_counts == 1);
        for (auto i = 0; i < model_info.skins_counts; i++)
        {
            const auto& skin = model_info.skins_array[i];
            engine_error_code = engineApplicationAddSkinFromDesc(app, &skin, "skin", nullptr);
            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating textured for loaded model. Exiting!\n");
                return -1;
            }
        }
    }

    if (model_info.animations_counts > 0)
    {
        assert(model_info.animations_counts == 1);       
        for (auto i = 0; i < model_info.animations_counts; i++)
        {
            const auto& anim = model_info.animations_array[i];
            engine_error_code = engineApplicationAddAnimationClipFromDesc(app, &anim, "animation", nullptr);
            if (engine_error_code != ENGINE_RESULT_CODE_OK)
            {
                engineLog("Failed creating textured for loaded model. Exiting!\n");
                return -1;
            }
        }
    }

    //engineApplicationReleaseModelInfo(app, &model_info);

    engine_font_t font_handle{};
    if (engineApplicationAddFontFromFile(app, "tahoma.ttf", "tahoma_font", &font_handle) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Couldnt load font!\n"));
        return -1;
    }



    std::array<engine_ui_document_data_binding_t, 1> bindings{};
    bindings[0].data_uint32_t = &project_c::health;
    bindings[0].name = "value";
    bindings[0].type = ENGINE_DATA_TYPE_UINT32;
    engine_ui_data_handle_t ui_data_handle{};
    engine_error_code = engineApplicationCreateUiDocumentDataHandle(app, "health", bindings.data(), bindings.size(), &ui_data_handle);

    // load ui doc
    engine_ui_document_t ui_doc{};
    engine_error_code = engineApplicationCreateUiDocumentFromFile(app, "project_c_health_bar.rml", &ui_doc);
    if (ui_doc)
    {
        engineUiDocumentShow(ui_doc);
    }


    engine::SceneManager scene_manager(app);
    scene_manager.register_scene<project_c::Overworld>();

    struct fps_counter_t
    {
        float frames_total_time = 0.0f;
        std::uint32_t frames_count = 0;
    };
    fps_counter_t fps_counter{};

    auto mouse_coords_prev = engine_coords_2d_t{};

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

        const auto mouse_coords = engineApplicationGetMouseCoords(app);
        if (mouse_coords.x != mouse_coords_prev.x || mouse_coords.y != mouse_coords_prev.y)
        {
            //log(std::format("Mouse coord x,y: [{}, {}]\n", mouse_coords.x, mouse_coords.y));
            mouse_coords_prev = mouse_coords;
        }

        fps_counter.frames_count += 1;
        fps_counter.frames_total_time += frame_begin.delta_time;
        if (fps_counter.frames_total_time > 1000.0f)
        {
            log(fmt::format("FPS: {}, latency: {} ms. \n",
                fps_counter.frames_count, fps_counter.frames_total_time / fps_counter.frames_count));
            fps_counter = {};
        }
        engineUiDataHandleDirtyVariable(ui_data_handle, "value");

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