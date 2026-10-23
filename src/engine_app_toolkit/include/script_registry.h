#pragma once
#include "engine.h"
#include "utils.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace engine
{
class IScene;
class IScript;

// What a game object's script is, as data: the `script` component, which scene files save and the editor edits (a
// dropdown of the registered script types). Like Unity's MonoBehaviour reference or Godot's script property, but the
// engine never knows scripts: it is a game-defined component the toolkit registers.
struct ScriptComponent
{
    // the index of the script type in the ScriptRegistry; NO_SCRIPT ("none") for none
    std::int32_t type;
};

// The script types of an application, by name, and the `script` component naming them. Register every script type a
// scene file or the editor may put on a game object once at startup, then call register_component(). A script
// created from the component takes only its scene and game object, like `T(IScene*, engine_game_object_t)`.
class ENGINE_APP_TOOLKIT_API ScriptRegistry
{
public:
    static constexpr std::int32_t NO_SCRIPT = 0;
    static constexpr const char* COMPONENT_NAME = "script";

    using Factory = std::function<IScript*(IScene*, engine_game_object_t)>;

    struct ScriptType
    {
        std::string name;
        std::type_index cpp_type;
        Factory factory;
        // game-defined components the script needs: added with their default values, when missing, as the script is
        // attached (like Unity's RequireComponent)
        std::vector<engine_component_type_t> required;
    };

    // Registers a script type under a name (unique, not "none"), before register_component(), with the game-defined
    // components it needs. The name is what scene files save: renaming a script type breaks the files using it. False
    // (logged) when the name or the type is registered already, or the component exists already.
    template<typename T>
    bool add(std::string_view name, std::vector<engine_component_type_t> required = {})
    {
        return add_type(name, typeid(T), [](IScene* scene, engine_game_object_t go) -> IScript* { return new T(scene, go); }, std::move(required));
    }

    // Registers the `script` component, its type an enum of the script names. Call it once, after every add() and
    // before a scene file holding scripts is loaded. False (logged) when it fails.
    bool register_component();
    // ENGINE_INVALID_OBJECT_HANDLE until register_component()
    engine_component_type_t get_component_type() const { return component_type_; }

    // nullptr for NO_SCRIPT or an unknown index
    const ScriptType* find(std::int32_t index) const;
    // the index of the script type registered for T, NO_SCRIPT when there is none
    std::int32_t find_index(std::type_index cpp_type) const;

private:
    bool add_type(std::string_view name, std::type_index cpp_type, Factory factory, std::vector<engine_component_type_t> required);

    // index - 1 -> type (index 0 is NO_SCRIPT)
    std::vector<ScriptType> types_;
    engine_component_type_t component_type_ = ENGINE_INVALID_OBJECT_HANDLE;
};
}  // namespace engine
