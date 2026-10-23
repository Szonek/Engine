#pragma once
#include "base_script.h"
#include "encounter_script.h"

#include "engine_string.h"
#include "engine_vector.h"

#include <cstdint>
#include <optional>

namespace project_c
{
// The gameplay HUD (assets/ui_docs/project_c_gameplay_ui.rml): player health, enemy health bars, weapons on the
// ground and the encounter status. It only reads game state (Player, Enemy, Weapon and Encounter scripts) every
// frame; gameplay scripts never call it, so a scene without a HUD (e.g. a test scene) plays the same.
class GameplayHud : public BaseNode
{
public:
    GameplayHud(engine::IScene* my_scene, engine_game_object_t go);
    ~GameplayHud();

    void late_update(float dt) override;

    // the scene shows/hides it (scene activation, editor game view); no-op when the document failed to load
    void set_visible(bool visible);
    bool is_visible() const { return visible_; }

private:
    struct ItemsOnGround
    {
        engine_vector_uint32_t go = engineVectorCreateUint32();
        engine_vector_engine_string_t name = engineVectorCreateEngineString();
        engine_vector_engine_string_t pos_x = engineVectorCreateEngineString();
        engine_vector_engine_string_t pos_y = engineVectorCreateEngineString();
        engine_vector_uint32_t show = engineVectorCreateUint32();
    };

    struct EnemyHealthbars
    {
        engine_vector_uint32_t go = engineVectorCreateUint32();
        engine_vector_engine_string_t pos_x = engineVectorCreateEngineString();
        engine_vector_engine_string_t pos_y = engineVectorCreateEngineString();
        engine_vector_uint32_t progressbar_value = engineVectorCreateUint32();  // in range: <0; 100>
    };

    void update_player_health();
    void update_enemies();
    void update_items_on_ground();
    void update_encounter_status();
    void set_status(const char* text);

    void update_enemy(const class Enemy* en);
    void remove_enemy(std::uint32_t index);
    void update_item_on_ground(const class Weapon* weapon);
    void remove_item_from_ground(std::uint32_t index);

private:
    engine_ui_document_t doc_{};
    engine_ui_data_handle_t handle_{};

    std::uint32_t player_health_ = 100;
    std::uint32_t player_max_health_ = 100;
    engine_string_t* encounter_status_ = nullptr;
    EnemyHealthbars enemies_{};
    ItemsOnGround items_{};

    bool visible_ = true;
    bool gameplay_started_ = false;
    std::optional<Encounter::State> shown_encounter_state_;
};
}  // namespace project_c
