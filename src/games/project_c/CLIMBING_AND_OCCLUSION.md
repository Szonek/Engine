# Climbing and camera occlusion

How the player gets around buildings (run up and climb walls, vault through windows, walk on roofs, leap and drop off
them) and how the camera keeps the player visible when geometry is in the way.

| File                                   | What lives there                                                              |
|----------------------------------------|-------------------------------------------------------------------------------|
| `scripts/player_script.h/.cpp`         | `Player`: the movement modes and their transitions (`set_movement_mode`), combat |
| `scripts/player_traversal.h/.cpp`      | `PlayerTraversal`: probes, plans, climbing, scripted moves, run-up, leap, falling, marker |
| `scripts/traversal_math.h`             | Pure maths, unit tested: move paths, leap launch, air control, the action an obstacle gets |
| `assets/data/player/traversal.json`    | `TraversalTuning`: the designer values (speeds, durations, heights, distances) |
| `scripts/building_script.h/.cpp`       | `BuildingPart` (what can be climbed) and `House` (the test building, roof cutaway) |
| `scripts/camera_script.h/.cpp`         | Camera follow and mouse look-ahead                                            |
| `assets/scenes/test_level.json`        | The level: the house, its parts and the chests on and in it                   |

Everything is game code, the engine is not changed. The traversal's designer values are the `traversal_tuning` data asset
(`TraversalTuning` in `game_components.h`, `assets/data/player/traversal.json`, referenced by the player's
`player_config`, read on every use, so an edit while playing takes effect at once). The probe geometry (heights, offsets,
ranges) stays `constexpr` at the top of `player_traversal.cpp`, the camera's and the house's at the top of
`camera_script.cpp` and `building_script.cpp`.

## Controls

There is no climb button, everything starts by walking into things:

| Situation                                  | What happens                                                   | Marker |
|--------------------------------------------|----------------------------------------------------------------|--------|
| Run into a wall                            | Run-up: up the wall and over its edge in one move (~0.5 s) when the top is within `run_up_max_height`; a higher wall is run up that far, then climbed | yellow |
| Walk into a wall from right next to it     | Start climbing, once the key pushed into the wall for `climb_intent_seconds` | yellow |
| Walk into a window                         | Vault through it (both directions)                             | green  |
| Run toward the edge of a roof              | Run off it and fall, when the drop is at most `max_safe_drop`; a higher edge is climbed down | cyan |
| Walk toward the edge of a roof             | Step over the edge and hang on the wall below                  | cyan   |
| Dodge (**Space**) toward an edge           | Leap: a jump of `leap_distance` (to the ground below), `leap_height` high | |
| On a wall: key **toward** the wall         | Climb up. At the top the player pulls up onto the roof         |        |
| On a wall: key **away** from the wall      | Climb down. At the ground the player lets go                   |        |
| On a wall: the other two keys              | Shimmy sideways along the wall                                 |        |
| On a wall: **Space**                       | Let go and fall                                                |        |

Running means approaching at `run_min_speed` or faster; it is decided when the player reaches the obstacle, so speeding
up while pushing into it doesn't change what happens. The player runs whenever a movement key is held for a moment (it
reaches its speed in ~0.1 s), so walking slowly happens when it starts right next to the wall or edge.

Nothing starts while the player attacks, dodges, is stunned or lands, and a climb needs the key held into the wall for
the intent time, so brushing a wall mid-fight never starts a climb.

On a wall WASD keeps its ground meaning, so the key that walked the player into the wall also climbs it.
I.e. a wall on the right side of the screen is climbed by holding D, the front wall by holding W.

## Traversal

### Who can be climbed

Traversal is driven by short physics raycasts ("probes") from the player. A probe only counts if it hits a collider owned
by a `BuildingPart` script, so floors, chests, enemies etc. are never climbed. A `BuildingPart` is climbable when its
`building_part_config` component says `climbable`; the house walls also set `climbable_from_outside_only`, which allows
climbing only faces pointing away from the part's parent (the house). Otherwise the player could climb an inner wall and
pull up through the roof.

Probe heights are measured from the player's feet (the transform origin, player is ~0.85 tall):

```
 0.95  head   - is there still wall above me?
 0.70  chest  - wall in front?            (vault: must be free)
 0.30  knee   - low obstacle in front?    (vault: must hit)
```

### Detection: `find_traversal` -> `TraversalPlan`

Every frame on the ground (not attacking, dodging, stunned or landing) and while there is movement input,
`PlayerTraversal::update_grounded` calls `find_traversal`, which probes in the movement direction up to
`TRAVERSAL_DETECT_RANGE` (1.3) and returns a `TraversalPlan`:

