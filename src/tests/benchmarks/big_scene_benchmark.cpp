// Big scene benchmark: builds dungeon-like scenes of growing size with the game's LevelBuilder and measures frame times.
// Not a test (no CTest, no CI): it needs a GPU and a visible window with vsync off, so frames are presented as fast as they render.
//
// Every size is a square grid of cells split into rooms of ROOM_CELLS x ROOM_CELLS cells, built the way the level builds
// today: a floor prefab instance (collider + script) per cell, wall pieces (BuildingPart, a collider each) along the room
// borders with a door in every side, two chests, one point light and one enemy per room, and the player in the middle.
// Each size is measured with two camera shots: the gameplay view on the player and an overview of the whole map (worst case).
// Every frame simulates exactly FIXED_DT_MS, so runs are deterministic; the measured times are real (wall clock).
//
// Usage: big_scene_benchmark [--frames N] [--warmup N] [--sizes 32,64,128] [--out results.md]
#include <app.h>
#include <scenes/level_builder.h>
#include <scripts/building_script.h>
#include <scripts/enviorment_script.h>
#include <scripts/player_script.h>

#include <engine.h>
#include <iscene.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr float FIXED_DT_MS = 1000.0f / 60.0f;
constexpr std::int32_t ROOM_CELLS = 8;
// the door in every side of a room, in cells
constexpr std::int32_t DOOR_CELLS = 2;
// Primitive_Wall is 4 x 4 x 1 m: scaled to a 1 m wide, 2 m tall and 0.25 m thick piece
const glm::vec3 WALL_SCALE(0.25f, 0.5f, 0.25f);
constexpr project_c::BuildingPart::Box WALL_BOX = { { 0.0f, 2.0f, 0.0f }, { 2.0f, 2.0f, 0.5f } };
// the gameplay camera of the game (CameraScript): orthographic, 45 degrees pitch
constexpr float GAMEPLAY_ORTHO_HALF_HEIGHT = 5.0f;
constexpr float CAMERA_PITCH_DEGREES = 45.0f;

struct Options
{
    std::uint32_t warmup_frames = 120;
    std::uint32_t measured_frames = 600;
    std::vector<std::int32_t> sizes = { 32, 64, 128 };
    std::string out_path;
};

struct SceneCounts
{
    std::uint32_t floor_tiles = 0;
    std::uint32_t wall_pieces = 0;
    std::uint32_t chests = 0;
    std::uint32_t enemies = 0;
    std::uint32_t point_lights = 0;
    // counted over the game objects with a name (every prefab node and everything placed here has one)
    std::uint32_t game_objects = 0;
    std::uint32_t meshes = 0;
    std::uint32_t skinned_meshes = 0;
    std::uint32_t colliders = 0;
    double build_ms = 0.0;
    double nav_mesh_build_ms = 0.0;
};

// the size of the next BenchmarkScene; scenes are created by type, so it's passed on the side
std::int32_t G_SCENE_CELLS = 0;
SceneCounts G_COUNTS{};

void point_light(float x, float y, float z)
{
    const auto go = engineGameObjectCreate();
    auto nc = engineGameObjectAddNameComponent(go);
    std::strncpy(nc.name, "point-light", ENGINE_ENTITY_NAME_MAX_LENGTH);
    engineGameObjectUpdateNameComponent(go, &nc);
    auto tc = engineGameObjectAddTransformComponent(go);
    tc.position[0] = x;
    tc.position[1] = y;
    tc.position[2] = z;
    engineGameObjectUpdateTransformComponent(go, &tc);
    // the game's PointLight values (enviorment_script.cpp)
    auto lc = engineGameObjectAddLightComponent(go);
    lc.type = ENGINE_LIGHT_TYPE_POINT;
    const std::array<float, 3> ambient = { 0.05f, 0.05f, 0.05f };
    const std::array<float, 3> diffuse = { 1.0f, 0.7f, 0.4f };
    std::copy(ambient.begin(), ambient.end(), lc.intensity.ambient);
    std::copy(diffuse.begin(), diffuse.end(), lc.intensity.diffuse);
    std::copy(diffuse.begin(), diffuse.end(), lc.intensity.specular);
    lc.point.constant = 1.0f;
    lc.point.linear = 0.09f;
    lc.point.quadratic = 0.032f;
    engineGameObjectUpdateLightComponent(go, &lc);
}

class BenchmarkScene : public engine::IScene
{
public:
    static constexpr const char* get_name() { return "big_scene_benchmark"; }

