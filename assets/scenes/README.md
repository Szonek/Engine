# Scene files

A scene file is a level (or any scene) as data: its game objects and their components, as JSON. The game loads its
level from `test_level.json` here (`LevelScene`; another file with `project_c_game --level <path>`, relative to this
folder or absolute). Files are read at runtime from the source tree's `assets`, so editing one needs no rebuild.

The engine writes them (`engineSceneSave`, `scene_serializer.cpp`) and reads them (`engineSceneLoad`). Until the editor
can save scenes, a level is changed by editing its file (or building it in code and saving it with `engineSceneSave`).
After a format change (a component gains a field), old files keep loading with the new field at its default; saving
the level writes the new format.

## Format

```json
{
  "version": 1,
  "types": { "transform": 1, "enemy_config": 1, ... },
  "entities": [
    { "id": 1, "components": { "name": { "name": "camera" }, "transform": { ... }, "camera": { ... }, "script": { "type": "camera" } } },
    { "id": 42, "prefab": "KayKit_Skeletons_1.0_FREE/characters/gltf/Skeleton_Warrior.glb",
      "components": { "name": { ... }, "transform": { ... }, "character_controller": { ... }, "enemy_config": { ... }, "script": { "type": "enemy" } },
      "nodes": { "Body/Hand": { "collider": { ... } } } }
  ]
}
```

- `version` is the file format's version; a file newer than the engine isn't loaded. `types` is the version of each
  component type the file holds; a component saved by a newer version is skipped.
- `entities` are the game objects, sorted by `id`, their **persistent ID**. Every reference to a game object (a parent,
  a game object field) is its persistent ID; `null` is none. Transient game objects (spawned at runtime, like the
  player) are never saved.
- `components` holds each component under its type's name with **every field**, defaults too. Enums are written by name,
  assets by their asset ID (`<model path under assets/models>#<name>`, or an image's path), floats as the shortest text
  that reads back exactly. A union (a light's or collider's data) writes only its active case, under the name of its
  type (`"type": "orthographic", "orthographic": { "scale": 5 }`).
- An entity with `prefab` is a **prefab instance**: the model (its path under `assets/models`) is instantiated again
  when the file is loaded, and only the root's name and transform and the components added to the instance are saved,
  on the root or under `nodes` by node path (the node names from the root down, joined by `/`). The game objects inside
  an instance have IDs hashed from the root's ID and their node path, so references to them survive a reload.
- `script` names the game object's script (a script type the game registered, or `none`).

The same scene always gives the same bytes, so files diff cleanly.

## Loading

The whole file is checked before anything changes: broken JSON, no version or a newer one, or no entity list fails
the load. Problems inside a usable file are logged (file, game object, component, field) and skipped: an unknown
component or field is ignored, a missing field keeps the component's default, an unknown enum value or script name
keeps the default, a reference to a game object that isn't in the file is none, and a model or asset that can't be
found is none. A game object whose ID is used already gets a new one and the file's references follow it.

Components are added type by type in the engine's load order (transforms and parents first), the way the C API adds
them. Scripts are created from the `script` components at the scene's next update; the nav mesh isn't saved, the level
builds it after loading (`engineNavMeshBuild`), and the level spawns the player at the `player_start` of the entrance
the player comes through (`"default"` when the game starts).

## Levels and their exits

A level holds **player starts** (`player_start`, with an `id`), never the player: `default` is where the game starts
in it, the others are entrances from other levels. A **level exit** (a game object with the `level_exit` script and
component, a kinematic trigger collider and a pad showing it) names a level file (relative to this folder) and the
`entrance` there: the player walking into it goes to that level, entering at the player start with that id (the default
one when there is none), with what they had (hp, coins, weapon). `test_level.json` and `test_level_2.json` lead to
each other.

To add a level, build it in code with `LevelBuilder` (`player_start`, `level_exit` and the rest) and save it with
`engineSceneSave`, or copy a file and edit it; then give an existing level an exit to it, and the new level a player
start with the entrance id that exit names. Place a player start a few steps away from its level's exits, so the player
arriving there doesn't stand in one.

## Game components

The game's own components (`src/games/project_c/scripts/game_components.h`: `enemy_config`, `player_config`,
`building_part_config`, `house_config`, `player_start`, `level_exit`) are saved like the engine's. Rules for adding one:

- A **plain C struct**: numbers, bools, enums (saved by name), fixed char arrays for strings, fixed arrays, structs of
  the game's own struct types, and game object / geometry / texture handles. No pointers, no `std::string`, no unions.
  At most 256 bytes.
- It holds **designer-set values only**: what a level designer tunes (an enemy's HP, speed, colours) or what links
  objects (a house's roof props). Runtime state (current HP, cooldowns, targets) stays in the script's members and is
  never saved.
- Scripts **read it on every use** (`get_game_component`), never copy it once at the start, so an edit in the editor
  takes effect while playing.
- Describe every field (`register_game_components`), with editor hints where they help: a range, a tooltip,
  `ENGINE_FIELD_KIND_COLOR` for a colour, read-only or hidden.
- A script that needs it lists it when it is registered (`ScriptRegistry::add<T>(name, { type })`), so attaching the
  script adds the component with its defaults.
- Renaming a component, a field, an enum value or a script type breaks the files using the old name: rename it in the
  files too.
