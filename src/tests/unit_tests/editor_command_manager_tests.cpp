#include "editor_command_manager.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>

// The editor's undo history: scene steps and data asset steps, which survive Stop and the scene's release and go when
// their file is read again.
namespace
{
using engine::CommandManager;

// the data assets' values by asset ID; a value starting with "bad" can't be set
class FakeDataAssets : public engine::IDataAssetValues
{
public:
    bool set_data_asset_value_text(std::string_view asset_id, std::string_view text) override
    {
        if (text.starts_with("bad"))
        {
            return false;
        }
        values[std::string(asset_id)] = std::string(text);
        return true;
    }

    std::map<std::string, std::string> values;
};

// a step of the scene: sets an int
class SetValue : public engine::ICommand
{
public:
    SetValue(int& target, int value) : target_(target), prev_(target), new_(value) {}
    void execute(engine::Scene& /*sc*/) override { target_ = new_; }
    void undo(engine::Scene& /*sc*/) override { target_ = prev_; }

private:
    int& target_;
    const int prev_;
    const int new_;
};

class EditorCommandManager : public ::testing::Test
{
protected:
    std::unique_ptr<engine::ICommand> set_value(int value) { return std::make_unique<SetValue>(scene_value_, value); }
    std::unique_ptr<engine::ICommand> set_asset(std::string_view asset_id, std::string value)
    {
        const auto it = assets_.values.find(std::string(asset_id));
        return std::make_unique<engine::CommandUpdateDataAsset>(assets_, asset_id, it == assets_.values.end() ? std::string() : it->second, std::move(value));
    }
    void execute(std::unique_ptr<engine::ICommand> cmd)
    {
        commands_.execute_command(scene(), std::move(cmd));
        // every step its own, as after a click
        commands_.end_continuous_edit();
    }
    // the steps here never touch the scene they are given
    engine::Scene& scene() { return *reinterpret_cast<engine::Scene*>(&scene_storage_); }
    const std::string& asset(const char* asset_id) { return assets_.values[asset_id]; }

    CommandManager commands_;
    FakeDataAssets assets_;
    int scene_value_ = 0;
    int scene_storage_ = 0;
};

TEST_F(EditorCommandManager, DataAssetStepIsUndoneWithoutMakingTheSceneUnsaved)
{
    assets_.values["a.json"] = "1";
    execute(set_asset("a.json", "2"));
    EXPECT_EQ(asset("a.json"), "2");
    EXPECT_FALSE(commands_.is_dirty());
    commands_.undo(scene());
    EXPECT_EQ(asset("a.json"), "1");
    commands_.redo(scene());
    EXPECT_EQ(asset("a.json"), "2");
}

TEST_F(EditorCommandManager, ValueWhichCantBeSetLeavesNoStep)
{
    execute(set_value(1));
    assets_.values["a.json"] = "1";
    execute(set_asset("a.json", "bad"));
    EXPECT_EQ(asset("a.json"), "1");
    // the undo takes back the scene step, not an empty one
    commands_.undo(scene());
    EXPECT_EQ(scene_value_, 0);
}

TEST_F(EditorCommandManager, DataAssetStepsSurviveStopAndKeepTheEditRedo)
{
    assets_.values["a.json"] = "1";
    execute(set_value(5));
    commands_.undo(scene());
    commands_.begin_play_session();
    execute(set_asset("a.json", "2"));
    execute(set_value(9));
    commands_.end_play_session();
    // Stop gave the scene back (the play session's step is gone), the data asset keeps its edit
    scene_value_ = 0;
    EXPECT_FALSE(commands_.is_dirty());

    commands_.undo(scene());
    EXPECT_EQ(asset("a.json"), "1");
    commands_.redo(scene());
    EXPECT_EQ(asset("a.json"), "2");
    // the step undone before Play can still be redone
    commands_.redo(scene());
    EXPECT_EQ(scene_value_, 5);
    EXPECT_TRUE(commands_.is_dirty());
}

TEST_F(EditorCommandManager, DataAssetStepsSurviveTheScenesRelease)
{
    assets_.values["a.json"] = "1";
    execute(set_value(5));
    execute(set_asset("a.json", "2"));
    commands_.clear();
    EXPECT_FALSE(commands_.is_dirty());
    commands_.undo(scene());
    EXPECT_EQ(asset("a.json"), "1");
    // the scene's step is gone
    commands_.undo(scene());
    EXPECT_EQ(scene_value_, 5);
}

TEST_F(EditorCommandManager, DroppingDataAssetStepsKeepsTheSavePoint)
{
    assets_.values["a.json"] = "1";
    assets_.values["b.json"] = "1";
    execute(set_value(1));
    execute(set_asset("a.json", "2"));
    commands_.mark_saved();
    execute(set_value(2));
    execute(set_asset("b.json", "2"));
    execute(set_asset("a.json", "3"));
    commands_.undo(scene());

    // a.json's file was read again: its steps go, done and undone, b.json's stay
    commands_.drop_data_asset_steps("a.json");
    commands_.redo(scene());
    EXPECT_EQ(asset("a.json"), "2");
    commands_.undo(scene());
    EXPECT_EQ(asset("b.json"), "1");
    EXPECT_TRUE(commands_.is_dirty());
    commands_.undo(scene());
    EXPECT_EQ(scene_value_, 1);
    EXPECT_FALSE(commands_.is_dirty());
    commands_.undo(scene());
    EXPECT_EQ(scene_value_, 0);
    EXPECT_EQ(asset("a.json"), "2");
    EXPECT_TRUE(commands_.is_dirty());
}

TEST_F(EditorCommandManager, DroppingDataAssetStepsKeepsTheEditHistoryAtStop)
{
    assets_.values["a.json"] = "1";
    execute(set_value(5));
    execute(set_asset("a.json", "2"));
    commands_.begin_play_session();
    commands_.drop_data_asset_steps("a.json");
    commands_.end_play_session();
    commands_.undo(scene());
    EXPECT_EQ(scene_value_, 0);
    EXPECT_EQ(asset("a.json"), "2");
}
}  // namespace