    BenchmarkScene(engine::IApplication* app)
        : IScene(app)
    {
        G_COUNTS = {};
        const auto build_start = std::chrono::steady_clock::now();
        auto& app_c = *static_cast<project_c::AppProjectC*>(app);
        project_c::LevelBuilder builder(app_c, *this);
        builder.nav_surface();
        register_script<project_c::MainLight>();

        const std::int32_t cells = G_SCENE_CELLS;
        const std::int32_t half = cells / 2;
        for (std::int32_t x = -half; x < cells - half; x++)
        {
            for (std::int32_t z = -half; z < cells - half; z++)
            {
                builder.floor(static_cast<float>(x), static_cast<float>(z));
                G_COUNTS.floor_tiles++;
            }
        }

        const std::int32_t rooms = cells / ROOM_CELLS;
        const auto door_begin = (ROOM_CELLS - DOOR_CELLS) / 2;
        for (std::int32_t room_x = 0; room_x < rooms; room_x++)
        {
            for (std::int32_t room_z = 0; room_z < rooms; room_z++)
            {
                // cell of the room's lower corner; walls stand on the room's lower x and z border (the next room closes the other two)
                const auto x0 = -half + room_x * ROOM_CELLS;
                const auto z0 = -half + room_z * ROOM_CELLS;
                for (std::int32_t i = 0; i < ROOM_CELLS; i++)
                {
                    if (i >= door_begin && i < door_begin + DOOR_CELLS)
                    {
                        continue;
                    }
                    wall(x0 - 0.5f, z0 + static_cast<float>(i), 90.0f);
                    wall(x0 + static_cast<float>(i), z0 - 0.5f, 0.0f);
                }
                const auto center_x = x0 + ROOM_CELLS * 0.5f;
                const auto center_z = z0 + ROOM_CELLS * 0.5f;
                point_light(center_x, 2.0f, center_z);
                G_COUNTS.point_lights++;
                if (builder.chest(x0 + 1.0f, 0.1f, z0 + 1.0f))
                {
                    G_COUNTS.chests++;
                }
                if (builder.chest(x0 + ROOM_CELLS - 2.0f, 0.1f, z0 + ROOM_CELLS - 2.0f))
                {
                    G_COUNTS.chests++;
                }
                // no enemy next to the player, so it stays alive during the measurement
                const bool player_room = room_x == rooms / 2 && room_z == rooms / 2;
                if (!player_room && builder.enemy(center_x + 1.5f, center_z + 1.5f))
                {
                    G_COUNTS.enemies++;
                }
            }
        }
        // the map's outer border on the high x and z sides
        for (std::int32_t i = 0; i < cells; i++)
        {
            wall(cells - half - 0.5f, -half + static_cast<float>(i), 90.0f);
            wall(-half + static_cast<float>(i), cells - half - 0.5f, 0.0f);
        }

        player_ = builder.player(ROOM_CELLS * 0.5f, ROOM_CELLS * 0.5f);
        camera_ = create_camera();
        G_COUNTS.build_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - build_start).count();

        const auto nav_start = std::chrono::steady_clock::now();
        if (engineNavMeshBuild() != ENGINE_RESULT_CODE_OK)
        {
            std::cout << "Nav mesh build failed for " << cells << "x" << cells << " cells; enemies will not move.\n";
        }
        G_COUNTS.nav_mesh_build_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - nav_start).count();
    }

    // gameplay: the game's camera on the player; overview: the whole map in view
    void set_camera_shot(bool overview)
    {
        const auto cells = static_cast<float>(G_SCENE_CELLS);
        glm::vec3 target(0.0f);
        if (!overview && player_)
        {
            const auto tc = engineGameObjectGetTransformComponent(player_->get_game_object());
            target = glm::vec3(tc.position[0], tc.position[1], tc.position[2]);
        }
        // the map is seen at 45 degrees, so its depth takes ~0.71 of its size on screen
        const float half_height = overview ? cells * 0.5f * 0.75f : GAMEPLAY_ORTHO_HALF_HEIGHT;
        const float distance = overview ? cells : 20.0f;
        const float pitch = glm::radians(CAMERA_PITCH_DEGREES);
        const glm::vec3 offset(0.0f, distance * std::sin(pitch), distance * std::cos(pitch));

        auto tc = engineGameObjectGetTransformComponent(camera_);
        tc.position[0] = target.x + offset.x;
        tc.position[1] = target.y + offset.y;
        tc.position[2] = target.z + offset.z;
        engineGameObjectUpdateTransformComponent(camera_, &tc);

        auto cc = engineGameObjectGetCameraComponent(camera_);
        cc.type_union.orthographics_scale = half_height;
        cc.clip_plane_far = distance * 3.0f + 50.0f;
        cc.target[0] = target.x;
        cc.target[1] = target.y;
        cc.target[2] = target.z;
        engineGameObjectUpdateCameraComponent(camera_, &cc);
    }

