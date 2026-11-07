#include "sandbox_scene.h"

#include <scripts/building_script.h>
#include <scripts/game_components.h>
#include <scripts/player_script.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <vector>

// The player's traversal around the house (LevelBuilder::house): running up a wall onto the roof, climbing, leaping,
// running off and climbing down roof edges. Outcomes with tolerances (where the player ends up, which movement mode it
// went through), not exact paths, so tuning changes don't break them.
namespace
{
using project_c::MovementMode;
using project_c::Player;
using project_c::PlayerInput;
using scenario::FIXED_DT_MS;

// frames for the player, spawned above the floor, to land and recover from the landing
constexpr std::uint32_t SETTLE_FRAMES = 45;
// the house's east wall: its outer face, and the middle of one of its two pieces
constexpr float EAST_WALL_FACE_X = project_c::House::half_size + project_c::House::wall_thickness * 0.5f;
constexpr float EAST_WALL_Z = project_c::House::half_size * 0.5f;
// the player's feet on the floor (its tiles' top is a little above 0) are below this
constexpr float ON_FLOOR_MAX_Y = 0.3f;
// the roof's top above the house's ground
constexpr float ROOF_TOP = project_c::House::wall_height + project_c::House::roof_thickness;

float seconds(std::uint32_t frames)
{
    return frames * FIXED_DT_MS / 1000.0f;
}

class TraversalTest : public scenario::GameplayTest
{
protected:
    // a house centered on the origin standing on `ground_y` (on an invisible block when above the floor and `on_block`,
    // floating otherwise), on a floor reaching well past it, and the player at (x, z) above the floor
    Player* build(float player_x, float player_z, float ground_y = 0.0f, bool on_block = true)
    {
        scene_ = load_sandbox_scene();
        auto* scene = scene_;
        scene->build().floor_area(-7.0f, -7.0f, 7.0f, 7.0f);
        if (ground_y > 0.0f && on_block)
        {
            const float block = (project_c::House::half_size + project_c::House::wall_thickness * 0.5f) * 2.0f;
            scene->build().obstacle(glm::vec3(0.0f, ground_y * 0.5f, 0.0f), glm::vec3(block, ground_y, block));
        }
        house_ground_y_ = ground_y;
        EXPECT_NE(scene->build().house(0.0f, 0.0f, ground_y), nullptr);
        auto* player = scene->build().player(player_x, player_z);
        scene->snap_camera_to_player();
        return player;
    }

    // the player put on the roof at (x, z) before the first step
    void place_on_roof(Player* player, float x, float z)
    {
        auto tc = engineGameObjectGetTransformComponent(player->get_game_object());
        tc.position[0] = x;
        tc.position[1] = house_ground_y_ + ROOF_TOP + 0.05f;
        tc.position[2] = z;
        engineGameObjectUpdateTransformComponent(player->get_game_object(), &tc);
    }

    // Steps with `input` until `done` (checked after every frame) or `max_frames`; the frames stepped, max_frames + 1
    // when it never got done. Every movement mode the player went through is in modes_seen_.
    std::uint32_t step_until(const PlayerInput& input, std::uint32_t max_frames, const std::function<bool()>& done)
    {
        set_input(input);
        for (std::uint32_t frame = 1; frame <= max_frames; frame++)
        {
            step();
            modes_seen_.push_back(player_->movement_mode());
            if (done())
            {
                return frame;
            }
        }
        return max_frames + 1;
    }

    bool went_through(MovementMode mode) const
    {
        return std::find(modes_seen_.begin(), modes_seen_.end(), mode) != modes_seen_.end();
    }

    glm::vec3 player_position() const
    {
        return position_of(player_->get_game_object());
    }

    scenario::SandboxScene* scene_ = nullptr;
    Player* player_ = nullptr;
    std::vector<MovementMode> modes_seen_;
    float house_ground_y_ = 0.0f;
};

PlayerInput moving_left()
{
    PlayerInput input{};
    input.left = true;
    return input;
}

PlayerInput moving_right()
{
    PlayerInput input{};
    input.right = true;
    return input;
}
}  // namespace

