#include "sandbox_scene.h"

#include <scenes/level_builder.h>
#include <scenes/level_scene.h>
#include <scripts/encounter_script.h>
#include <scripts/enemy_script.h>
#include <scripts/game_components.h>
#include <scripts/gameplay_hud_script.h>
#include <scripts/player_script.h>

#include <fmt/format.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <gtest/gtest.h>
#include <rapidjson/document.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

// The real level (LevelScene loading assets/scenes/test_level.json). It renders the whole level every frame, so it is
// the slowest scenario per frame (~17 ms in Debug); keep the tests short.
namespace
{
using LevelTest = scenario::ScenarioTest;

const std::filesystem::path LEVEL_FILE = std::filesystem::path(TEST_ASSETS_DIR) / "scenes" / "test_level.json";

// a file in the temp folder, named for this process: two test runs at the same time don't share it
std::filesystem::path temp_file(const char* name)
{
#ifdef _WIN32
    const auto pid = _getpid();
#else
    const auto pid = getpid();
#endif
    return std::filesystem::temp_directory_path() / fmt::format("project_c_{}_{}.json", name, pid);
}

std::string read_text(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// a level file in the temp folder, loaded instead of the game's level while it lives
class TempLevel
{
public:
    TempLevel() { scenario::app().set_level_path(path_.string()); }
    explicit TempLevel(const std::string& content) : TempLevel() { std::ofstream(path_, std::ios::binary) << content; }
    // a temp file of the name, written later (e.g. by engineSceneSave), not loaded until the test sets the level path
    static TempLevel unwritten(const char* name) { return TempLevel(temp_file(name)); }
    ~TempLevel()
    {
        // the game starts again: its first level, a fresh player
        scenario::app().set_level_path(project_c::AppConfig{}.level_path);
        scenario::app().set_level_entry_state(std::nullopt);
        std::filesystem::remove(path_);
    }
    const std::filesystem::path& path() const { return path_; }

private:
    explicit TempLevel(std::filesystem::path path) : path_(std::move(path)) {}

    std::filesystem::path path_ = temp_file("level_test");
};

// replaces the first `from` in `text`; the test fails when there is none
void replace_first(std::string& text, const std::string& from, const std::string& to)
{
    const auto at = text.find(from);
    ASSERT_NE(at, std::string::npos) << from;
    text.replace(at, from.size(), to);
}

void replace_all(std::string& text, const std::string& from, const std::string& to)
{
    for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
    {
        text.replace(at, from.size(), to);
    }
}

// Everything in `file` is in `saved` with the same value: objects may have more members in `saved` (fields a newer
// engine added), but nothing of `file` is missing or different. `path` names where a difference is.
void expect_contained(const rapidjson::Value& file, const rapidjson::Value& saved, const std::string& path)
{
    if (file.IsObject() && saved.IsObject())
    {
        for (const auto& member : file.GetObject())
        {
            const auto name = std::string(member.name.GetString(), member.name.GetStringLength());
            const auto it = saved.FindMember(member.name);
            if (it == saved.MemberEnd())
            {
                ADD_FAILURE() << path << "." << name << " is lost when the level is loaded";
                continue;
            }
            expect_contained(member.value, it->value, path + "." + name);
        }
        return;
    }
    if (file.IsArray() && saved.IsArray() && file.Size() == saved.Size())
    {
        for (rapidjson::SizeType i = 0; i < file.Size(); i++)
        {
            expect_contained(file[i], saved[i], fmt::format("{}[{}]", path, i));
        }
        return;
    }
    EXPECT_TRUE(file == saved) << path << " changes when the level is loaded";
}

TEST_F(LevelTest, LevelLoadsAndPlays)
{
    auto* scene = load_scene<project_c::LevelScene>();
    step(60);

    const auto* player = scene->find_script<project_c::Player>();
    ASSERT_NE(player, nullptr);
    EXPECT_FALSE(player->is_dead());
    EXPECT_NE(scene->find_script<project_c::GameplayHud>(), nullptr);
    // one pack of 5
    EXPECT_EQ(scene->get_scripts<project_c::Enemy>().size(), 5u);
    const auto* encounter = scene->find_script<project_c::Encounter>();
    ASSERT_NE(encounter, nullptr);
    EXPECT_EQ(encounter->get_state(), project_c::Encounter::State::eInProgress);
    EXPECT_EQ(encounter->get_remaining_enemies(), 5u);
}

// The committed level loads with nothing skipped: everything it holds is still there when the loaded level is saved.
// (A newer engine may write more, e.g. a field added to a component: old files keep loading with its default.)
TEST_F(LevelTest, LevelFileLoadsCompletely)
{
    load_scene<project_c::LevelScene>();
    const auto saved_path = temp_file("level_saved");
    ASSERT_EQ(engineSceneSave(saved_path.string().c_str()), ENGINE_RESULT_CODE_OK);
    const auto saved_text = read_text(saved_path);
    std::filesystem::remove(saved_path);

    rapidjson::Document file;
    rapidjson::Document saved;
    file.Parse(read_text(LEVEL_FILE).c_str());
    saved.Parse(saved_text.c_str());
    ASSERT_FALSE(file.HasParseError());
    ASSERT_FALSE(saved.HasParseError());
    expect_contained(file, saved, "test_level.json");
}

// Saving a loaded level is stable: a save loaded and saved again gives the same bytes (deterministic output, nothing
// lost or added on the way; the spawned player is never saved).
TEST_F(LevelTest, LevelSaveLoadSaveGivesTheSameFile)
{
    load_scene<project_c::LevelScene>();
    const TempLevel first;
    ASSERT_EQ(engineSceneSave(first.path().string().c_str()), ENGINE_RESULT_CODE_OK);
    TearDown();

    load_scene<project_c::LevelScene>();
    const auto second = temp_file("level_saved_again");
    ASSERT_EQ(engineSceneSave(second.string().c_str()), ENGINE_RESULT_CODE_OK);
    const auto again = read_text(second);
    std::filesystem::remove(second);
    EXPECT_EQ(again, read_text(first.path()));
}

// a file edited by hand or saved by another version: unknown components and fields are skipped (logged), a missing
// field keeps its default, and the level still plays
TEST_F(LevelTest, LevelFileWithUnknownAndMissingDataStillPlays)
{
    const auto default_max_hp = project_c::default_enemy_config().max_hp;
    const auto file_max_hp = default_max_hp + 57;
    auto text = read_text(LEVEL_FILE);
    replace_first(text, "\"components\": {", "\"components\": { \"no_such_component\": { \"x\": 1 },");
    // every enemy gets a max_hp other than the default, then the first one loses it
    replace_all(text, fmt::format("\"max_hp\": {},", default_max_hp), fmt::format("\"max_hp\": {},", file_max_hp));
    replace_first(text, fmt::format("\"max_hp\": {},", file_max_hp), "\"no_such_field\": 1,");
    const TempLevel level(text);

    auto* scene = load_scene<project_c::LevelScene>();
    step(60);
    ASSERT_NE(scene->find_script<project_c::Player>(), nullptr);
    const auto enemies = scene->get_scripts<project_c::Enemy>();
    ASSERT_EQ(enemies.size(), 5u);
    std::size_t with_default = 0;
    std::size_t with_file_value = 0;
    for (const auto* enemy : enemies)
    {
        with_default += enemy->get_max_hp() == default_max_hp;
        with_file_value += enemy->get_max_hp() == file_max_hp;
    }
    EXPECT_EQ(with_default, 1u);
    EXPECT_EQ(with_file_value, 4u);
    EXPECT_EQ(scene->find_script<project_c::Encounter>()->get_state(), project_c::Encounter::State::eInProgress);
}

// a level which can't be loaded stops the game (LevelScene throws, main reports it and exits)
TEST_F(LevelTest, LevelWhichCantBeLoadedThrows)
{
    const TempLevel missing;  // no file is written
    EXPECT_THROW(load_scene<project_c::LevelScene>(), std::runtime_error);
}

// A scene destroyed before its first update still owns the scripts waiting in its register queue (the HUD's
// UI document among them); leaking them made the next load of the level crash.
TEST_F(LevelTest, LevelUnloadedBeforeFirstStepCanBeLoadedAgain)
{
    load_scene<project_c::LevelScene>();
    TearDown();

    auto* scene = load_scene<project_c::LevelScene>();
    step(1);
    EXPECT_NE(scene->find_script<project_c::GameplayHud>(), nullptr);
    EXPECT_NE(scene->find_script<project_c::Player>(), nullptr);
}

// the level holds player starts, not the player: the player is spawned at the default start, as a runtime object
TEST_F(LevelTest, PlayerIsSpawnedAtThePlayerStart)
{
    auto* scene = load_scene<project_c::LevelScene>();
    step(1);
    const auto* player = scene->find_script<project_c::Player>();
    ASSERT_NE(player, nullptr);
    EXPECT_TRUE(engineGameObjectIsTransient(player->get_game_object()));

    // the file's default player start (the others are entrances from other levels)
    rapidjson::Document file;
    file.Parse(read_text(LEVEL_FILE).c_str());
    const rapidjson::Value* start_position = nullptr;
    for (const auto& entity : file["entities"].GetArray())
    {
        // a prefab instance (the enemies) has its root's components under its node path ""
        const auto& components = entity.HasMember("prefab") ? entity["nodes"][""]["components"] : entity["components"];
        if (components.HasMember("player_start") && std::string(components["player_start"]["id"].GetString()) == "default")
        {
            EXPECT_EQ(start_position, nullptr) << "the level has one default player start";
            start_position = &components["transform"]["position"];
        }
        EXPECT_FALSE(components.HasMember("script") && std::string(components["script"]["type"].GetString()) == "player");
    }
    ASSERT_NE(start_position, nullptr);
    // one frame after spawning: it hasn't moved sideways, and has fallen at most a little
    const auto tc = engineGameObjectGetTransformComponent(player->get_game_object());
    EXPECT_NEAR(tc.position[0], (*start_position)[0].GetFloat(), 1e-3f);
    EXPECT_NEAR(tc.position[2], (*start_position)[2].GetFloat(), 1e-3f);
    EXPECT_NEAR(tc.position[1], (*start_position)[1].GetFloat(), 0.1f);
}

// Two small levels with an exit to each other: walking into an exit loads the other level, the player enters it at the
// exit's entrance and keeps what they had (hp, coins, weapon); the exit there leads back.
TEST_F(LevelTest, LevelExitsChangeTheLevelAndBack)
{
    const auto first = TempLevel::unwritten("level_first");
    const auto second = TempLevel::unwritten("level_second");
    // first: the game starts at (0, 0), its exit is 2 m to the right (+x); the way back from the second level enters
    // at (0, -1.5)
    {
        auto* scene = load_scene<scenario::EmptyScene>();
        project_c::LevelBuilder build(scenario::app(), *scene);
        // the player needs a camera to aim with
        build.camera();
        build.nav_surface();
        build.floor_area(-3.0f, -3.0f, 3.0f, 3.0f);
        build.player_start(0.0f, 0.0f, 90.0f, "default");
        build.player_start(0.0f, -1.5f, 90.0f, "from_second");
        build.level_exit(2.0f, 0.0f, second.path().string(), "from_first");
        ASSERT_EQ(engineSceneSave(first.path().string().c_str()), ENGINE_RESULT_CODE_OK);
        TearDown();
    }
    // second: entered from the first level at (0, 1), its exit back is 2 m to the left
    {
        auto* scene = load_scene<scenario::EmptyScene>();
        project_c::LevelBuilder build(scenario::app(), *scene);
        // the player needs a camera to aim with
        build.camera();
        build.nav_surface();
        build.floor_area(-3.0f, -3.0f, 3.0f, 3.0f);
        build.player_start(0.0f, 1.0f, -90.0f, "from_first");
        build.level_exit(-2.0f, 1.0f, first.path().string(), "from_second");
        ASSERT_EQ(engineSceneSave(second.path().string().c_str()), ENGINE_RESULT_CODE_OK);
        TearDown();
    }

    // walks with `input` until the level changes; the player of the new level
    const auto walk_to_next_level = [this](const project_c::PlayerInput& input) -> project_c::Player*
    {
        const auto level = scenario::app().get_level_path();
        set_input(input);
        for (int frame = 0; frame < 300 && scenario::app().get_level_path() == level; frame++)
        {
            step();
        }
        release_all_input();
        EXPECT_NE(scenario::app().get_level_path(), level) << "the player never went through the exit";
        // the new level's player starts in its first update
        step();
        auto* scene = scenario::app().get_scene(project_c::LevelScene::get_name());
        return scene ? scene->find_script<project_c::Player>() : nullptr;
    };
    const auto position_of = [](const project_c::Player* player)
    {
        const auto tc = engineGameObjectGetTransformComponent(player->get_game_object());
        return glm::vec3(tc.position[0], tc.position[1], tc.position[2]);
    };

    scenario::app().set_level_path(first.path().string());
    auto* scene = load_scene<project_c::LevelScene>();
    step(30);
    auto* player = scene->find_script<project_c::Player>();
    ASSERT_NE(player, nullptr);
    player->add_coin(3);
    const auto max_hp = player->get_max_hp();
    ASSERT_TRUE(player->receive_damage(10, position_of(player) + glm::vec3(-1.0f, 0.0f, 0.0f)));
    step(30);

    project_c::PlayerInput right{};
    right.right = true;
    player = walk_to_next_level(right);
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(scenario::app().get_level_path(), second.path().string());
    EXPECT_NEAR(position_of(player).x, 0.0f, 0.05f);
    EXPECT_NEAR(position_of(player).z, 1.0f, 0.05f);
    EXPECT_EQ(player->get_coins(), 3u);
    EXPECT_EQ(player->get_hp(), max_hp - 10);
    EXPECT_EQ(player->state().weapon, project_c::WeaponType::eDaggers);

    step(30);
    project_c::PlayerInput left{};
    left.left = true;
    player = walk_to_next_level(left);
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(scenario::app().get_level_path(), first.path().string());
    EXPECT_NEAR(position_of(player).x, 0.0f, 0.05f);
    EXPECT_NEAR(position_of(player).z, -1.5f, 0.05f);
    EXPECT_EQ(player->get_coins(), 3u);
    EXPECT_EQ(player->get_hp(), max_hp - 10);
}

using PlayerStartTest = scenario::GameplayTest;

// a start parented under a moved and turned room: the player stands at the start's world position, facing its way
TEST_F(PlayerStartTest, PlayerSpawnsAtTheStartsWorldTransform)
{
    auto* scene = load_sandbox_scene();
    const auto room = engineGameObjectCreate();
    auto room_tc = engineGameObjectAddTransformComponent(room);
    room_tc.position[0] = 2.0f;
    room_tc.position[2] = 1.0f;
    const auto turn = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    room_tc.rotation[0] = turn.x;
    room_tc.rotation[1] = turn.y;
    room_tc.rotation[2] = turn.z;
    room_tc.rotation[3] = turn.w;
    engineGameObjectUpdateTransformComponent(room, &room_tc);

    const auto start = engineGameObjectCreate();
    auto start_tc = engineGameObjectAddTransformComponent(start);
    start_tc.position[0] = 1.0f;
    start_tc.position[1] = 1.0f;
    engineGameObjectUpdateTransformComponent(start, &start_tc);
    auto pc = engineGameObjectAddParentComponent(start);
    pc.parent = room;
    engineGameObjectUpdateParentComponent(start, &pc);

    // spawned before any scene update, like a level right after loading
    const auto* player = scene->build().spawn_player(start);
    ASSERT_NE(player, nullptr);
    const auto tc = engineGameObjectGetTransformComponent(player->get_game_object());
    // the turn takes the start's local +x to the world's -z
    EXPECT_NEAR(tc.position[0], 2.0f, 1e-4f);
    EXPECT_NEAR(tc.position[1], 1.0f, 1e-4f);
    EXPECT_NEAR(tc.position[2], 0.0f, 1e-4f);
    const auto rotation = glm::make_quat(tc.rotation);
    EXPECT_NEAR(std::abs(glm::dot(rotation, turn)), 1.0f, 1e-4f);
    EXPECT_NEAR(tc.scale[0], project_c::Player::MODEL_SCALE, 1e-6f);
}
}  // namespace

