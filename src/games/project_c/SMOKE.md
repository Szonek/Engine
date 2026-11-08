# Smoke bomb

The player's escape and opener: a bomb thrown at the cursor whose cloud hides the player from the enemies' sight and
blinds the ones it bursts on. Everything is game code; how enemies notice the player is in `ENEMY_AWARENESS.md`,
executions are in `EXECUTIONS.md`.

| File                                   | What lives there                                                              |
|----------------------------------------|-------------------------------------------------------------------------------|
| `scripts/smoke.h`                      | `SmokeTuning`, `cloud_radius`, `segment_touches_sphere`, `smoke_landing`. Engine-free, unit tested (`smoke_tests.cpp` in `engine_logic_tests`) |
| `scripts/player_smoke.h/.cpp`          | `PlayerSmoke`: the cooldown, where a throw lands, the cloud it spawns; owned by `Player` |
| `scripts/smoke_cloud.h/.cpp`           | `SmokeCloud`: the cloud (a runtime object), its pop, its look, `utils::smoke_blocks` |
| `scripts/player_script.h/.cpp`         | The throw (E): when it may start, the Throw clip and its release event       |
| `scripts/enemy_script.h/.cpp`          | `Enemy::blind`, `hear_loud_noise`, sight blocked by clouds                     |
| `scripts/enemy_awareness.h`            | `EnemyAwareness::lose_track`                                                  |
| `scripts/gameplay_hud_script.cpp`      | "Smoke (E): ready" or the seconds left, under the momentum bar                |
| `assets/data/player/smoke_bomb.json`   | `SmokeTuning` (`PlayerConfig::smoke`)                                          |

## The throw

**E** (`PlayerInput::smoke`) throws, on the ground only, not during an attack, a dodge, a hit stun, a landing, a kick
or another throw, and only when the cooldown is ready (`cooldown_seconds`, 12 s from the throw; momentum doesn't
change it). The bomb lands at the point under the cursor, or at `throw_range` (8 m, flat) from the player in that
direction (`smoke_landing`), on the first level geometry below that point (a roof under the cursor puts it on the roof).
It pops even behind a wall. The player turns that way and plays `Throw` on the locomotion layer, standing still; the
bomb leaves the hand `throw_release_seconds` (0.35) into the throw, where its cloud appears, and the player acts again
`throw_recovery_seconds` (0.25) later (the clip's long follow-through is cut). A hit before the release cuts the
throw short: no cloud, and the cooldown stays.

## The cloud

`SmokeCloud` is a runtime object (transient, never saved), like a coin. Several can exist. It is a sphere centred
`centre_height` (1 m) above where the bomb landed, whose radius grows to `radius` (3 m) in `grow_seconds` (0.25),
holds, and shrinks to nothing over the last `fade_seconds` (0.5) of its `duration_seconds` (6 s) life
(`cloud_radius`); then it goes. Its placeholder look is three grey wire spheres (its own, and two smaller puffs
turning slowly around its centre).

**Sight.** An enemy's sight ray (its eyes to the player's chest) that touches a cloud (`segment_touches_sphere`
against its current radius) is blocked, like by a wall: with the player inside, the enemy inside, or a cloud between
them (`utils::smoke_blocks`, asked by `Enemy::sees` with the level geometry ray; nothing is asked while there is no
cloud). Hearing isn't blocked.

**The pop**, when the cloud starts:

* every enemy within its full radius (measured to its chest) is **blinded** for `blind_seconds` (3 s): it plays `Hit_B`,
  then idles, can't move, turn or attack, and perceives nothing: no sight, no hearing and no pack (a mate who sees
  the player neither keeps nor makes it alert, and it tells its pack nothing). It **loses track** of the player
  (`EnemyAwareness::lose_track`): an alert enemy searches, from the cloud's centre, as if it hadn't perceived the
  player for a long time, so it doesn't keep its pack alert. A hit, or an execution it survives (an enemy type whose `execution_kills` is false), ends the blindness (and alerts
  it). While blinded
  it can be **executed from any side** (`Candidate::any_side`, like a knocked-down enemy). After the blindness it
  searches, and its pack can alert it again by the usual rules. A dying, executed, falling or knocked-down enemy only loses track.
* it is a **loud noise** at its centre, heard once by every enemy within `noise_radius` (5 m) which hasn't noticed the
  player (unaware or suspicious, outside the cloud): its meter fills by its `loud_noise_fill`. It alerts nobody by
  itself.

## Tests

* `src/tests/unit_tests/smoke_tests.cpp`: the cloud's radius over its life, lines of sight through, inside, touching
  and missing a cloud, the throw range. `enemy_awareness_tests.cpp`: `lose_track`.
* `src/tests/scenario_tests/smoke_tests.cpp`: an alert enemy loses the player and searches the cloud; a cloud between
  them hides the player until it fades; a blinded enemy doesn't attack and is executed from the front; a blinded pack
  member stays blinded while a mate sees the player; a hit ends the blindness; the cooldown; the pop's noise radius; a throw beyond the range.
