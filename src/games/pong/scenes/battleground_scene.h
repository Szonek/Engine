#pragma once
#include "iscene.h"

namespace pong
{
    class BattlegroundScene : public engine::IScene
    {
    public:
        enum class PlayerType
        {
            eHuman = 0,
            eBotLow = 1
        };

    public:
        BattlegroundScene(engine_application_t app_handle, engine::SceneManager* scn_mgn, engine_result_code_t& engine_error_code, PlayerType left_player_type);

    public:
        static constexpr const char* get_name() { return "pve_scene"; }

        void activate() override;
        void deactivate() override;
};
}