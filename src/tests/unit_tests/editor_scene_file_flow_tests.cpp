#include "editor_scene_file_flow.h"

#include <gtest/gtest.h>

namespace
{
using engine::SceneFileAction;
using engine::SceneFileFlow;

TEST(EditorSceneFileFlow, SaveWritesTheScenesFile)
{
    SceneFileFlow flow;
    EXPECT_FALSE(flow.request(SceneFileAction::eSave, {}, "level.json", true));
    ASSERT_TRUE(flow.has_request());
    const auto request = flow.take_request();
    EXPECT_EQ(request.action, SceneFileAction::eSave);
    EXPECT_EQ(request.path, "level.json");
    EXPECT_FALSE(flow.has_request());
}

TEST(EditorSceneFileFlow, SaveOfASceneWithoutAFileIsSaveAs)
{
    SceneFileFlow flow;
    EXPECT_TRUE(flow.request(SceneFileAction::eSave, {}, "", true));
    EXPECT_FALSE(flow.has_request());
    // Save As confirmed
    EXPECT_FALSE(flow.request(SceneFileAction::eSave, "rooms/a.json", "", true));
    EXPECT_EQ(flow.take_request().path, "rooms/a.json");
}

TEST(EditorSceneFileFlow, OpenWithoutUnsavedChangesIsDoneAtOnce)
{
    SceneFileFlow flow;
    EXPECT_FALSE(flow.request(SceneFileAction::eOpen, "b.json", "a.json", false));
    EXPECT_FALSE(flow.take_prompt_open());
    const auto request = flow.take_request();
    EXPECT_EQ(request.action, SceneFileAction::eOpen);
    EXPECT_EQ(request.path, "b.json");
}

TEST(EditorSceneFileFlow, OpenWithUnsavedChangesAsksFirst)
{
    SceneFileFlow flow;
    EXPECT_FALSE(flow.request(SceneFileAction::eOpen, "b.json", "a.json", true));
    EXPECT_FALSE(flow.has_request());
    EXPECT_TRUE(flow.take_prompt_open());
    EXPECT_FALSE(flow.take_prompt_open());
    EXPECT_EQ(flow.get_waiting(), SceneFileAction::eOpen);
}

TEST(EditorSceneFileFlow, DontSaveGoesOnWithWhatWaited)
{
    SceneFileFlow flow;
    (void)flow.request(SceneFileAction::eNew, "templates/level.json", "a.json", true);
    EXPECT_FALSE(flow.prompt_discard());
    EXPECT_EQ(flow.get_waiting(), SceneFileAction::eNone);
    const auto request = flow.take_request();
    EXPECT_EQ(request.action, SceneFileAction::eNew);
    EXPECT_EQ(request.path, "templates/level.json");
}

TEST(EditorSceneFileFlow, CancelDropsWhatWaited)
{
    SceneFileFlow flow;
    (void)flow.request(SceneFileAction::eOpen, "b.json", "a.json", true);
    flow.cancel_waiting();
    EXPECT_EQ(flow.get_waiting(), SceneFileAction::eNone);
    EXPECT_FALSE(flow.has_request());
}

TEST(EditorSceneFileFlow, SaveInThePromptSavesThenGoesOn)
{
    SceneFileFlow flow;
    (void)flow.request(SceneFileAction::eOpen, "b.json", "a.json", true);
    EXPECT_FALSE(flow.prompt_save("a.json"));
    // the save first, the Open waits for it
    const auto save = flow.take_request();
    EXPECT_EQ(save.action, SceneFileAction::eSave);
    EXPECT_EQ(save.path, "a.json");
    EXPECT_EQ(flow.get_waiting(), SceneFileAction::eOpen);
    EXPECT_FALSE(flow.on_saved());
    const auto open = flow.take_request();
    EXPECT_EQ(open.action, SceneFileAction::eOpen);
    EXPECT_EQ(open.path, "b.json");
}

TEST(EditorSceneFileFlow, SaveInThePromptOfASceneWithoutAFileIsSaveAsFirst)
{
    SceneFileFlow flow;
    (void)flow.request(SceneFileAction::eOpen, "b.json", "", true);
    EXPECT_TRUE(flow.prompt_save(""));
    EXPECT_FALSE(flow.has_request());
    // Save As confirmed: saved, then the Open
    EXPECT_FALSE(flow.request(SceneFileAction::eSave, "new.json", "", true));
    EXPECT_EQ(flow.take_request().path, "new.json");
    EXPECT_FALSE(flow.on_saved());
    EXPECT_EQ(flow.take_request().action, SceneFileAction::eOpen);
}

TEST(EditorSceneFileFlow, ASaveWhichFailedDropsWhatWaited)
{
    SceneFileFlow flow;
    (void)flow.request(SceneFileAction::eOpen, "b.json", "a.json", true);
    EXPECT_FALSE(flow.prompt_save("a.json"));
    (void)flow.take_request();
    flow.cancel_waiting();
    EXPECT_FALSE(flow.on_saved());
    EXPECT_FALSE(flow.has_request());
}

TEST(EditorSceneFileFlow, QuitWithoutUnsavedChangesClosesAtOnce)
{
    SceneFileFlow flow;
    EXPECT_TRUE(flow.request_quit(false));
    EXPECT_FALSE(flow.take_prompt_open());
}

TEST(EditorSceneFileFlow, QuitWithUnsavedChangesGoesOnAfterTheAnswer)
{
    SceneFileFlow discarded;
    EXPECT_FALSE(discarded.request_quit(true));
    EXPECT_TRUE(discarded.take_prompt_open());
    EXPECT_EQ(discarded.get_waiting(), SceneFileAction::eQuit);
    EXPECT_TRUE(discarded.prompt_discard());
    EXPECT_FALSE(discarded.has_request());

    SceneFileFlow saved;
    (void)saved.request_quit(true);
    EXPECT_FALSE(saved.prompt_save("a.json"));
    EXPECT_EQ(saved.take_request().action, SceneFileAction::eSave);
    EXPECT_TRUE(saved.on_saved());
    EXPECT_FALSE(saved.has_request());
}

TEST(EditorSceneFileFlow, ASaveWithNothingWaitingGoesNowhere)
{
    SceneFileFlow flow;
    (void)flow.request(SceneFileAction::eSave, {}, "a.json", true);
    (void)flow.take_request();
    EXPECT_FALSE(flow.on_saved());
    EXPECT_FALSE(flow.has_request());
}
}  // namespace
