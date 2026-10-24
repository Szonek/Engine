#include "copy_name.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

namespace
{
constexpr std::size_t MAX_LENGTH = 16;
}

TEST(CopyName, FirstCopyGetsOne)
{
    EXPECT_EQ(engine::copy_name("wall", { "wall" }, MAX_LENGTH), "wall (1)");
}

TEST(CopyName, TakesTheFirstNumberNoSiblingUses)
{
    EXPECT_EQ(engine::copy_name("wall", { "wall", "wall (1)", "wall (3)" }, MAX_LENGTH), "wall (2)");
}

TEST(CopyName, CopyOfACopyCountsOnFromTheBase)
{
    EXPECT_EQ(engine::copy_name("wall (1)", { "wall", "wall (1)" }, MAX_LENGTH), "wall (2)");
    EXPECT_EQ(engine::copy_name("wall (12)", { "wall (12)" }, MAX_LENGTH), "wall (1)");
}

TEST(CopyName, KeepsParenthesesWhichAreNotANumber)
{
    EXPECT_EQ(engine::copy_name("wall (a)", {}, MAX_LENGTH), "wall (a) (1)");
    EXPECT_EQ(engine::copy_name("wall ()", {}, MAX_LENGTH), "wall () (1)");
    EXPECT_EQ(engine::copy_name("wall(1)", {}, MAX_LENGTH), "wall(1) (1)");
}

TEST(CopyName, EmptyName)
{
    EXPECT_EQ(engine::copy_name("", {}, MAX_LENGTH), " (1)");
}

TEST(CopyName, LongNameIsCutAndKeepsTheNumber)
{
    // 15 characters and the terminator fit
    const auto name = engine::copy_name("abcdefghijklmnop", {}, MAX_LENGTH);
    EXPECT_EQ(name, "abcdefghijk (1)");
    EXPECT_EQ(engine::copy_name("abcdefghijklmnop", { name }, MAX_LENGTH), "abcdefghijk (2)");
}

TEST(CopyName, CutNeverSplitsAUtf8Character)
{
    // "ł" is two bytes (C5 82); a cut after 11 bytes would land between them
    const std::string name = "abcdefghij\xC5\x82xyz";
    EXPECT_EQ(engine::copy_name(name, {}, MAX_LENGTH), "abcdefghij (1)");
}
