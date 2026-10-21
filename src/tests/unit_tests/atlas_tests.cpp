#include "named_atlas.h"
#include "test_logger.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
using TestAtlas = engine::Atlas<int, 4>;

class AtlasTest : public ::testing::Test
{
protected:
    void SetUp() override { test_logger::messages().clear(); }
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
    atlas.release(idx);
}

TEST_F(AtlasTest, DuplicateNameIsRejected)
{
    TestAtlas atlas;
    const auto first = atlas.add_object("a", 1);
    const auto second = atlas.add_object("a", 2);
    EXPECT_EQ(second, ENGINE_INVALID_OBJECT_HANDLE);
    ASSERT_EQ(test_logger::messages().size(), 1u);
    EXPECT_NE(test_logger::messages().front().find("\"a\" already exists"), std::string::npos);
    // the original object is untouched
    EXPECT_EQ(atlas.get_object("a"), first);
    EXPECT_EQ(*atlas.get_object(first), 1);
    atlas.release(first);
}

TEST_F(AtlasTest, EmptyNameIsRejected)
{
    TestAtlas atlas;
    EXPECT_EQ(atlas.add_object("", 1), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(test_logger::messages().size(), 1u);
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
    atlas.release(idx);
}

TEST_F(AtlasTest, ReleaseFreesTheName)
{
    TestAtlas atlas;
    const auto idx = atlas.add_object("a", 1);
    atlas.release(idx);
    EXPECT_EQ(atlas.get_object("a"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(atlas.get_object(idx), nullptr);
    // the name can be used again
    const auto again = atlas.add_object("a", 2);
    ASSERT_NE(again, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(*atlas.get_object(again), 2);
    atlas.release(again);
}

TEST_F(AtlasTest, ObjectIsDestroyedWithItsLastReference)
{
    TestAtlas atlas;
    const auto idx = atlas.add_object("a", 1);
    EXPECT_EQ(atlas.get_reference_count(idx), 1u);
    EXPECT_TRUE(atlas.acquire(idx));
    EXPECT_EQ(atlas.get_reference_count(idx), 2u);
    atlas.release(idx);
    ASSERT_NE(atlas.get_object(idx), nullptr);
    EXPECT_EQ(atlas.get_object("a"), idx);
    atlas.release(idx);
    EXPECT_EQ(atlas.get_object(idx), nullptr);
    EXPECT_EQ(atlas.get_object("a"), ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(atlas.get_reference_count(idx), 0u);
}

TEST_F(AtlasTest, ReleasedHandleStaysInvalidWhenItsSlotIsReused)
{
    TestAtlas atlas;
    const auto a = atlas.add_object("a", 1);
    atlas.release(a);
    // fill every slot, so one of them is a's
    std::vector<std::uint32_t> handles;
    for (int i = 0; i < 4; i++)
    {
        handles.push_back(atlas.add_object("o" + std::to_string(i), int(i)));
        ASSERT_NE(handles.back(), ENGINE_INVALID_OBJECT_HANDLE);
        EXPECT_NE(handles.back(), a);
    }
    EXPECT_EQ(atlas.get_object(a), nullptr);
    EXPECT_EQ(atlas.get_object_name(a), "");
    EXPECT_EQ(atlas.get_reference_count(a), 0u);
    for (const auto handle : handles)
    {
        atlas.release(handle);
    }
}

TEST_F(AtlasTest, ReleasedSlotIsReusedLast)
{
    // a handle released by mistake isn't followed by a new object in its slot right away
    TestAtlas atlas;
    const auto a = atlas.add_object("a", 1);
    atlas.release(a);
    const auto b = atlas.add_object("b", 2);
    // the slot is the low bits of a handle
    EXPECT_NE(b & 3u, a & 3u);
    atlas.release(b);
}

TEST_F(AtlasTest, InvalidHandleCanNotBeAcquiredOrReleased)
{
    TestAtlas atlas;
    const auto a = atlas.add_object("a", 1);
    const auto b = atlas.add_object("b", 2);
    atlas.release(a);
    test_logger::messages().clear();
    EXPECT_FALSE(atlas.acquire(a));
    // released once too many: logged, and the other objects are untouched
    atlas.release(a);
    atlas.release(ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(test_logger::messages().size(), 3u);
    EXPECT_EQ(atlas.get_reference_count(b), 1u);
    EXPECT_EQ(*atlas.get_object(b), 2);
    atlas.release(b);
}

TEST_F(AtlasTest, FirstObjectsHandleIsZero)
{
    // the application's default texture is the first one added and materials refer to it as 0
    TestAtlas atlas;
    const auto a = atlas.add_object("a", 1);
    EXPECT_EQ(a, 0u);
    atlas.release(a);
}

TEST_F(AtlasTest, FreeSlotIsNotFound)
{
    TestAtlas atlas;
    EXPECT_EQ(atlas.get_object(0u), nullptr);
    EXPECT_EQ(atlas.get_object_name(0u), "");
    EXPECT_EQ(atlas.get_object("missing"), ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(AtlasTest, InvalidHandleHasNoName)
{
    TestAtlas atlas;
    EXPECT_EQ(atlas.get_object_name(ENGINE_INVALID_OBJECT_HANDLE), "");
}

TEST_F(AtlasTest, ObjectNamesListsOccupiedSlotsInIndexOrder)
{
    TestAtlas atlas;
    const auto a = atlas.add_object("a", 1);
    const auto b = atlas.add_object("b", 2);
    const auto c = atlas.add_object("c", 3);
    atlas.release(b);
    using Names = std::vector<std::pair<std::uint32_t, std::string>>;
    EXPECT_EQ(atlas.get_object_names(), (Names{ { a, "a" }, { c, "c" } }));
    atlas.release(a);
    atlas.release(c);
    EXPECT_TRUE(atlas.get_object_names().empty());
}

}  // namespace
