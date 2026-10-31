#include "editor_save_point.h"

#include <gtest/gtest.h>

namespace
{
using engine::SavePoint;

TEST(EditorSavePoint, ALoadedSceneIsClean)
{
    const SavePoint save_point;
    EXPECT_FALSE(save_point.is_dirty());
}

TEST(EditorSavePoint, AnEditMakesItUnsavedAndSaveMakesItClean)
{
    SavePoint save_point;
    save_point.on_execute(true);
    EXPECT_TRUE(save_point.is_dirty());
    save_point.mark_saved();
    EXPECT_FALSE(save_point.is_dirty());
}

TEST(EditorSavePoint, UndoingBackToTheSavePointMakesItCleanAgain)
{
    SavePoint save_point;
    save_point.on_execute(true);
    save_point.on_undo();
    EXPECT_FALSE(save_point.is_dirty());
    save_point.on_redo();
    EXPECT_TRUE(save_point.is_dirty());
}

TEST(EditorSavePoint, UndoAfterSaveMakesItUnsaved)
{
    SavePoint save_point;
    save_point.on_execute(true);
    save_point.mark_saved();
    save_point.on_undo();
    EXPECT_TRUE(save_point.is_dirty());
    save_point.on_redo();
    EXPECT_FALSE(save_point.is_dirty());
}

TEST(EditorSavePoint, ASelectionChangeKeepsItClean)
{
    SavePoint save_point;
    save_point.on_execute(true);
    save_point.mark_saved();
    save_point.on_execute(false);
    EXPECT_FALSE(save_point.is_dirty());
    // undoing past the selection to before the save is a change again
    save_point.on_undo();
    save_point.on_undo();
    EXPECT_TRUE(save_point.is_dirty());
}

TEST(EditorSavePoint, ANewStepReplacingTheUndoneSavedOnesCantBeUndoneToClean)
{
    SavePoint save_point;
    save_point.on_execute(true);
    save_point.mark_saved();
    save_point.on_undo();
    // the saved step is gone with the redo stack
    save_point.on_execute(true);
    EXPECT_TRUE(save_point.is_dirty());
    save_point.on_undo();
    EXPECT_TRUE(save_point.is_dirty());
}

TEST(EditorSavePoint, ASavePointWhichFellOffTheHistoryStaysUnsavedUntilTheNextSave)
{
    SavePoint save_point;
    // saved with an empty history, then the first step falls off
    save_point.on_execute(true);
    save_point.on_drop_oldest();
    EXPECT_TRUE(save_point.is_dirty());
    save_point.on_undo();
    EXPECT_TRUE(save_point.is_dirty());
    save_point.mark_saved();
    EXPECT_FALSE(save_point.is_dirty());
}

TEST(EditorSavePoint, ASavePointStillInTheHistoryMovesWithIt)
{
    SavePoint save_point;
    save_point.on_execute(true);
    save_point.on_execute(true);
    save_point.mark_saved();
    save_point.on_execute(true);
    save_point.on_drop_oldest();
    EXPECT_TRUE(save_point.is_dirty());
    save_point.on_undo();
    EXPECT_FALSE(save_point.is_dirty());
}

TEST(EditorSavePoint, NoStepMergesIntoTheSavedOne)
{
    SavePoint save_point;
    save_point.on_execute(true);
    save_point.mark_saved();
    EXPECT_FALSE(save_point.can_merge());
    save_point.on_execute(true);
    EXPECT_TRUE(save_point.can_merge());
    save_point.on_merge(true);
    EXPECT_TRUE(save_point.is_dirty());
}

TEST(EditorSavePoint, AChangeWithoutAnUndoStepStaysUnsavedUntilTheNextSave)
{
    SavePoint save_point;
    save_point.mark_changed();
    EXPECT_TRUE(save_point.is_dirty());
    save_point.mark_saved();
    EXPECT_FALSE(save_point.is_dirty());
}
}  // namespace
