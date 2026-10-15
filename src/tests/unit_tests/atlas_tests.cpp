#include "named_atlas.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

// The atlas is header-only. Tests provide the logger the engine library would, and record what was logged.
namespace
{
std::vector<std::string> G_LOGGED_MESSAGES;
}  // namespace

void engine::log::log(engine::log::LogLevel /*level*/, std::string_view msg)
{
    G_LOGGED_MESSAGES.emplace_back(msg);
}

namespace
{
using TestAtlas = engine::Atlas<int, 4>;

class AtlasTest : public ::testing::Test
{
protected:
    void SetUp() override { G_LOGGED_MESSAGES.clear(); }
};

TEST_F(AtlasTest, AddedObjectCanBeFoundByIndexAndName)
{
    TestAtlas atlas;
    const auto idx = atlas.add_object("a", 7);
    ASSERT_NE(idx, ENGINE_INVALID_OBJECT_HANDLE);
    ASSERT_NE(atlas.get_object(idx), nullptr);
    EXPECT_EQ(*atlas.get_object(idx), 7);
    EXPECT_EQ(atlas.get_object("a"), idx);
    EXPECT_EQ(atlas.get_object_name(idx), "a");
    atlas.remove_object(idx);
}

TEST_F(AtlasTest, DuplicateNameIsRejected)
{
    TestAtlas atlas;
    const auto first = atlas.add_object("a", 1);
    const auto second = atlas.add_object("a", 2);
    EXPECT_EQ(second, ENGINE_INVALID_OBJECT_HANDLE);
    ASSERT_EQ(G_LOGGED_MESSAGES.size(), 1u);
    EXPECT_NE(G_LOGGED_MESSAGES.front().find("\"a\" already exists"), std::string::npos);
    // the original object is untouched
    EXPECT_EQ(atlas.get_object("a"), first);
    EXPECT_EQ(*atlas.get_object(first), 1);
    atlas.remove_object(first);
}

TEST_F(AtlasTest, EmptyNameIsRejected)
{
    TestAtlas atlas;
    EXPECT_EQ(atlas.add_object("", 1), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(G_LOGGED_MESSAGES.size(), 1u);
    // an empty name never matches, not even free slots
    EXPECT_EQ(atlas.get_object(""), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(AtlasTest, NameIsCopiedFromNonNullTerminatedView)
{
    TestAtlas atlas;
    const std::string storage = "abcdef";
    const auto idx = atlas.add_object(std::string_view(storage).substr(0, 3), 1);
    EXPECT_EQ(atlas.get_object_name(idx), "abc");
    EXPECT_EQ(atlas.get_object("abc"), idx);
    EXPECT_EQ(atlas.get_object("abcdef"), ENGINE_INVALID_OBJECT_HANDLE);
    atlas.remove_object(idx);
}

TEST_F(AtlasTest, RemoveByNameFreesTheName)
{
    TestAtlas atlas;
    const auto idx = atlas.add_object("a", 1);
    atlas.remove_object("a");
    EXPECT_EQ(atlas.get_object("a"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(atlas.get_object(idx), nullptr);
    // the name can be used again
    const auto again = atlas.add_object("a", 2);
    ASSERT_NE(again, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(*atlas.get_object(again), 2);
    atlas.remove_object(again);
}

TEST_F(AtlasTest, RemovedSlotIsReused)
{
    TestAtlas atlas;
    const auto a = atlas.add_object("a", 1);
    const auto b = atlas.add_object("b", 2);
    atlas.remove_object(a);
    const auto c = atlas.add_object("c", 3);
    EXPECT_EQ(c, a);
    EXPECT_EQ(atlas.get_object("a"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(atlas.get_object("b"), b);
    EXPECT_EQ(atlas.get_object("c"), c);
    atlas.remove_object(b);
    atlas.remove_object(c);
}

TEST_F(AtlasTest, FreeSlotIsNotFound)
{
    TestAtlas atlas;
    EXPECT_EQ(atlas.get_object(0u), nullptr);
    EXPECT_EQ(atlas.get_object_name(0u), "");
    EXPECT_EQ(atlas.get_object("missing"), ENGINE_INVALID_OBJECT_HANDLE);
}

}  // namespace
