#include "level_scene.h"

#include "../app.h"
#include "../scripts/encounter_script.h"
#include "../scripts/game_components.h"
#include "../scripts/gameplay_hud_script.h"
#include "level_builder.h"

#include <cstring>
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

// the start named "default", else the first one; ENGINE_INVALID_GAME_OBJECT_ID when the level has none
engine_game_object_t find_default_player_start()
{
    const auto starts = find_player_starts();
    for (const auto start : starts)
    {
        const auto ps = project_c::get_game_component(start, project_c::game_components().player_start, project_c::default_player_start());
        if (std::strcmp(ps.id, "default") == 0)
        {
            return start;
        }
    }
    return starts.empty() ? ENGINE_INVALID_GAME_OBJECT_ID : starts.front();
}
}  // namespace

project_c::LevelScene::LevelScene(engine::IApplication* app)
    : IScene(app)
{
    auto& typed_app = *static_cast<AppProjectC*>(app);
    const auto& path = typed_app.get_level_path();
    if (engineSceneLoad(path.c_str()) != ENGINE_RESULT_CODE_OK)
    {
        log(fmt::format("Failed loading the level {}.\n", path));
        return;
    }
    // derived data, never saved: built from the level's static colliders
    if (engineNavMeshBuild() != ENGINE_RESULT_CODE_OK)
    {
        engineLog("Failed to build the level's nav mesh; enemies will not move.\n");
    }
    const auto start = find_default_player_start();
    if (start == ENGINE_INVALID_GAME_OBJECT_ID || !LevelBuilder(typed_app, *this).spawn_player(start))
    {
        log(fmt::format("The level {} has no player start (or the player can't be spawned), it has no player.\n", path));
    }
}

project_c::GameplayHud* project_c::LevelScene::hud()
{
    return find_script<GameplayHud>();
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

    // the gameplay UI follows the editor's game/editor view (always the game view without the editor)
    if (auto* hud = this->hud())
    {
        hud->set_visible(engineEditorIsGameView());
    }

    // once the encounter is over (cleared or player dead), restart loads the level again
    auto* app = static_cast<AppProjectC*>(get_app());
    const bool restart_down = app->player_input().restart;
    const bool restart_pressed = restart_down && !restart_was_down_;
    restart_was_down_ = restart_down;
    const auto* encounter = find_script<Encounter>();
    if (encounter && encounter->is_over() && restart_pressed)
    {
        app->request_scene_reload<LevelScene>();
    }
    return result;
}
