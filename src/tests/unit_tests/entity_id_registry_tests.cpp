#include "entity_id_registry.h"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

namespace
{
using engine::EntityIdRegistry;

class EntityIdRegistryTest : public ::testing::Test
{
protected:
    entt::registry registry_;
    EntityIdRegistry ids_{ registry_ };
};

TEST_F(EntityIdRegistryTest, IdsStartAtOneAndIncrease)
{
    const auto a = registry_.create();
    const auto b = registry_.create();
    EXPECT_EQ(ids_.assign_next(a), 1u);
    EXPECT_EQ(ids_.assign_next(b), 2u);
    EXPECT_EQ(ids_.get_id(a), 1u);
    EXPECT_EQ(ids_.get_id(b), 2u);
}

TEST_F(EntityIdRegistryTest, IdFindsItsEntity)
{
    const auto a = registry_.create();
    const auto id = ids_.assign_next(a);
    EXPECT_EQ(ids_.find(id), a);
    EXPECT_EQ(ids_.find(id + 1), entt::entity{ entt::null });
    EXPECT_EQ(ids_.find(EntityIdRegistry::INVALID_ID), entt::entity{ entt::null });
}

TEST_F(EntityIdRegistryTest, EntityWithoutIdHasInvalidId)
{
    const auto a = registry_.create();
    EXPECT_EQ(ids_.get_id(a), EntityIdRegistry::INVALID_ID);
    EXPECT_EQ(ids_.get_id(entt::null), EntityIdRegistry::INVALID_ID);
}

TEST_F(EntityIdRegistryTest, SameCreationOrderGivesSameIds)
{
    const auto build = []()
    {
        entt::registry registry;
        EntityIdRegistry ids(registry);
        std::vector<std::uint64_t> out;
        for (int i = 0; i < 10; i++)
        {
            const auto e = registry.create();
            out.push_back(ids.assign_next(e));
            if (i % 3 == 0)
            {
                registry.destroy(e);
            }
        }
        return out;
    };
    EXPECT_EQ(build(), build());
}

TEST_F(EntityIdRegistryTest, DestroyedEntityReleasesItsIdButItIsNotReused)
{
    const auto a = registry_.create();
    const auto id_a = ids_.assign_next(a);
    registry_.destroy(a);
    EXPECT_EQ(ids_.find(id_a), entt::entity{ entt::null });

    // entt recycles the entity slot, the ID moves on
    const auto b = registry_.create();
    EXPECT_GT(ids_.assign_next(b), id_a);
}

TEST_F(EntityIdRegistryTest, KnownIdCanBeAssignedAndCounterSkipsPastIt)
{
    const auto a = registry_.create();
    ASSERT_TRUE(ids_.assign(a, 42));
    EXPECT_EQ(ids_.find(42), a);

    const auto b = registry_.create();
    EXPECT_EQ(ids_.assign_next(b), 43u);
}

TEST_F(EntityIdRegistryTest, CounterSkipsKnownIdsAssignedAhead)
{
    const auto a = registry_.create();
    ASSERT_TRUE(ids_.assign(a, 1));
    // a lower ID assigned later must not move the counter back
    const auto b = registry_.create();
    ASSERT_TRUE(ids_.assign(b, 5));
    const auto c = registry_.create();
    EXPECT_EQ(ids_.assign_next(c), 6u);
}

TEST_F(EntityIdRegistryTest, ReleasedIdCanBeAssignedAgainExplicitly)
{
    // e.g. undo of a destroy restores the entity with its old ID
    const auto a = registry_.create();
    const auto id = ids_.assign_next(a);
    registry_.destroy(a);
    const auto restored = registry_.create();
    ASSERT_TRUE(ids_.assign(restored, id));
    EXPECT_EQ(ids_.find(id), restored);
}

TEST_F(EntityIdRegistryTest, AssignRejectsInvalidUsedOrDoubleIds)
{
    const auto a = registry_.create();
    const auto b = registry_.create();
    EXPECT_FALSE(ids_.assign(a, EntityIdRegistry::INVALID_ID));
    ASSERT_TRUE(ids_.assign(a, 7));
    EXPECT_FALSE(ids_.assign(b, 7));  // used by a
    EXPECT_FALSE(ids_.assign(a, 8));  // a already has an ID
    EXPECT_EQ(ids_.get_id(a), 7u);
    EXPECT_EQ(ids_.get_id(b), EntityIdRegistry::INVALID_ID);
    EXPECT_EQ(ids_.find(8), entt::entity{ entt::null });
}

TEST_F(EntityIdRegistryTest, LargestIdIsRejectedSoTheCounterNeverWraps)
{
    const auto a = registry_.create();
    EXPECT_FALSE(ids_.assign(a, std::numeric_limits<std::uint64_t>::max()));
    ASSERT_TRUE(ids_.assign(a, EntityIdRegistry::MAX_ID));

    // the counter is past the last valid ID: no more IDs, and never the reserved 0
    const auto b = registry_.create();
    EXPECT_EQ(ids_.assign_next(b), EntityIdRegistry::INVALID_ID);
    EXPECT_EQ(ids_.get_id(b), EntityIdRegistry::INVALID_ID);
    EXPECT_EQ(ids_.find(EntityIdRegistry::INVALID_ID), entt::entity{ entt::null });
}

TEST_F(EntityIdRegistryTest, AssignNextFailsForEntityThatCantTakeAnId)
{
    const auto a = registry_.create();
    const auto id = ids_.assign_next(a);
    // already has one
    EXPECT_EQ(ids_.assign_next(a), EntityIdRegistry::INVALID_ID);
    EXPECT_EQ(ids_.get_id(a), id);
    // dead
    const auto b = registry_.create();
    registry_.destroy(b);
    EXPECT_EQ(ids_.assign_next(b), EntityIdRegistry::INVALID_ID);
}

TEST_F(EntityIdRegistryTest, AssignRejectsDeadEntity)
{
    const auto a = registry_.create();
    registry_.destroy(a);
    EXPECT_FALSE(ids_.assign(a, 3));
    EXPECT_EQ(ids_.find(3), entt::entity{ entt::null });
}

TEST_F(EntityIdRegistryTest, ClearingTheRegistryReleasesAllIds)
{
    const auto id = ids_.assign_next(registry_.create());
    registry_.clear();
    EXPECT_EQ(ids_.find(id), entt::entity{ entt::null });
}
}  // namespace
