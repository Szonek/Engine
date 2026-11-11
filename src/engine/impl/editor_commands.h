#pragma once
#include "scene.h"
#include "editor_asset_browser.h"
#include "editor_command_manager.h"

#include <algorithm>
#include <optional>
#include <vector>
#include <memory>
#include <string>
#include <string_view>

#include "components_internals/guizmo_component.h"
#include "components_internals/outline_component.h"

namespace engine
{
// logs that the game object an undo step changes isn't in the scene (any more)
void log_missing(std::string_view command);

template<typename TComp>
class CommandUpdateComponent : public ICommand
{
public:
    CommandUpdateComponent(Scene& sc, entt::entity e, TComp& new_comp)
        : ref_(sc.make_ref(e))
        , prev_(*sc.get_component<TComp>(e))
        , new_(new_comp)
    {
    }

    void execute(Scene& sc) override
    {
        apply(sc, new_);
    }

    void undo(Scene& sc) override
    {
        apply(sc, prev_);
    }

    // the same component of the same game object, changed again by the same edit: one step from before the edit to
    // after it
    bool merge_with(const ICommand* other) override
    {
        const auto* next = dynamic_cast<const CommandUpdateComponent<TComp>*>(other);
        if (!next || next->ref_ != ref_)
        {
            return false;
        }
        new_ = next->new_;
        return true;
    }

private:
    void apply(Scene& sc, const TComp& value)
    {
        const auto e = sc.resolve(ref_);
        if (e == entt::null || !sc.has_component<TComp>(e))
        {
            log_missing("component update");
            return;
        }
        sc.update_component<TComp>(e, value);
    }

    const GameObjectRef ref_;
    const TComp prev_;
    TComp new_;
};

// Like CommandUpdateComponent, for a component known only by its type description (game-defined components).
class CommandUpdateTypedComponent : public ICommand
{
public:
    CommandUpdateTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type, const void* new_value);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;
    // like CommandUpdateComponent's
    bool merge_with(const ICommand* other) override;

private:
    // nothing happens when the component was removed meanwhile
    void apply(Scene& sc, const ValueBuffer& value);

    const GameObjectRef ref_;
    const TypeInfo& type_;
    ValueBuffer prev_;
    ValueBuffer new_;
};

// Several steps as one undo step: executed in order, undone in reverse (e.g. a script set with the components it requires).
class CommandSequence : public ICommand
{
public:
    explicit CommandSequence(std::vector<std::unique_ptr<ICommand>> commands) : commands_(std::move(commands)) {}

    void execute(Scene& sc) override
    {
        for (auto& command : commands_)
        {
            command->execute(sc);
        }
    }

    void undo(Scene& sc) override
    {
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it)
        {
            (*it)->undo(sc);
        }
    }

    bool changes_scene() const override
    {
        return std::ranges::any_of(commands_, [](const auto& command) { return command->changes_scene(); });
    }

private:
    std::vector<std::unique_ptr<ICommand>> commands_;
};

// Changes a component known only by its type description and adds the components its new value requires which the game
// object doesn't have (engine::required_components: a script's config when the script component names it, like Unity's
// RequireComponent), with their defaults, in one undo step.
std::unique_ptr<ICommand> make_update_typed_component_command(Scene& sc, entt::entity e, const TypeInfo& type, const void* new_value);

// Adds a component known only by its type description, with its default value; undo removes it.
class CommandAddTypedComponent : public ICommand
{
public:
    CommandAddTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    const GameObjectRef ref_;
    const TypeInfo& type_;
};

// Removes a component known only by its type description; undo adds it back with the value it had.
class CommandRemoveTypedComponent : public ICommand
{
public:
    CommandRemoveTypedComponent(Scene& sc, entt::entity e, const TypeInfo& type);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    const GameObjectRef ref_;
    const TypeInfo& type_;
    ValueBuffer value_;
};

// Adds or removes the selection markers (gizmo, outline) of a game object, entt::null for none. They are internal, so a
// game object loaded back (Stop, undo of a delete) has none.
void set_selection_markers(Scene& sc, entt::entity e, bool selected);

// Changes the selection, one undo step like in Blender and Unity: the game object (entt::null for none) and, given the
// asset browser's selection, the file, so undo and redo bring back a file selected in place of a game object too.
class CommandSetSelection : public ICommand
{
public:
    // selects the game object, leaving the selected file alone (none is selected while a game object is)
    CommandSetSelection(Scene& sc, entt::entity prev_selected, entt::entity new_selected);
    // selects the game object and the file (an empty path for none); the file selected before is the one selected now
    CommandSetSelection(Scene& sc, IAssetSelection& assets, entt::entity prev_selected, entt::entity new_selected, std::string_view new_asset);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;
    // the selection isn't saved
    bool changes_scene() const override { return false; }

private:
    void select(Scene& sc, const GameObjectRef& deselected, const GameObjectRef& selected, const std::string& asset);

    const GameObjectRef prev_;
    const GameObjectRef new_;
    IAssetSelection* assets_ = nullptr;
    std::string asset_prev_;
    std::string asset_new_;
};

class CommandRenameEntity : public ICommand
{
public:
    CommandRenameEntity(Scene& sc, entt::entity e, std::string_view new_name);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    void rename(Scene& sc, const std::string& name);

    const GameObjectRef ref_;
    std::string prev_name_;
    std::string new_name_;
};

// An empty game object, with only a name; with `position`, also a transform there (created in the scene view)
class CommandAddEntity : public ICommand
{
public:
    CommandAddEntity() = default;
    explicit CommandAddEntity(const glm::vec3& position) : position_(position) {}

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    GameObjectRef ref_;
    std::optional<glm::vec3> position_;
};

