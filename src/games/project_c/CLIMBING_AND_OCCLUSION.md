# Climbing and camera occlusion

How the player gets around buildings (climb walls, vault through windows, walk on roofs) and how the camera keeps
the player visible when geometry is in the way.

| File                                   | What lives there                                                              |
|----------------------------------------|-------------------------------------------------------------------------------|
| `scripts/player_script.h/.cpp`         | Traversal: detection, climbing, vault / mantle / climb-down moves, marker     |
| `scripts/building_script.h/.cpp`       | `BuildingPart` (what can be climbed) and `House` (the test building, roof cutaway) |
| `scripts/camera_script.h/.cpp`         | Camera follow and mouse look-ahead                                            |
| `scenes/scene_test.cpp`                | Map: the `h` block is the house footprint, `B` is a chest inside the house    |

Everything is game code, the engine is not changed. All tuning values are `constexpr` constants at the top of
`player_script.cpp`, `camera_script.cpp` and `building_script.cpp`.

## Controls

There is no climb button, everything starts by walking into things:

| Situation                                  | What happens                                                   | Marker |
|--------------------------------------------|----------------------------------------------------------------|--------|
| Walk into a wall                           | Start climbing                                                 | yellow |
| Walk into a window                         | Vault through it (both directions)                             | green  |
| Walk toward the edge of a roof             | Step over the edge and hang on the wall below                  | cyan   |
| On a wall: key **toward** the wall         | Climb up. At the top the player pulls up onto the roof         |        |
| On a wall: key **away** from the wall      | Climb down. At the ground the player lets go                   |        |
| On a wall: the other two keys              | Shimmy sideways along the wall                                 |        |
| On a wall: **Space**                       | Let go and fall                                                |        |
| On a roof: dodge (**Space**) at the edge   | Jump down without climbing - start the dodge more than 0.45 from the edge, closer the climb down starts first |        |

On a wall WASD keeps its ground meaning, so the key that walked the player into the wall also climbs it.
I.e. a wall on the right side of the screen is climbed by holding D, the front wall by holding W.

## Traversal

### Who can be climbed

Traversal is driven by short physics raycasts ("probes") from the player. A probe only counts if it hits a collider owned
by a `BuildingPart` script, so floors, chests, enemies etc. are never climbed. A `BuildingPart` is climbable if it was
created with `climbable = true`; the house walls additionally use `set_climbable_away_from(house_center)`, which allows
climbing only faces pointing away from the house. Otherwise the player could climb an inner wall and pull up through the roof.

Probe heights are measured from the player's feet (the transform origin, player is ~0.85 tall):

```
 0.95  head   - is there still wall above me?
 0.70  chest  - wall in front?            (vault: must be free)
 0.30  knee   - low obstacle in front?    (vault: must hit)
```

### Detection: `find_traversal` -> `TraversalPlan`

Every frame on the ground (not attacking, dodging or stunned) and while there is movement input, `find_traversal`
probes in the movement direction up to `TRAVERSAL_DETECT_RANGE` (1.3) and returns a `TraversalPlan`:

* **Vault**: knee probe hits a building face, chest probe hits nothing (or something further away).
  `plan_vault` then measures the obstacle: casts back from the far side to get its depth, casts down for its top, checks that
  the body fits above it (window opening) and that there is ground on the other side. For window frames the player is
  aligned to the opening centre (`BuildingPart::get_alignment_point`).
* **Climb**: chest probe hits a climbable face.
* **Climb down**: `plan_climb_down` samples a few points ahead for missing floor (a roof edge), then casts back under the
  edge to find a climbable wall face below it.

Faces are only accepted when approached head-on (`TRAVERSAL_MIN_APPROACH_DOT`), so walking along a wall does not trigger anything.

The plan is used twice:

1. The **marker** (a small pulsing `Primitive_Cube_Small`) is drawn at `plan.marker`. When the player isn't walking toward
   anything, `find_nearest_traversal` scans 8 directions (45 degrees apart) so the marker stays visible whenever the player
   stands next to something traversable.
2. If the plan was found in the movement direction and `plan.distance <= TRAVERSAL_TRIGGER_DISTANCE` (0.35), i.e. the
   player is pressed against the obstacle, `execute_traversal` starts it.