private:
    void wall(float x, float z, float y_rotation_degrees)
    {
        auto& app_c = *static_cast<project_c::AppProjectC*>(get_app());
        if (!app_c.is_prefab_available(project_c::PREFAB_TYPE_BUILDING_WALL))
        {
            return;
        }
        register_script<project_c::BuildingPart>(app_c.instantiate_prefab(project_c::PREFAB_TYPE_BUILDING_WALL).go, "wall",
            glm::vec3(x, 0.0f, z), y_rotation_degrees, WALL_SCALE, WALL_BOX, false);
        G_COUNTS.wall_pieces++;
    }

    static engine_game_object_t create_camera()
    {
        const auto go = engineGameObjectCreate();
        auto cc = engineGameObjectAddCameraComponent(go);
        cc.enabled = true;
        cc.type = ENGINE_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC;
        cc.clip_plane_near = 0.1f;
        cc.clip_plane_far = 100.0f;
        engineGameObjectUpdateCameraComponent(go, &cc);
        engineGameObjectAddTransformComponent(go);
        return go;
    }

    project_c::Player* player_ = nullptr;
    engine_game_object_t camera_ = ENGINE_INVALID_GAME_OBJECT_ID;
};

void count_components(SceneCounts& counts)
{
    engine_component_view_t view{};
    engineComponentViewCreate(&view);
    engineComponentViewAttachNameComponent(view);
    engine_component_iterator_t it{};
    engine_component_iterator_t end{};
    engineComponentViewCreateBeginComponentIterator(view, &it);
    engineComponentViewCreateEndComponentIterator(view, &end);
    while (!engineComponentIteratorCheckEqual(it, end))
    {
        const auto go = engineComponentIteratorGetGameObject(it);
        counts.game_objects++;
        counts.meshes += engineGameObjectHasMeshComponent(go) ? 1 : 0;
        counts.skinned_meshes += engineGameObjectHasSkinnedMeshComponent(go) ? 1 : 0;
        counts.colliders += engineGameObjectHasColliderComponent(go) ? 1 : 0;
        engineComponentIteratorNext(it);
    }
    engineComponentIteratorDelete(it);
    engineComponentIteratorDelete(end);
    engineComponentViewDestroy(view);
}

struct Stats
{
    double median = 0.0;
    double p95 = 0.0;
    double max = 0.0;
};

Stats stats_of(std::vector<double> samples)
{
    if (samples.empty())
    {
        return {};
    }
    std::sort(samples.begin(), samples.end());
    const auto at = [&](double q) { return samples[std::min(samples.size() - 1, static_cast<std::size_t>(q * static_cast<double>(samples.size())))]; };
    return { at(0.5), at(0.95), samples.back() };
}

struct ShotResult
{
    std::int32_t cells = 0;
    std::string shot;
    Stats update;     // AppProjectC::update: scripts, animation, physics, world transforms, gathering draw items
    Stats frame_end;  // engineFrameEnd: rendering (CPU side), present, waiting for the GPU
    Stats frame;      // whole frame, engineFrameBegin to the end of engineFrameEnd
};

// runs `frames` frames; returns false when the window was closed
bool run_frames(project_c::AppProjectC& app, std::uint32_t frames, ShotResult* result)
{
    std::vector<double> update_ms, frame_end_ms, frame_ms;
    for (std::uint32_t i = 0; i < frames; i++)
    {
        const auto t0 = std::chrono::steady_clock::now();
        const auto begin = engineFrameBegin();
        if (begin.events & ENGINE_EVENT_QUIT)
        {
            return false;
        }
        const auto t1 = std::chrono::steady_clock::now();
        app.update(FIXED_DT_MS);
        const auto t2 = std::chrono::steady_clock::now();
        engineFrameEnd();
        const auto t3 = std::chrono::steady_clock::now();
        update_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
        frame_end_ms.push_back(std::chrono::duration<double, std::milli>(t3 - t2).count());
        frame_ms.push_back(std::chrono::duration<double, std::milli>(t3 - t0).count());
    }
    if (result)
    {
        result->update = stats_of(update_ms);
        result->frame_end = stats_of(frame_end_ms);
        result->frame = stats_of(frame_ms);
    }
    return true;
}

