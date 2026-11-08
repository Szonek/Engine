# Executions

How the player kills an enemy that hasn't noticed it: from behind (the ground execution), from above (the plunge, an
aerial assassination) and one after another (the chain). Everything is game code; how enemies notice the player is in
`ENEMY_AWARENESS.md`.

| File                                        | What lives there                                                           |
|---------------------------------------------|----------------------------------------------------------------------------|
| `scripts/execution_math.h`                  | The rules: `Candidate`, `Rules`, `executable`, `pick_target`, `execution_spot`, `predict_landing`, `move_seconds`, `knockdown_radius`, `ChainWindow`. Engine-free, unit tested (`execution_math_tests.cpp`) |
| `scripts/player_executions.h/.cpp`          | `PlayerExecutions`: finds the target, runs the execution and the plunge in `MovementMode::eExecuting`, keeps the chain window |
| `scripts/player_script.h/.cpp`              | When an execution may start, the strike's hit stop and noise, the plunge's landing, invulnerability, the debug cues |
| `scripts/enemy_script.h/.cpp`               | `begin_execution`, `receive_execution`, `knock_down` (the `KNOCKED_DOWN` state) |
| `scripts/gameplay_hud_script.cpp`           | The red `†` above the current target                                       |
| `scripts/gizmos.cpp`                        | The green arc behind each enemy (`enemy_config` gizmo)                     |
| `assets/data/player/executions.json`        | `ExecutionTuning`, the player's `execution_tuning` data asset (`PlayerConfig::executions`) |
| `assets/data/enemies/skeleton_warrior.json` | `execution_kills`                                                          |

## Ground execution

An attack press on the ground executes the nearest enemy (the candidates come from an overlap with the characters'
collision layer around the player, so floors and walls never crowd them out) which

* hasn't noticed the player: Unaware, or Suspicious while it doesn't see the player this frame;
* is within `execution_range` (1.4 m, flat) and 0.5 m of the player's height;
* has the player behind it: within `execution_behind_half_angle_degrees` (70) of its back, a 140° arc;
* is reachable: the line between the chests hits no level geometry (`utils::level_blocks`, one ray against the blocking
  collision layers, everything but characters, hitboxes and pickups, the same test as the enemies' sight);
* isn't dying or being executed.

A knocked-down enemy, or one blinded by a smoke bomb (`SMOKE.md`), can be executed from any side, whatever it knows. The player must be grounded and not dodging,
stunned or recovering from a landing; a press in the middle of a combo cancels the swing. Without a weapon the player
kicks (`Unarmed_Melee_Attack_Kick`), with the daggers it stabs (`Dualwield_Melee_Attack_Stab`). The target is computed
every frame (`Player::execution_target`); the HUD marks it and a press executes it, otherwise the press is a normal
attack.

The execution: the player lunges in `execution_lunge_seconds` (0.15) to `0.6 m` behind the victim, facing its back,
or, when a wall is right behind the victim, to `0.6 m` from it on the side the player comes from; the clip starts so
that it hits as the lunge ends, and the strike lands at the clip's hit time with a hit stop
(`execution_hit_stop_seconds`, 0.1) and a camera shake; it ends `execution_seconds` (0.55) after the clip started. The
victim is frozen from the start (`Enemy::begin_execution`), perceiving nothing (so it can't alert its pack), and the
player can't be hurt. The kill makes a noise of
`execution_noise_radius` (2.5 m) for one frame, heard like footsteps (not loud), so a packmate further away notices
nothing. The victim dies (`Death_B`) without alerting its pack (`was_just_killed`, the killing blow which alerts it, is false for
executions). An enemy type
with `execution_kills` false takes `execution_damage` (60) instead and is alerted.

## Plunge (aerial assassination)

An attack press in the air (`eAirborne`: any fall, the leap) at least `aerial_min_drop` (1 m) above the ground (level geometry: characters and dynamic bodies below don't
count) plunges
onto the enemy nearest where the player would land (`predict_landing`, from the velocity measured over the last frame and
the scene's gravity, at the height of each enemy's feet), within `aerial_aim_radius` (2.5 m), in any awareness state but
dying or knocked down, with no level geometry on the straight line to it (no plunge through a roof). The plunge is a straight line at `plunge_speed` (14 m/s, at least 0.12 s) to just short of the
target, in `eExecuting`; air control stops. At the impact the target is executed (`receive_execution`) and every other
enemy on its level (within 1 m of height) and within `knockdown_radius_per_metre` × drop (0.6, at most `knockdown_max_radius`, 3 m) is knocked down for
`knockdown_seconds` (1.5) and alerted. The landing is the hard one (a loud noise, `Jump_Land`, the landing recovery);
the player can't be hurt from the plunge's start until the recovery ends. With no target the press waits for the
landing, like any press in the air.

While airborne with a target, a red ring is drawn at the target's feet and a faint one for the knockdown radius
(always on, like the noise rings).

## Chain

After an execution, and after a plunge's landing recovery, the chain window is open for `chain_window_seconds` (1 s),
counting down in every movement mode (a climb or a fall doesn't keep it open).
A press in it executes the nearest enemy within `chain_range` (6 m) and 0.5 m of height which hasn't noticed the player
or is knocked down, and is reachable; no behind rule. The player dashes to its back at `chain_dash_speed` (12 m/s), then
strikes, and the window opens again; the chain has no cap. A press with no target is a normal attack and ends the chain.
While the window is open, the HUD marks the next target and a line leads to it.

## Momentum

Executions feed the player's momentum (`FALLS_AND_SHOVES.md`): an execution from behind fills it by 0.25, a plunge by
0.35, each further execution of a chain by 0.15 (in place of the 0.25). The chain's range grows with it, up to 50% more
at a full meter (`chain_range` times `PlayerMomentum::chain_range_multiplier`).

## Tests

* `src/tests/unit_tests/execution_math_tests.cpp`: the rules, target picking, landing prediction, the chain window.
* `src/tests/scenario_tests/execution_tests.cpp`: from behind (silent for a pack 5 m away), not from the front, not an
  alert enemy, not through a wall, invulnerability, unarmed, the plunge onto a pack and without a target, the chain and
  its window expiring, an enemy type which survives executions.
