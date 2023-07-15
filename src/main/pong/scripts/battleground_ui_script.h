#pragma once
#include "engine.h"

#include "iscript.h"

namespace pong
{
class PlayerPaddleScript;

class BattlegroundUiScript : public engine::IScript
{
public:
    BattlegroundUiScript(engine::IScene* my_scene);

    void set_left_player_script(PlayerPaddleScript* script, bool is_bot);
    void set_right_player_script(PlayerPaddleScript* script);

    void update(float dt) override;

public:
    struct MyDataForUI
    {
        struct PlayerDataUI
        {
            PlayerPaddleScript* script = nullptr;
            bool show_controllos = false;
            bool move_allowed = false;
            std::uint32_t score = 0;
        };


        PlayerDataUI player_right;
        PlayerDataUI player_left;
    };

private:
    engine_ui_data_handle_t ui_data_handle_ = nullptr;
    engine_ui_document_t ui_doc_ = nullptr;
    MyDataForUI my_data_{};
};


} // namespace pong