* **Vault**: knee probe hits a building face, chest probe hits nothing (or something further away).
  `plan_vault` then measures the obstacle: casts back from the far side to get its depth, casts down for its top, checks that
  the body fits above it (window opening) and that there is ground on the other side. For window frames the player is
  aligned to the opening centre (`BuildingPart::get_alignment_point`).
* **Climb**: chest probe hits a climbable face.
* **Climb down**: `find_edge` samples a few points ahead for missing floor (a roof edge) and measures the drop below it;
  `plan_climb_down` then casts back under the edge to find a climbable wall face below it.

Faces are only accepted when approached head-on (`TRAVERSAL_MIN_APPROACH_DOT`), so walking along a wall does not trigger anything.

The plan is used twice:

1. The **marker** (a small pulsing `Primitive_Cube_Small`) is drawn at `plan.marker`. When the player isn't walking toward
   anything, `find_nearest_traversal` scans 8 directions (45 degrees apart) so the marker stays visible whenever the player
   stands next to something traversable.
2. If the plan was found in the movement direction and `plan.distance <= TRAVERSAL_TRIGGER_DISTANCE` (0.35), i.e. the
   player reached the obstacle, `traversal_math::choose_ground_action` decides what happens from the obstacle, whether
   the player ran when it reached it, the drop behind an edge and how long the input has pushed into it (the table
   above), and `start_action` starts it: climb, run-up, vault, climb down, or nothing (running off an edge: the player
   walks on and falls).

A dodge toward an edge (`find_edge` finds one within 1.25 m in the movement direction) is a leap instead of a dodge.

After a traversal ends the player can't start a new one for `TRAVERSAL_REGRAB_COOLDOWN` (0.3 s, or
`TRAVERSAL_DROP_REGRAB_COOLDOWN` 0.8 s after letting go with Space), because the movement key is usually still held toward the wall.

### Movement modes

`MovementMode` (`player_traversal.h`), like the movement modes of Unreal's Character Movement Component. The player is in
exactly one; `PlayerTraversal`'s updates return the mode the player is in next and `Player::set_movement_mode` is the
only place it changes (leaving the ground ends the attack, dodge and stun in progress and mutes the combat layer;
landing plays `Jump_Land` and starts the landing recovery; back on the ground the layers are restored):

* **eGrounded** - walking with the character controller; combat happens only here. Walking into a wall -> eClimbing,
  running into it -> eScriptedMove (run-up, ending on the roof or in eClimbing), into a window -> eScriptedMove (vault),
  toward a roof edge -> eScriptedMove (climb down, ends in eClimbing), a dodge at an edge -> eAirborne (leap), not on the
  ground for `FALL_DELAY_SECONDS` (0.1) -> eAirborne (a fall: running off an edge, a dodge off it, ...).
* **eAirborne** - falling or leaping; landing -> eGrounded.
* **eClimbing** - on a wall. Climbing past the top -> eScriptedMove (mantle, ends in eGrounded on the roof);
  reaching the ground or Space -> eGrounded.
* **eScriptedMove** - run-up / vault / mantle / climb down in progress, input is ignored until it ends.

In more detail:

