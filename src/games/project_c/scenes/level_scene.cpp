#include "level_scene.h"

#include "../app.h"
#include "../scripts/encounter_script.h"
#include "../scripts/game_components.h"
#include "../scripts/gameplay_hud_script.h"
#include "level_builder.h"
#include "../scripts/scripts_utils.h"

#include <utils.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
// the level's player starts, in the order of the scene's storage
std::vector<engine_game_object_t> find_player_starts()
{
    std::vector<engine_game_object_t> starts;
    engine_component_view_t view = nullptr;
    engineComponentViewCreate(&view);
    engineComponentViewAttachDataComponent(view, project_c::game_components().player_start);
    engine_component_iterator_t it = nullptr;
    engine_component_iterator_t end = nullptr;
    engineComponentViewCreateBeginComponentIterator(view, &it);
    engineComponentViewCreateEndComponentIterator(view, &end);
    for (; !engineComponentIteratorCheckEqual(it, end); engineComponentIteratorNext(it))
    {
        starts.push_back(engineComponentIteratorGetGameObject(it));
    }
    engineComponentIteratorDelete(it);
    engineComponentIteratorDelete(end);
    engineComponentViewDestroy(view);
    return starts;
}

// the start with the id; ENGINE_INVALID_GAME_OBJECT_ID when the level has none
engine_game_object_t find_player_start(const std::vector<engine_game_object_t>& starts, std::string_view id)
{
    for (const auto start : starts)
    {
        const auto ps = project_c::get_game_component(start, project_c::game_components().player_start, project_c::default_player_start());
        if (project_c::utils::to_string_view(ps.id) == id)
        {
            return start;
        }
    }
    return ENGINE_INVALID_GAME_OBJECT_ID;
}

// the entrance's start, else the one named "default", else the first one; ENGINE_INVALID_GAME_OBJECT_ID when the level
// has none
engine_game_object_t find_entrance(const std::string& level_path, const std::string& entrance)
{
    const auto starts = find_player_starts();
    if (starts.empty())
    {
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
    if (const auto start = find_player_start(starts, entrance); start != ENGINE_INVALID_GAME_OBJECT_ID)
    {
        return start;
    }
    if (entrance != "default")
    {
        if (const auto start = find_player_start(starts, "default"); start != ENGINE_INVALID_GAME_OBJECT_ID)
        {
            log(fmt::format("The level {} has no player start \"{}\", the player enters at the default one.\n", level_path, entrance));
            return start;
        }
    }
    log(fmt::format("The level {} has no player start \"{}\" and no default one, the player enters at its first one.\n", level_path,
        entrance));
    return starts.front();
}
}  // namespace

project_c::LevelScene::LevelScene(engine::IApplication* app)
    : IScene(app)
{
    auto& typed_app = *static_cast<AppProjectC*>(app);
    const auto& path = typed_app.get_level_path();
    // a game without its level has nothing to play: the caller (main) reports it and exits
    if (engineSceneLoad(path.c_str()) != ENGINE_RESULT_CODE_OK)
    {
        throw std::runtime_error(fmt::format("Failed loading the level {} (a missing file or not a scene file).", path));
    }
    // derived data, never saved: built from the level's static colliders
    if (engineNavMeshBuild() != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed to build the level's nav mesh; enemies will not move.\n");
    }
}

void project_c::LevelScene::on_play_begin()
{
    auto& typed_app = *static_cast<AppProjectC*>(get_app());
    const auto& path = typed_app.get_level_path();
    const auto start = find_entrance(path, typed_app.get_level_entrance());
    if (start == ENGINE_INVALID_GAME_OBJECT_ID || !LevelBuilder(typed_app, *this).spawn_player(start))
    {
        log(fmt::format("The level {} has no player start (or the player can't be spawned), it has no player.\n", path));
    }
}

void project_c::LevelScene::on_play_end()
{
    // the session's scripts are gone; the next session's are found again
    level_scripts_found_ = false;
    hud_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    encounter_go_ = ENGINE_INVALID_GAME_OBJECT_ID;
    // a key held at Stop isn't a press of the next session
    restart_was_down_ = false;
}

void project_c::LevelScene::find_level_scripts()
{
    if (const auto* hud = find_script<GameplayHud>())
    {
        hud_go_ = hud->get_game_object();
    }
    if (const auto* encounter = find_script<Encounter>())
    {
        encounter_go_ = encounter->get_game_object();
    }
    level_scripts_found_ = true;
}

project_c::GameplayHud* project_c::LevelScene::hud()
{
    return get_script<GameplayHud>(hud_go_);
}

void project_c::LevelScene::activate()
{
    IScene::activate();
    if (auto* hud = this->hud())
    {
        hud->set_visible(engineEditorIsGameView());
    }
}

void project_c::LevelScene::deactivate()
{
    IScene::deactivate();
    if (auto* hud = this->hud())
    {
        hud->set_visible(false);
    }
}

engine_result_code_t project_c::LevelScene::update(float dt)
{
    const auto result = IScene::update(dt);
    // the scripts from the file exist after the first update of a play session
    if (!level_scripts_found_ && engine::is_play_session_state())
    {
        find_level_scripts();
    }

    // the gameplay UI follows the editor's game/editor view (always the game view without the editor)
    if (auto* hud = this->hud())
    {
        hud->set_visible(engineEditorIsGameView());
    }

    // a level exit was taken: the next level replaces this one, the player enters it at the exit's entrance with what
    // they carry
    auto* app = static_cast<AppProjectC*>(get_app());
    if (auto travel = app->take_level_travel_request())
    {
        app->set_level_path(std::move(travel->level_path), std::move(travel->entrance));
        // what the player has now is what they enter the next level with (and get back when they restart it)
        app->set_level_entry_state(app->player_state());
        app->request_scene_reload<LevelScene>();
        return result;
    }

    // once the encounter is over (cleared or player dead), restart loads the level again (with what the player had
    // when entering it, at the same entrance)
    const bool restart_down = app->player_input().restart;
    const bool restart_pressed = restart_down && !restart_was_down_;
    restart_was_down_ = restart_down;
    const auto* encounter = get_script<Encounter>(encounter_go_);
    if (encounter && encounter->is_over() && restart_pressed)
    {
        // the scene's file restarts: the level the editor opened (or saved under another name), not the one the game
        // started with
        const std::string scene_path = engineSceneGetPath();
        if (scene_path.empty())
        {
            log("The level has no file (a new scene not saved yet), so it can't be loaded again to restart it.\n");
            return result;
        }
        app->set_level_path(scene_path, app->get_level_entrance());
        app->request_scene_reload<LevelScene>();
    }
    return result;
}