After a traversal ends the player can't start a new one for `TRAVERSAL_REGRAB_COOLDOWN` (0.3 s, or
`TRAVERSAL_DROP_REGRAB_COOLDOWN` 0.8 s after letting go with Space), because the movement key is usually still held toward the wall.

### States

`TraversalState`:

* **eNone** - normal locomotion. Walking into a wall -> eClimbing, into a window -> eScriptedMove (vault),
  toward a roof edge -> eScriptedMove (climb down, ends in eClimbing).
* **eClimbing** - on a wall. Climbing past the top -> eScriptedMove (mantle, ends in eNone on the roof);
  reaching the ground or Space -> eNone.
* **eScriptedMove** - vault / mantle / climb down in progress, input is ignored until it ends.

In more detail:

* **eClimbing**: each frame the input is projected onto the wall: toward the wall = up, along the wall = sideways.
  A move is accepted only if the wall is still there (probes at the new position; for sideways moves at the leading side of
  the body, so the player can't hang over a corner). The player is then snapped to `CLIMB_WALL_OFFSET` (0.2) from the wall surface.
  When the head probe stops hitting the wall while climbing up, `try_start_mantle` looks for a ledge behind the wall top
  (with standing room above it and nothing above the player on the way up).
* **eScriptedMove**: plays a `TraversalMove` - a fixed path from `p0` through `p1` to `p2`. Vaults use a quadratic curve
  through `p1`, mantle and climb down use two straight segments (up then in, out then down) so the body doesn't cut the ledge
  corner. The clip playback rate is stretched to the move's duration.

### Physics while traversing

The player is normally a dynamic rigid body moved by setting its transform. While traversing that doesn't work - gravity
keeps accumulating velocity and pulls the player off the wall. `set_kinematic(true)` therefore **removes the rigid body
component**: the collider becomes a static body that only follows the transform. `stop_traversal` adds the rigid body
back, which creates a fresh body with zero velocity. If the player dies on a wall, traversal is stopped and they fall.

### Animations

The KayKit character pack has no climbing clips, so the closest ones are used: `Jump_Idle` while on the wall (slowed down
when not moving) and `Jump_Full_Short` for vaults and pulling up. They are constants at the top of `player_script.cpp`.
The combat (upper body) layer is muted while traversing.

## The house

`House` is built over the bounding box of the `h` cells of the map. These cells get floor tiles with a not-walkable nav
modifier, so the nav mesh leaves them out and enemies don't walk into it. Enemies also ignore a player who is more than 0.6 higher than them (on a wall or the roof).

* Walls are KayKit `Primitive_Wall` pieces at half size: 2 wide, 2 tall, 0.5 thick. The roof (`Primitive_Floor`) sits on
  top, flush with the outer faces, so the top of a climbed wall is the roof edge.
* The engine's compound collider supports one child only, so the window wall's collision is four invisible blockers
  (left/right pillar, sill, lintel) sharing the transform of the visible `Primitive_Window` piece. The blockers point their
  alignment point (vault) to the visible piece.
* While the player is inside, the roof and the wall facing the camera are hidden - see Occlusion below.

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
* When the player enters, the roof is hidden (mesh disabled; collider removed, so mouse aiming hits the room floor instead
  of the roof) together with the props standing on it (`House::add_roof_prop`), and the camera facing wall pieces are
  hidden (`BuildingPart::set_hidden`; their colliders stay, so the walls still block movement and can be vaulted through).
* When the player leaves, everything is shown again.

Outside the house nothing reacts: a player behind the house or climbing its back wall is hidden by it. That was a
deliberate choice for the POC - fading or lowering occluders was tried (see the PR history) and dropped for now. The
intended future route is a per-material dithered fade once the engine has a material system, or a player silhouette
render feature.

## Adding things

* **A new climbable object**: give it a `BuildingPart` script (collider in mesh space through `BuildingPart::Box`,
  `climbable = true`). Vault/mantle need a flat top (normal up) and enough room above it for the player.

## Known limitations

* Walking toward a roof edge always climbs down; dropping off needs a dodge started a bit away from the edge, or letting go of the wall.
* No climbing animations (see Animations).
* The house is a fixed 4x4 one-room layout; the `h` block only positions it.
* Only the house interior is handled; the player can be hidden behind the house (see Occlusion).
* Traversal rays are cast every frame (a few dozen per frame), fine for this scene but not budgeted for
  large levels.