// running into a wall up to the run-up height carries the player up it and over the edge onto the roof in one move
TEST_F(TraversalTest, RunningIntoWallRunsUpOntoRoof)
{
    player_ = build(EAST_WALL_FACE_X + 3.0f, EAST_WALL_Z);
    ASSERT_NE(player_, nullptr);
    step(SETTLE_FRAMES);

    // run at the wall until the traversal starts
    const auto reached = step_until(moving_left(), 180, [&] { return player_->movement_mode() != MovementMode::eGrounded; });
    ASSERT_LE(reached, 180u);
    EXPECT_EQ(player_->movement_mode(), MovementMode::eScriptedMove);

    // on the roof, standing
    modes_seen_.clear();
    const auto on_roof = step_until(moving_left(), 60, [&] { return player_->movement_mode() == MovementMode::eGrounded; });
    EXPECT_LE(seconds(on_roof), 0.6f);
    EXPECT_GT(player_position().y, ROOF_TOP - 0.1f);
    EXPECT_LT(player_position().x, EAST_WALL_FACE_X);
    EXPECT_FALSE(went_through(MovementMode::eClimbing));
}

// an attack pressed during a run-up isn't lost: it is done once the player stands on the roof
TEST_F(TraversalTest, AttackPressedDuringRunUpIsDoneAfterIt)
{
    player_ = build(EAST_WALL_FACE_X + 3.0f, EAST_WALL_Z);
    ASSERT_NE(player_, nullptr);
    step(SETTLE_FRAMES);
    const auto reached = step_until(moving_left(), 180, [&] { return player_->movement_mode() == MovementMode::eScriptedMove; });
    ASSERT_LE(reached, 180u);

    PlayerInput attack{};
    attack.attack = true;
    step_until(attack, 1, [] { return true; });
    ASSERT_EQ(player_->movement_mode(), MovementMode::eScriptedMove);
    const auto on_roof = step_until({}, 60, [&] { return player_->movement_mode() == MovementMode::eGrounded; });
    ASSERT_LE(on_roof, 60u);

    // the first attack of the combo plays on the combat layer
    const auto attacking = step_until({}, 5, [&]
        {
            return engineAnimationControllerIsAnimationPlayingOnLayer(player_->get_game_object(), "Dualwield_Melee_Attack_Chop", 1);
        });
    EXPECT_LE(attacking, 5u);
}

// a wall higher than the run-up reaches is run up that far, then climbed: the player stays on the wall (regression: it
// was put at the world origin)
TEST_F(TraversalTest, RunningIntoHighWallRunsUpThenClimbs)
{
    // a house floating a little above the floor: its roof is above the run-up height, its walls reach down below the
    // player's chest (so they are climbed) and its gap above the floor is too low to walk under
    constexpr float FLOAT_Y = 0.7f;
    player_ = build(EAST_WALL_FACE_X + 3.0f, EAST_WALL_Z, FLOAT_Y, false);
    ASSERT_NE(player_, nullptr);
    step(SETTLE_FRAMES);
    const float floor_y = player_position().y;

    const auto climbing = step_until(moving_left(), 180, [&] { return player_->movement_mode() == MovementMode::eClimbing; });
    ASSERT_LE(climbing, 180u);
    EXPECT_TRUE(went_through(MovementMode::eScriptedMove));
    // on the wall, about the run-up height up
    const auto on_wall = player_position();
    EXPECT_NEAR(on_wall.x, EAST_WALL_FACE_X + 0.2f, 0.1f);
    EXPECT_NEAR(on_wall.z, EAST_WALL_Z, 0.3f);
    const float hands_reach = project_c::default_traversal_tuning().run_up_max_height;
    EXPECT_GT(on_wall.y - floor_y, hands_reach - 1.3f);
    EXPECT_LT(on_wall.y - floor_y, hands_reach);
}

