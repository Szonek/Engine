#pragma once
#include "iscene.h"

namespace pong
{
    class BattlegroundScene : public engine::IScene
    {
    public:
        struct MyDataForUI
        {
            bool move_right_player = false;
            bool move_left_player = false;
            class PlayerPaddleScript* right_player;
            class PlayerPaddleScript* left_player;

            std::uint32_t score_left = 0;
            std::uint32_t score_right = 0;

            bool show_left_player_controllors = false;
            bool show_right_player_controllors = false;
        };

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

    private:
        engine_ui_data_handle_t ui_data_handle_ = nullptr;
        engine_ui_document_t ui_doc_ = nullptr;
        engine_ui_element_t ui_element_left_controller = nullptr;
        engine_ui_element_t ui_element_right_controller_ = nullptr;

        MyDataForUI my_data_{};
};
}