* **eClimbing**: each frame the input is projected onto the wall: toward the wall = up, along the wall = sideways.
  A move is accepted only if the wall is still there (probes at the new position; for sideways moves at the leading side of
  the body, so the player can't hang over a corner). The player is then snapped to `CLIMB_WALL_OFFSET` (0.2) from the wall surface.
  When the head probe stops hitting the wall while climbing up, `try_start_mantle` looks for a ledge behind the wall top
  (with standing room above it and nothing above the player on the way up).
* **eScriptedMove**: plays a `TraversalMove` - a fixed path from `p0` through `p1` to `p2`
  (`traversal_math::evaluate_move_path`). Vaults use a quadratic curve through `p1`, mantle, run-up and climb down use
  two straight segments (up then in, out then down) so the body doesn't cut the ledge corner. The clip playback rate is
  stretched to the move's duration.
* **Run-up** (`plan_run_up`): the mantle's ledge probe looks for the top of the wall from `run_up_max_height` down. With a
  ledge and standing room on it, one move goes up the wall (70% of `run_up_duration_seconds`, `Running_A` sped up) and
  over the edge (the rest, `Jump_Full_Short`). Without one within reach, the move ends on the wall with the hands at the
  run-up height and climbing starts. When the wall doesn't go that high (or something is above the way up) the player
  just starts climbing.
* **eAirborne**: the character controller moves the player with the horizontal air velocity and, after a leap, the
  launch's vertical speed (the controller adds the fall: its gravity), so a leap is a ballistic arc
  (`traversal_math::leap_launch` gives the launch from `leap_distance` and `leap_height`, aimed at the ground below the
  edge). The input steers it with `air_control` times the ground acceleration; nothing brakes in the air. A wall it hits
  takes its speed. It lands when the controller reports it grounded; there is no fall damage.

### Physics while traversing

The player walks with the engine's character controller (`engineCharacterControllerMove`, which also applies gravity).
While climbing or in a scripted move, the traversal code sets the player's transform and doesn't call the controller, so
nothing pulls the player off the wall; its capsule is kinematic and follows the transform, so enemy hitboxes still detect
it. Walking and falling resume with the next move after the traversal ends. If the player dies on a wall, traversal is
stopped and they fall.

### Animations

The KayKit character pack has no climbing clips, so the closest ones are used: `Jump_Idle` while on the wall (slowed down
when not moving) and in the air, `Jump_Full_Short` for vaults, pulling up and the end of a run-up, `Running_A` sped up
for running up a wall, `Jump_Start` for a leap's takeoff and `Jump_Land` for landings. They are constants at the top of
`player_traversal.cpp` (`Jump_Land`: `player_script.cpp`). The combat (upper body) layer is muted off the ground and
while landing.

## The house

`House` is built by `build_house` (`LevelBuilder::house`) and saved in the level file: the house's game object stands at
the center of its floor, and its parts are level content placed relative to it, `BuildingPart` children with a
`building_part_config` component, which the house collects in `on_start`. Its floor tiles have a not-walkable nav
modifier, so the nav mesh leaves them out and enemies don't walk into it. Enemies also ignore a player who is more than 0.6 higher than them (on a wall or the roof).

* Walls are KayKit `Primitive_Wall` pieces at half size: 2 wide, 2 tall, 0.5 thick. The roof (`Primitive_Floor`) sits on
  top, flush with the outer faces, so the top of a climbed wall is the roof edge.
* The engine's compound collider supports one child only, so the window wall's collision is four invisible blockers
  (left/right pillar, sill, lintel) sharing the transform of the visible `Primitive_Window` piece. The blockers align
  traversal (vault) to their position (`align_to_position`), the one of the visible piece.
* While the player is inside, the parts whose config has a `hide_when_inside` (the roof and the wall facing the camera)
  are hidden - see Occlusion below.

## Camera

`CameraScript` (orthographic, 45 degree pitch, no yaw) does two things every `late_update`:

1. **Follow**: the look-at point follows the player with a critically damped spring - `FOLLOW_SMOOTH_TIME_HORIZONTAL`
   (0.1 s) keeps combat tight, `FOLLOW_SMOOTH_TIME_VERTICAL` (0.3 s) softens climbs and drops.
2. **Look-ahead**: shifts the view toward the mouse cursor (cursor projected on the ground plane at the player's height),
   `LOOK_AHEAD_FRACTION` of the distance, clamped to `LOOK_AHEAD_MAX_DISTANCE`, smoothed. The cursor's world position
   depends on the camera itself, the effective shift settles at `FRACTION / (1 - FRACTION)` (~25%) of the cursor offset.

## Occlusion: roof cutaway

The only occlusion handling is the classic ARPG cutaway of the house, all in `House::update`:

* `House::is_inside` checks the player against the interior of the house (below the roof).
* When the player enters, the roof is hidden (`mesh_and_collider`: mesh disabled, collider disabled, so mouse
  aiming hits the room floor instead of the roof; the collider stays, so a scene saved meanwhile keeps it) together with the props standing on it (the `roof_props` of the house's
  `house_config`, `House::add_roof_prop`), and the camera facing wall pieces are hidden (`mesh`: their colliders stay, so
  the walls still block movement and can be vaulted through).
* When the player leaves, everything is shown again. The house looks at its parts' components, not at what it did
  before: a scene saved while the player was inside loads with the parts hidden, and the house's first update shows
  them (or keeps them hidden). A part whose script is replaced or removed is shown again.

Outside the house nothing reacts: a player behind the house or climbing its back wall is hidden by it. That was a
deliberate choice for the POC - fading or lowering occluders was tried (see the PR history) and dropped for now. The
intended future route is a per-material dithered fade once the engine has a material system, or a player silhouette
render feature.

## Adding things

* **A new climbable object**: give it a collider and a `BuildingPart` script, with `climbable` set in its
  `building_part_config`. Vault/mantle need a flat top (normal up) and enough room above it for the player.

## Known limitations

* No climbing animations (see Animations).
* Walking slowly only happens when the player starts right next to a wall or edge (there is no walk key).
* No fall damage: any fall lands with the same short recovery.
* The house is a fixed 4x4 one-room layout; the `h` block only positions it.
* Only the house interior is handled; the player can be hidden behind the house (see Occlusion).
* Traversal rays are cast every frame (a few dozen per frame), fine for this scene but not budgeted for
  large levels.
