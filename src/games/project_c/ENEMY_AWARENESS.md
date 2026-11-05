# Enemy awareness

How enemies notice the player and what they do about it: sight, hearing, packs and sneaking. Everything is game code.

| File                                         | What lives there                                                          |
|----------------------------------------------|---------------------------------------------------------------------------|
| `scripts/enemy_awareness.h`                  | The model: `EnemyPerception`, `Awareness`, `EnemyStimuli`, `EnemyAwareness` (meter and state machine), `in_sight_cone`, `sight_fill_seconds`. Engine-free, unit tested (`enemy_awareness_tests.cpp`) |
| `scripts/enemy_script.h/.cpp`                | `Enemy::perceive` (the stimuli of a frame) and the behaviour per state    |
| `scripts/enemy_spawner_script.h/.cpp`        | The pack: `EnemySpawner::pack_state`, computed from its members           |
| `scripts/player_script.h/.cpp`               | Sneaking and the player's noise (`Player::noise_radius`, `noise_is_loud`) |
| `scripts/noise_rings.h`                      | The rings showing the noise on the ground (engine-free, unit tested)     |
| `scripts/gameplay_hud_script.cpp`            | The `?` / `!` above each enemy's health bar                               |
| `scripts/gizmos.cpp`                         | The sight cone gizmo of the `enemy_config` component                      |
| `assets/data/enemies/skeleton_warrior.json`  | `perception`: the enemy type's sight, hearing and memory values           |
| `assets/data/player/tuning.json`             | `sneak_speed`, `footstep_noise_radius`, `loud_noise_radius`, `show_footstep_ring`, `footstep_ring_interval_seconds` |

## States

Each enemy has a detection meter from 0 to 1 and one state (`EnemyAwareness::update` holds every transition):

| State        | Icon | Enters when                                   | Does                                                         |
|--------------|------|-----------------------------------------------|--------------------------------------------------------------|
| Unaware      |      | the meter decayed to 0                        | stands                                                       |
| Suspicious   | `?`  | the meter is above 0                          | turns to what it noticed and walks there (`suspicious_move_speed`) |
| Alert        | `!`  | the meter is full, it is hit, or its pack is alert | chases and attacks: the player while it perceives it, otherwise its last known position |
| Searching    | `?`  | alert and it lost the player                  | walks to the last known position and looks around            |

An alert enemy which stops perceiving the player keeps hunting it where it is for `lose_sight_grace_seconds` (0.5), so a
frame without line of sight (another enemy passing between them) doesn't stop a chase; then it searches. Searching ends after `lose_seconds` (back to Suspicious, with a full meter which then decays) or when the player is
perceived again (Alert). The meter stays full while Alert or Searching and decays only when nothing is perceived (a full
meter empties in `meter_decay_seconds`).

## Senses

* **Sight**: the player is in view within `sight_range` and `sight_half_angle_degrees` of the enemy's facing (flat), or
  within `close_range` in any direction. Then one ray cast from the enemy's eyes (`eye_height`) to the player's chest
  must hit the player first, so walls hide the player. While seen, the meter fills in `detect_seconds_near` (right next
  to it) to `detect_seconds_far` (at `sight_range`), linear in between; a player above the enemy's reach (on a roof)
  fills it `above_sight_multiplier` times as fast (0.35: about three times as slow).
* **Hearing**: the player's noise of the frame (`Player::noise_radius`), which walls don't block: 0 while sneaking,
  standing or off the ground, `footstep_noise_radius` while walking, `loud_noise_radius` for one frame on an attack, a
  dodge or a landing from 1.5 m or higher. Heard footsteps fill the meter in `hearing_fill_seconds`, a loud noise adds
  `loud_noise_fill` at once. The noise shows on the ground, like Mark of the Ninja's (`scripts/noise_rings.h`, unit
  tested): while the player walks, a faint ring at the footstep radius pulses with the steps (every
  `footstep_ring_interval_seconds`, `show_footstep_ring` turns it off); while it sneaks or stands there is none; a loud
  noise shows a bright double ring of its radius for 0.3 s.
* **Damage**: a hit alerts the enemy at once (also during its hit stop, and a killing blow), with the player's position as
  the last known position.

Enemies read the player (scripts read each other, nothing reports), so the order the scripts update in doesn't matter:
a loud noise is seen by every enemy exactly once.

## Packs

The enemies an `EnemySpawner` spawned are a pack. Each enemy knows its spawner's game object (`Enemy::set_pack`, set
when spawned, runtime state, never saved) and reads its `pack_state(reader)`, which the spawner computes from its other members
when asked: alert when one of them is, the newest last known position, and the time since one of them perceived the
player itself. The reader's own perception doesn't count, so a pack member forgets like a placed enemy. A dying member leaves the pack, but
its killing blow alerts the rest of it (for 0.2 s, with the player's position then), so a one-hit kill still alerts the
pack and a corpse never keeps it alert.
An enemy of an alert pack whose members perceived the player within `lose_seconds` is alerted too (a frame later at
most), so a pack calms down only once none of them has perceived the player for `lose_seconds`. A placed enemy, or one
whose spawner is gone, is a pack of one.

## Sneaking

Holding Left Shift (`PlayerInput::sneak`) moves the player at `sneak_speed` and makes no footstep noise. It is below the
traversal's `run_min_speed`, so a sneaking player climbs walls instead of running up them.

## Editor

The `enemy_config` component's gizmo draws the sight cone (its edges and the arc at `sight_range`) and the `close_range`
circle, coloured by the enemy's state while it plays (grey unaware or in Edit mode, yellow suspicious, red alert, orange
searching), brighter when selected. The perception values are edited on the enemy type data asset, while playing too.