// walking into the wall from right next to it (no run-up) starts a climb at the bottom of the wall
TEST_F(TraversalTest, WalkingSlowlyIntoWallStartsClimb)
{
    player_ = build(EAST_WALL_FACE_X + 0.25f, EAST_WALL_Z);
    ASSERT_NE(player_, nullptr);
    step(SETTLE_FRAMES);

    const auto climbing = step_until(moving_left(), 30, [&] { return player_->movement_mode() == MovementMode::eClimbing; });
    ASSERT_LE(climbing, 30u);
    EXPECT_LT(player_position().y, ON_FLOOR_MAX_Y);
    EXPECT_FALSE(went_through(MovementMode::eScriptedMove));

    // and climbs up while the input pushes into the wall
    step(20);
    EXPECT_GT(player_position().y, 0.5f);
}

// a dodge toward the roof edge leaps off it, lands near the tuned distance and walks on
TEST_F(TraversalTest, LeapOffRoofLandsNearTunedDistance)
{
    player_ = build(1.2f, EAST_WALL_Z);
    ASSERT_NE(player_, nullptr);
    place_on_roof(player_, 1.2f, EAST_WALL_Z);
    step(SETTLE_FRAMES);
    const auto takeoff = player_position();
    ASSERT_GT(takeoff.y, ROOF_TOP - 0.1f);

    PlayerInput leap = moving_right();
    leap.dodge = true;
    step_until(leap, 1, [] { return true; });
    EXPECT_EQ(player_->movement_mode(), MovementMode::eAirborne);

    // no input in the air: the leap keeps its speed
    const auto landed = step_until({}, 180, [&] { return player_->movement_mode() == MovementMode::eGrounded; });
    ASSERT_LE(landed, 180u);
    const auto landing = player_position();
    EXPECT_LT(landing.y, ON_FLOOR_MAX_Y);
    const float distance = landing.x - takeoff.x;
    EXPECT_NEAR(distance, project_c::default_traversal_tuning().leap_distance, 0.4f);
    EXPECT_FALSE(went_through(MovementMode::eClimbing));

    // walks again after the landing
    step(SETTLE_FRAMES);
    const auto before_walking = player_position();
    step_until(moving_left(), 30, [] { return false; });
    EXPECT_LT(player_position().x, before_walking.x - 0.3f);
}

// the leap is aimed with the scene's gravity: with a weaker one it still lands near the tuned distance
TEST_F(TraversalTest, LeapUsesTheSceneGravity)
{
    player_ = build(1.2f, EAST_WALL_Z);
    ASSERT_NE(player_, nullptr);
    const float low_gravity[3] = { 0.0f, -5.0f, 0.0f };
    enginePhysicsSetGravityVector(low_gravity);
    place_on_roof(player_, 1.2f, EAST_WALL_Z);
    step(SETTLE_FRAMES);
    const auto takeoff = player_position();

    PlayerInput leap = moving_right();
    leap.dodge = true;
    step_until(leap, 1, [] { return true; });
    const auto landed = step_until({}, 240, [&] { return player_->movement_mode() == MovementMode::eGrounded; });
    ASSERT_LE(landed, 240u);
    EXPECT_NEAR(player_position().x - takeoff.x, project_c::default_traversal_tuning().leap_distance, 0.4f);
}

// running across the roof off its edge falls (no climb down) and lands below
TEST_F(TraversalTest, RunningOffRoofEdgeFallsAndLands)
{
    player_ = build(-1.5f, EAST_WALL_Z);
    ASSERT_NE(player_, nullptr);
    place_on_roof(player_, -1.5f, EAST_WALL_Z);
    step(SETTLE_FRAMES);

    const auto landed = step_until(moving_right(), 240, [&]
        {
            return went_through(MovementMode::eAirborne) && player_->movement_mode() == MovementMode::eGrounded;
        });
    ASSERT_LE(landed, 240u);
    EXPECT_FALSE(went_through(MovementMode::eClimbing));
    EXPECT_FALSE(went_through(MovementMode::eScriptedMove));
    EXPECT_LT(player_position().y, ON_FLOOR_MAX_Y);
    EXPECT_GT(player_position().x, EAST_WALL_FACE_X);
    // a fall from the roof is a hard landing: loud
    EXPECT_TRUE(player_->noise_is_loud());
}