// Destroys the game object and everything parented under it. They are saved first and loaded back on undo, with
// their persistent IDs and handles (so references to them find them again) and at the same place among their parent's
// children. Transient game objects aren't saved, so they don't come back, and a game object removed from what a prefab
// created on its instance can't be saved yet, so it comes back (logged); a game object of a prefab instance (but its
// root) isn't deleted (see can_destroy).
class CommandDestroyEntity : public ICommand
{
public:
    CommandDestroyEntity(Scene& sc, entt::entity e);

    // false for a game object its prefab instance created: a game object removed from an instance can't be saved yet,
    // and undo couldn't bring back what the prefab gave it which scene files don't describe yet (skin, joint attachment);
    // also false when one is parented under `e` without its instance's root (moved out of its instance by code: the
    // editor doesn't move them), which would come back apart from its instance
    static bool can_destroy(const Scene& sc, entt::entity e);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;

private:
    const GameObjectRef ref_;
    std::string saved_;
    SceneLoadOptions restore_;
    GameObjectRef parent_;
    std::size_t child_index_ = 0;
};

// Copies the game object and everything parented under it, like Duplicate in Unity, Godot and Unreal: the copy gets
// new persistent IDs, the same parent, the place right after the original among its parent's children and the name
// "<name> (1)" (the first number no sibling uses), and becomes the selection. References between the copied game
// objects point at the copies, references to the rest of the scene stay. A prefab instance is copied as an instance
// of its prefab with what was added to it, changed in it (prefab overrides) and removed from it (a game object removed
// from what the prefab created can't be saved yet, so the copy has it, logged). Undo destroys the copy (and gives the selection back), redo brings it back with the same IDs and
// handles; one undo step.
class CommandDuplicateEntity : public ICommand
{
public:
    // `selected`: the selection before the copy, given back on undo
    CommandDuplicateEntity(Scene& sc, entt::entity e, entt::entity selected);

    // false for a transient game object (runtime objects aren't scene content) and a game object its prefab instance
    // created (a copy out of an instance would need what scene files don't describe yet: skin, joint attachment), or
    // which holds one without its instance's root (see can_destroy); duplicating it anyway logs why and copies nothing
    static bool can_duplicate(const Scene& sc, entt::entity e);

    // the copy in `sc`, entt::null when nothing was copied
    entt::entity get_copy(const Scene& sc) const { return sc.resolve(copy_); }

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;
    // a copy which couldn't be made leaves no undo step
    bool has_effect() const override { return !copy_.is_none(); }

private:
    void duplicate(Scene& sc);

    const GameObjectRef ref_;
    const GameObjectRef selected_;
    GameObjectRef copy_;
    // undo destroys the copy the way a delete does, redo is that delete's undo
    std::unique_ptr<CommandDestroyEntity> destroy_copy_;
    std::unique_ptr<CommandSetSelection> select_copy_;
};

// Places something dragged from the asset browser into the scene view as a new game object (with what it brings under
// it) at `position`, and selects it. Undo destroys it the way a delete does (and gives the selection back, a file
// too), redo brings it back with the same IDs and handles; one undo step. What is placed is the derived command's.
class CommandPlaceAsset : public ICommand
{
public:
    // the placed game object in `sc`, entt::null when nothing was placed
    entt::entity get_instance(const Scene& sc) const { return sc.resolve(root_); }

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;
    // an asset which couldn't be loaded or placed leaves no undo step
    bool has_effect() const override { return !root_.is_none(); }

protected:
    // `selected`: the game object selected before the placement, given back on undo with the file selected in `assets`
    CommandPlaceAsset(Scene& sc, IAssetSelection& assets, std::string_view asset_id, const glm::vec3& position, entt::entity selected);
    // Creates the game object for the asset at `position`, named; entt::null (logged) when it can't.
    virtual entt::entity create(Scene& sc, const std::string& asset_id, const glm::vec3& position) = 0;

private:
    void place(Scene& sc);

    IAssetSelection& assets_;
    const std::string asset_id_;
    const glm::vec3 position_;
    const GameObjectRef selected_;
    GameObjectRef root_;
    // undo destroys what was placed the way a delete does, redo is that delete's undo
    std::unique_ptr<CommandDestroyEntity> destroy_placed_;
    std::unique_ptr<CommandSetSelection> select_placed_;
};

// Places a model or a scene file dragged into the scene view: a persistent instance of its prefab, its root at the
// position. The scene's assets hold the prefab, so the instance's assets live as long as it does.
class CommandPlacePrefab : public CommandPlaceAsset
{
public:
    CommandPlacePrefab(Scene& sc, IAssetSelection& assets, std::string_view prefab_asset_id, const glm::vec3& position, entt::entity selected)
        : CommandPlaceAsset(sc, assets, prefab_asset_id, position, selected) {}

protected:
    entt::entity create(Scene& sc, const std::string& asset_id, const glm::vec3& position) override;
};

// Places a terrain file dragged into the scene view: a new game object named after the file with what a terrain is
// made of (a transform, the terrain and a material to draw it, a heightfield collider to walk on it), its middle at
// the position.
class CommandPlaceTerrain : public CommandPlaceAsset
{
public:
    CommandPlaceTerrain(Scene& sc, IAssetSelection& assets, std::string_view terrain_asset_id, const glm::vec3& position, entt::entity selected)
        : CommandPlaceAsset(sc, assets, terrain_asset_id, position, selected) {}

protected:
    entt::entity create(Scene& sc, const std::string& asset_id, const glm::vec3& position) override;
};

class CommandSetPhysicsDebugDraw: public ICommand
{
public:
    explicit CommandSetPhysicsDebugDraw(bool flag);

    void execute(Scene& sc) override;
    void undo(Scene& sc) override;
    bool changes_scene() const override { return false; }

private:
    bool flag_;
};

}  // namespace engine
