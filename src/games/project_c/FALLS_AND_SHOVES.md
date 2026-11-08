# Falls, shoves and momentum

What makes height and speed pay off: the kick which sends enemies off roofs, the fall damage that kills them there
(and hurts the player), and the momentum meter that stealthy kills fill. Everything is game code; executions are in
`EXECUTIONS.md`, how enemies notice the player in `ENEMY_AWARENESS.md`.

| File                                        | What lives there                                                           |
|---------------------------------------------|----------------------------------------------------------------------------|
| `scripts/fall_damage.h`                     | `FallDamage` (a curve), `fall_damage_fraction`, `FallMeasure` (the drop from the fall's highest point). Engine-free, unit tested (`fall_damage_tests.cpp`) |
| `scripts/momentum.h`                        | `MomentumTuning`, `MomentumEvent`, `PlayerMomentum`. Engine-free, unit tested (`momentum_tests.cpp`) |
| `scripts/player_shove.h/.cpp`               | `PlayerShove`: the kick's target search and its push, owned by `Player`    |
| `scripts/player_script.h/.cpp`              | The kick (Q), the player's fall damage and roll, momentum's fills, drains and effects |
| `scripts/enemy_script.h/.cpp`               | `receive_shove`, the Staggered and Falling states, landing (damage, knockdown or death, the landing noise) |
| `scripts/gameplay_hud_script.cpp`           | The momentum bar under the health bar                                      |
| `scripts/gizmos.cpp`                        | The kick's reach, in front of the player (`player_config` gizmo)           |
| `assets/data/player/tuning.json`            | `shove_*`                                                                  |
| `assets/data/player/traversal.json`         | `fall`, `roll_window_seconds`                                              |
| `assets/data/player/momentum.json`          | `MomentumTuning` (`PlayerConfig::momentum`)                                |
| `assets/data/enemies/skeleton_warrior.json` | `fall`                                                                     |

## Shove

**Q** (`PlayerInput::shove`) kicks, on the ground only, with or without a weapon, whatever the enemies know, but not
during an attack, a dodge, a hit stun, a landing or an execution, and not again before `shove_cooldown_seconds` (0.8).
The target is the nearest enemy (an overlap with the characters' collision layer) within `shove_range` (1.2 m, flat,
0.5 m of height) and `shove_half_angle_degrees` (50) of the player's facing; the player turns to it and holds that
facing during the kick. With nobody there the kick still plays (a miss).

The kick is `Unarmed_Melee_Attack_Kick` on the locomotion layer (the upper body layer is off, like a dodge); the foot
lands at its animation event (`PlayerShove::KICK_CONTACT_SECONDS`). The kick is a loud noise for one frame. At contact
the enemy gets `Enemy::receive_shove`: a push of `shove_speed` (7 m/s) in the player's facing, decaying exponentially
(`ENEMY_KNOCKBACK_DECAY`, 4/s: about 1.75 m on flat ground), `shove_damage` (0) and a stagger of
`shove_stagger_seconds` (0.6 s, `Hit_A`: no moving, no attacking). A wall or another character stops the push (the
character controller slides it); there's no wall slam damage. An enemy which hadn't noticed the player is alerted
once the stagger ends or it survives a landing; a fall death leaves it quiet for its pack (no killing blow alarm), but
the body's landing is still loud.

## Falls

One rule for everyone (`fall_damage_fraction`): a fall of `drop` metres takes 0 of the max HP up to the curve's
`min_height`, rising linearly to all of it at `lethal_height`; a roll halves it. The drop is from the fall's highest
point (a leap counts from its top) to the landing.

| Who | min_height | lethal_height | |
|---|---|---|---|
| Skeleton warrior (`EnemyType::fall`) | 1 m | 2 m | a kick off the house roof (2.25 m) kills it, off a 1.5 m ledge it loses half its HP |
| Player (`TraversalTuning::fall`) | 3 m (the traversal's `max_safe_drop`) | 8 m | a fall from 5.5 m costs half the HP |

**Enemies** fall when their character controller has had nothing under them for 0.1 s (`ENEMY_FALL_DELAY_SECONDS`, so
a step down isn't a fall): the Falling state (`Jump_Idle`, no AI, the push kept with a little air drag). On landing a
damaging drop hurts it; a lethal one kills it (the usual death), otherwise it is knocked down (lies, stands up
alerted). A damaging landing is a loud noise where the body lands, which the enemies around hear like the player's
loud noises (one update of each: `Enemy::landing_noise`). A smaller drop (being spawned above the floor) does nothing.

**The player** is hurt by the falls it can't avoid: letting go of a wall high up, being knocked off a roof, a leap past
a high edge (the traversal never chooses an unsafe drop by itself). The damage uses the usual hit reaction and camera
shake, without a push; a lethal one plays the death. **The roll**: a dodge pressed within `roll_window_seconds` (0.25 s)
before landing halves the damage and is the dodge, instead of the landing's recovery. **The plunge** is an attack: its
landing costs nothing.

## Momentum

A meter from 0 to 1 (`PlayerMomentum`), runtime state, never saved, empty at the start of every level.

| Fills it | |
|---|---|
| an execution from behind | +0.25 |
| a plunge | +0.35 |
| each further execution of a chain (in place of the 0.25) | +0.15 |
| an enemy the player kicked dying from its fall | +0.25 |

It stops at 1. At a full meter the player moves 25% faster (`speed_bonus`, linear in between; above the traversal's
run speed it runs up walls and off edges sooner) and the chain reaches 50% farther (`chain_range_bonus`). It drains at
`combat_drain_per_second` (0.15/s) on the ground in combat, which is an alert enemy within `combat_radius` (8 m) and
within 1 m of the player's height (one below a roof the player stands on doesn't count), and by `hit_drain` (0.2) for
each hit taken (not for a fall). Out of combat, on walls, in the air and above the fight it holds.

The HUD shows it as a thin bar under the health bar, brighter while it drains. Nothing else shows it.

## Tests

* `src/tests/unit_tests/fall_damage_tests.cpp`, `momentum_tests.cpp`: the rules.
* `src/tests/scenario_tests/shove_tests.cpp`: enemies falling from the roof's height and a low ledge, a small drop, a
  fall death heard; the kick on flat ground, a miss, off a roof and a low ledge, an unaware enemy alerted, a wall; the
  player falling 6 m, rolling, a safe drop, the plunge from the same height; momentum from an execution and the faster
  run, holding out of combat, draining in ground combat.