// letting go of the wall near its bottom is a short fall, not a hard landing, however high the climb started
// (regression: the fall was measured from where the player left the ground, the roof)
TEST_F(TraversalTest, LettingGoNearTheBottomOfAWallIsQuiet)
{
    player_ = build(EAST_WALL_FACE_X - 0.35f, EAST_WALL_Z);
    ASSERT_NE(player_, nullptr);
    place_on_roof(player_, EAST_WALL_FACE_X - 0.35f, EAST_WALL_Z);
    step(SETTLE_FRAMES);
    const auto climbing = step_until(moving_right(), 60, [&] { return player_->movement_mode() == MovementMode::eClimbing; });
    ASSERT_LE(climbing, 60u);

    // down the wall (away from it) to under a metre above the floor
    const auto low = step_until(moving_right(), 180, [&] { return player_position().y < 0.9f; });
    ASSERT_LE(low, 180u);
    ASSERT_EQ(player_->movement_mode(), MovementMode::eClimbing);
    PlayerInput let_go{};
    let_go.dodge = true;
    bool loud = false;
    step_until(let_go, 1, [] { return true; });
    const auto landed = step_until({}, 120, [&] { return went_through(MovementMode::eAirborne) && player_->movement_mode() == MovementMode::eGrounded; });
    ASSERT_LE(landed, 120u);
    // the landing's frame and the next
    for (int frame = 0; frame < 3; frame++)
    {
        loud = loud || player_->noise_is_loud();
        step();
    }
    EXPECT_FALSE(loud);
    EXPECT_LT(player_position().y, ON_FLOOR_MAX_Y);
}

// walking from standstill at the roof edge toward it climbs down onto the wall instead
TEST_F(TraversalTest, WalkingSlowlyToRoofEdgeClimbsDown)
{
    player_ = build(EAST_WALL_FACE_X - 0.35f, EAST_WALL_Z);
    ASSERT_NE(player_, nullptr);
    place_on_roof(player_, EAST_WALL_FACE_X - 0.35f, EAST_WALL_Z);
    step(SETTLE_FRAMES);

    const auto climbing = step_until(moving_right(), 60, [&] { return player_->movement_mode() == MovementMode::eClimbing; });
    ASSERT_LE(climbing, 60u);
    EXPECT_FALSE(went_through(MovementMode::eAirborne));
    // hanging on the wall below the edge, outside the house
    EXPECT_LT(player_position().y, ROOF_TOP);
    EXPECT_GT(player_position().x, EAST_WALL_FACE_X);
}

// a roof higher than the safe drop is climbed down even when running at its edge
TEST_F(TraversalTest, DropAboveSafeDropClimbsDownWhenRunning)
{
    const float ground_y = project_c::default_traversal_tuning().max_safe_drop - ROOF_TOP + 1.0f;
    player_ = build(-1.5f, EAST_WALL_Z, ground_y);
    ASSERT_NE(player_, nullptr);
    place_on_roof(player_, -1.5f, EAST_WALL_Z);
    step(SETTLE_FRAMES);
    ASSERT_GT(player_position().y, ground_y + ROOF_TOP - 0.1f);

    const auto climbing = step_until(moving_right(), 180, [&] { return player_->movement_mode() == MovementMode::eClimbing; });
    ASSERT_LE(climbing, 180u);
    EXPECT_FALSE(went_through(MovementMode::eAirborne));
    EXPECT_GT(player_position().y, ground_y);
}

// an attack next to a wall never turns into a climb while it plays, however the input pushes into the wall
TEST_F(TraversalTest, NoTraversalWhileAttackingNextToWall)
{
    player_ = build(EAST_WALL_FACE_X + 0.25f, EAST_WALL_Z);
    ASSERT_NE(player_, nullptr);
    step(SETTLE_FRAMES);

    PlayerInput attack = moving_left();
    attack.attack = true;
    step_until(attack, 1, [] { return true; });
    // the swing (its exit time is ~1 s of animation, played faster)
    step_until(moving_left(), 40, [] { return false; });
    EXPECT_FALSE(went_through(MovementMode::eClimbing));
    EXPECT_FALSE(went_through(MovementMode::eScriptedMove));
    EXPECT_LT(player_position().y, ON_FLOOR_MAX_Y);
}