Options parse_options(int argc, char** argv)
{
    Options options;
    for (int i = 1; i + 1 < argc; i += 2)
    {
        const std::string key = argv[i];
        const std::string value = argv[i + 1];
        if (key == "--frames")
        {
            options.measured_frames = static_cast<std::uint32_t>(std::stoul(value));
        }
        else if (key == "--warmup")
        {
            options.warmup_frames = static_cast<std::uint32_t>(std::stoul(value));
        }
        else if (key == "--sizes")
        {
            options.sizes.clear();
            std::stringstream ss(value);
            std::string size;
            while (std::getline(ss, size, ','))
            {
                options.sizes.push_back(std::stoi(size));
            }
        }
        else if (key == "--out")
        {
            options.out_path = value;
        }
    }
    return options;
}

std::string format_ms(const Stats& s)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2f / %.2f / %.2f", s.median, s.p95, s.max);
    return buffer;
}
}  // namespace

int main(int argc, char** argv)
{
    const auto options = parse_options(argc, argv);

    project_c::AppConfig config{};
    config.enable_editor = false;
    config.vsync = false;
    config.window_width = 1920;
    config.window_height = 1080;
    project_c::AppProjectC app(project_c::default_prefabs_data(), config);

    std::vector<std::pair<std::int32_t, SceneCounts>> counts;
    std::vector<ShotResult> results;
    for (const auto cells : options.sizes)
    {
        G_SCENE_CELLS = cells;
        auto* scene = static_cast<BenchmarkScene*>(app.register_scene<BenchmarkScene>());
        scene->activate();
        // the first update creates the scripts and starts them (enemies find the player, the player equips its weapon)
        run_frames(app, 1, nullptr);
        auto scene_counts = G_COUNTS;
        count_components(scene_counts);
        counts.emplace_back(cells, scene_counts);

        for (const bool overview : { false, true })
        {
            scene->set_camera_shot(overview);
            ShotResult result;
            result.cells = cells;
            result.shot = overview ? "overview" : "gameplay";
            if (!run_frames(app, options.warmup_frames, nullptr) || !run_frames(app, options.measured_frames, &result))
            {
                return 1;
            }
            std::cout << cells << "x" << cells << " " << result.shot << ": frame median " << result.frame.median << " ms\n";
            results.push_back(result);
        }
        app.unregister_scene(BenchmarkScene::get_name());
        // the unregistered scene is destroyed in the next update
        run_frames(app, 1, nullptr);
    }

    std::ostringstream report;
    report << "## Scenes\n\n";
    report << "| Cells | Floor tiles | Wall pieces | Chests | Enemies | Point lights | Game objects | Meshes | Skinned meshes | Colliders | Build (ms) | Nav mesh build (ms) |\n";
    report << "|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    for (const auto& [cells, c] : counts)
    {
        char build[32], nav[32];
        std::snprintf(build, sizeof(build), "%.0f", c.build_ms);
        std::snprintf(nav, sizeof(nav), "%.0f", c.nav_mesh_build_ms);
        report << "| " << cells << "x" << cells << " | " << c.floor_tiles << " | " << c.wall_pieces << " | " << c.chests << " | " << c.enemies
            << " | " << c.point_lights << " | " << c.game_objects << " | " << c.meshes << " | " << c.skinned_meshes << " | " << c.colliders
            << " | " << build << " | " << nav << " |\n";
    }
    report << "\n## Frame times (ms: median / p95 / max, " << options.measured_frames << " frames after " << options.warmup_frames << " warm-up frames)\n\n";
    report << "| Cells | Shot | Update (game + scene) | Frame end (render + present) | Whole frame | FPS (median) |\n";
    report << "|---|---|---|---|---|---|\n";
    for (const auto& r : results)
    {
        char fps[32];
        std::snprintf(fps, sizeof(fps), "%.0f", r.frame.median > 0.0 ? 1000.0 / r.frame.median : 0.0);
        report << "| " << r.cells << "x" << r.cells << " | " << r.shot << " | " << format_ms(r.update) << " | " << format_ms(r.frame_end)
            << " | " << format_ms(r.frame) << " | " << fps << " |\n";
    }

    std::cout << "\n" << report.str();
    if (!options.out_path.empty())
    {
        std::ofstream(options.out_path) << report.str();
    }
    return 0;
}
