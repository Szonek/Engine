#include "serialization/builtin_types.h"
#include "serialization/game_types.h"
#include "test_logger.h"

#include <engine.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

// Types the game registers through the C API (engineComponentTypeRegister): how a description becomes a TypeInfo, the
// mistakes it catches, and the storage of the components.
namespace
{
using engine::FieldKind;
using engine::TypeInfo;

enum Mood { MOOD_CALM = 0, MOOD_ANGRY = 3 };

struct Circle
{
    float radius;
};

struct Box
{
    float size[2];
};

// a component with a field of every kind
struct EnemyConfig
{
    float max_hp;
    std::int32_t level;
    std::uint32_t gold;
    bool boss;
    char title[15];
    Mood mood;
    engine_game_object_t target;
    engine_geometry_t weapon;
    engine_texture2d_t skin;
    float offset[3];
    Circle aura;
    Box zone;
};

engine_field_desc_t field(const char* name, engine_field_kind_t kind, std::size_t offset, std::uint32_t count = 0)
{
    engine_field_desc_t f{};
    f.name = name;
    f.kind = kind;
    f.offset = static_cast<std::uint32_t>(offset);
    f.count = count;
    return f;
}

const engine_enum_value_desc_t MOODS[] = { { "calm", MOOD_CALM }, { "angry", MOOD_ANGRY } };

class GameTypesTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_logger::messages().clear();
        registry_.create();  // entity 0 is reserved in a scene
    }

    template<typename T>
    engine_type_desc_t desc(const char* name, const std::vector<engine_field_desc_t>& fields, const T* default_value = nullptr)
    {
        engine_type_desc_t d{};
        d.name = name;
        d.size = sizeof(T);
        d.alignment = alignof(T);
        d.default_value = default_value;
        d.fields = fields.data();
        d.fields_count = static_cast<std::uint32_t>(fields.size());
        return d;
    }

    // the struct types EnemyConfig uses
    void register_struct_types()
    {
        const std::vector<engine_field_desc_t> circle = { field("radius", ENGINE_FIELD_KIND_FLOAT, offsetof(Circle, radius)) };
        ASSERT_NE(engine::register_game_type(types_, desc<Circle>("circle", circle), false), nullptr);
        const std::vector<engine_field_desc_t> box = { field("size", ENGINE_FIELD_KIND_FLOAT, offsetof(Box, size), 2) };
        ASSERT_NE(engine::register_game_type(types_, desc<Box>("box", box), false), nullptr);
    }

    std::vector<engine_field_desc_t> enemy_config_fields()
    {
        auto mood = field("mood", ENGINE_FIELD_KIND_ENUM, offsetof(EnemyConfig, mood));
        mood.enum_values = MOODS;
        mood.enum_values_count = 2;
        auto aura = field("aura", ENGINE_FIELD_KIND_STRUCT, offsetof(EnemyConfig, aura));
        aura.type_name = "circle";
        auto zone = field("zone", ENGINE_FIELD_KIND_STRUCT, offsetof(EnemyConfig, zone));
        zone.type_name = "box";
        return {
            field("max_hp", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyConfig, max_hp)),
            field("level", ENGINE_FIELD_KIND_INT32, offsetof(EnemyConfig, level)),
            field("gold", ENGINE_FIELD_KIND_UINT32, offsetof(EnemyConfig, gold)),
            field("boss", ENGINE_FIELD_KIND_BOOL, offsetof(EnemyConfig, boss)),
            field("title", ENGINE_FIELD_KIND_STRING, offsetof(EnemyConfig, title), sizeof(EnemyConfig::title)),
            mood,
            field("target", ENGINE_FIELD_KIND_GAME_OBJECT, offsetof(EnemyConfig, target)),
            field("weapon", ENGINE_FIELD_KIND_GEOMETRY, offsetof(EnemyConfig, weapon)),
            field("skin", ENGINE_FIELD_KIND_TEXTURE, offsetof(EnemyConfig, skin)),
            field("offset", ENGINE_FIELD_KIND_FLOAT, offsetof(EnemyConfig, offset), 3),
            aura,
            zone,
        };
    }

    const TypeInfo* register_enemy_config(const EnemyConfig* default_value = nullptr)
    {
        register_struct_types();
        const auto fields = enemy_config_fields();
        return engine::register_game_type(types_, desc<EnemyConfig>("enemy_config", fields, default_value), true);
    }

    // whether a component with these fields, size and alignment registers
    bool registers(std::vector<engine_field_desc_t> fields, std::uint32_t size = sizeof(EnemyConfig), std::uint32_t alignment = alignof(EnemyConfig))
    {
        auto d = desc<EnemyConfig>("broken", fields);
        d.size = size;
        d.alignment = alignment;
        return engine::register_game_type(types_, d, true) != nullptr;
    }

    engine::TypeRegistry types_;
    entt::registry registry_;
};

// -- descriptions ---------------------------------------------------------------------------------------------------

TEST_F(GameTypesTest, DescriptionBecomesATypeWithEveryField)
{
    const auto* type = register_enemy_config();
    ASSERT_NE(type, nullptr);
    EXPECT_TRUE(test_logger::messages().empty());
    EXPECT_TRUE(type->is_component());
    EXPECT_TRUE(type->game_defined);
    EXPECT_EQ(type->version, 1u);
    EXPECT_EQ(type->size, sizeof(EnemyConfig));
    EXPECT_EQ(types_.find("enemy_config"), type);
    EXPECT_EQ(types_.get(type->handle), type);
    ASSERT_EQ(type->fields.size(), 12u);

    const auto* title = type->find_field("title");
    EXPECT_EQ(title->kind, FieldKind::eString);
    EXPECT_FALSE(title->is_array);
    EXPECT_EQ(title->size, sizeof(EnemyConfig::title));

    const auto* offset = type->find_field("offset");
    EXPECT_EQ(offset->kind, FieldKind::eFloat);
    EXPECT_TRUE(offset->is_array);
    EXPECT_EQ(offset->count, 3u);

    EXPECT_EQ(type->find_field("weapon")->kind, FieldKind::eAsset);
    EXPECT_EQ(type->find_field("weapon")->asset_kind, engine::AssetKind::eGeometry);
    EXPECT_EQ(type->find_field("skin")->asset_kind, engine::AssetKind::eTexture);
    EXPECT_EQ(type->find_field("target")->kind, FieldKind::eGameObject);
    EXPECT_EQ(type->find_field("aura")->struct_type, types_.find("circle"));
    EXPECT_EQ(type->find_field("zone")->struct_type, types_.find("box"));
}

TEST_F(GameTypesTest, StructTypesHaveNoStorage)
{
    register_struct_types();
    EXPECT_FALSE(types_.find("circle")->is_component());
    EXPECT_TRUE(types_.find("circle")->game_defined);
}

TEST_F(GameTypesTest, GameTypesAreLoadedAfterTheEngineComponents)
{
    engine::register_builtin_types(types_);
    register_enemy_config();
    const auto components = types_.get_components();
    ASSERT_FALSE(components.empty());
    EXPECT_EQ(components.back()->name, "enemy_config");
    EXPECT_FALSE(types_.find("transform")->game_defined);
}

TEST_F(GameTypesTest, NameCanBeRegisteredOnceAlsoAgainstTheEngine)
{
    engine::register_builtin_types(types_);
    const std::vector<engine_field_desc_t> fields = { field("max_hp", ENGINE_FIELD_KIND_FLOAT, 0) };
    EXPECT_EQ(engine::register_game_type(types_, desc<EnemyConfig>("transform", fields), true), nullptr);
    EXPECT_TRUE(test_logger::logged("registered already"));
}

TEST_F(GameTypesTest, ComponentTooBigOrBadlyAlignedIsRejected)
{
    const std::vector<engine_field_desc_t> fields = { field("max_hp", ENGINE_FIELD_KIND_FLOAT, 0) };
    EXPECT_FALSE(registers(fields, ENGINE_COMPONENT_TYPE_MAX_SIZE + 4, 4));
    EXPECT_TRUE(test_logger::logged("a component is at most"));
    EXPECT_FALSE(registers(fields, 12, 3));
    EXPECT_FALSE(registers(fields, 64, 32));
    EXPECT_FALSE(registers(fields, 10, 4));
    EXPECT_FALSE(registers(fields, 0, 4));
}

TEST_F(GameTypesTest, FieldMistakesAreRejected)
{
    // outside the struct
    EXPECT_FALSE(registers({ field("max_hp", ENGINE_FIELD_KIND_FLOAT, sizeof(EnemyConfig)) }));
    EXPECT_TRUE(test_logger::logged("outside the struct"));
    EXPECT_FALSE(registers({ field("offset", ENGINE_FIELD_KIND_FLOAT, sizeof(EnemyConfig) - 8, 3) }));
    // misaligned
    EXPECT_FALSE(registers({ field("max_hp", ENGINE_FIELD_KIND_FLOAT, 2) }));
    EXPECT_TRUE(test_logger::logged("not aligned"));
    // overlapping (a wrong offset copied from the field before)
    EXPECT_FALSE(registers({ field("max_hp", ENGINE_FIELD_KIND_FLOAT, 0), field("level", ENGINE_FIELD_KIND_INT32, 0) }));
    EXPECT_TRUE(test_logger::logged("overlap"));
    // twice, without a name, unknown kind
    EXPECT_FALSE(registers({ field("max_hp", ENGINE_FIELD_KIND_FLOAT, 0), field("max_hp", ENGINE_FIELD_KIND_FLOAT, 4) }));
    EXPECT_TRUE(test_logger::logged("described twice"));
    EXPECT_FALSE(registers({ field("", ENGINE_FIELD_KIND_FLOAT, 0) }));
    EXPECT_FALSE(registers({ field("max_hp", ENGINE_FIELD_KIND_COUNT, 0) }));
    EXPECT_TRUE(test_logger::logged("unknown kind"));
    // a string without its size
    EXPECT_FALSE(registers({ field("title", ENGINE_FIELD_KIND_STRING, offsetof(EnemyConfig, title)) }));
    EXPECT_TRUE(test_logger::logged("size of its char array"));
    EXPECT_EQ(types_.find("broken"), nullptr);
}

TEST_F(GameTypesTest, EnumNeedsUniqueNamedValues)
{
    auto mood = field("mood", ENGINE_FIELD_KIND_ENUM, offsetof(EnemyConfig, mood));
    EXPECT_FALSE(registers({ mood }));
    EXPECT_TRUE(test_logger::logged("has no values"));

    const engine_enum_value_desc_t same_value[] = { { "calm", 0 }, { "quiet", 0 } };
    mood.enum_values = same_value;
    mood.enum_values_count = 2;
    EXPECT_FALSE(registers({ mood }));
    EXPECT_TRUE(test_logger::logged("names and values are unique"));
}

TEST_F(GameTypesTest, StructFieldNeedsAStructTypeOfTheGame)
{
    auto aura = field("aura", ENGINE_FIELD_KIND_STRUCT, offsetof(EnemyConfig, aura));
    aura.type_name = "circle";
    EXPECT_FALSE(registers({ aura }));
    EXPECT_TRUE(test_logger::logged("is not a struct type registered by the game"));

    // the engine's struct types may hold unions, which game types don't support; a component isn't a struct type
    engine::register_builtin_types(types_);
    aura.type_name = "sphere_collider";
    EXPECT_FALSE(registers({ aura }));
    const std::vector<engine_field_desc_t> ring = { field("radius", ENGINE_FIELD_KIND_FLOAT, 0) };
    ASSERT_NE(engine::register_game_type(types_, desc<Circle>("ring", ring), true), nullptr);
    aura.type_name = "ring";
    EXPECT_FALSE(registers({ aura }));
}

TEST_F(GameTypesTest, FieldAlignedMoreThanTheStructIsRejected)
{
    // a struct claiming alignment 1 while holding a float: a field of it could be placed at any offset
    const std::vector<engine_field_desc_t> fields = { field("radius", ENGINE_FIELD_KIND_FLOAT, 0) };
    auto d = desc<Circle>("packed", fields);
    d.alignment = 1;
    EXPECT_EQ(engine::register_game_type(types_, d, false), nullptr);
    EXPECT_TRUE(test_logger::logged("more than the struct's alignment"));
}

TEST_F(GameTypesTest, NamesSharingAStorageIdAreRejected)
{
    // these two hash to the same id ("game_component/<name>", FNV-1a): they would share one storage
    const std::vector<engine_field_desc_t> fields = { field("radius", ENGINE_FIELD_KIND_FLOAT, 0) };
    ASSERT_NE(engine::register_game_type(types_, desc<Circle>("t981798", fields), true), nullptr);
    EXPECT_EQ(engine::register_game_type(types_, desc<Circle>("t1049482", fields), true), nullptr);
    EXPECT_TRUE(test_logger::logged("its storage id is the one of t981798"));
    // a struct type has no storage
    EXPECT_NE(engine::register_game_type(types_, desc<Circle>("t1049482", fields), false), nullptr);
}

TEST_F(GameTypesTest, VersionZeroIsOne)
{
    const std::vector<engine_field_desc_t> fields = { field("radius", ENGINE_FIELD_KIND_FLOAT, 0) };
    auto d = desc<Circle>("ring", fields);
    d.version = 0;
    EXPECT_EQ(engine::register_game_type(types_, d, true)->version, 1u);
    d.name = "ring2";
    d.version = 4;
    EXPECT_EQ(engine::register_game_type(types_, d, true)->version, 4u);
}

// -- storage --------------------------------------------------------------------------------------------------------

TEST_F(GameTypesTest, ComponentIsAddedWithItsDefaultValue)
{
    EnemyConfig defaults{};
    defaults.max_hp = 100.0f;
    defaults.mood = MOOD_ANGRY;
    std::strcpy(defaults.title, "grunt");
    const auto* type = register_enemy_config(&defaults);
    ASSERT_NE(type, nullptr);

    const auto e = registry_.create();
    EXPECT_FALSE(type->storage->has(registry_, e));
    type->storage->add(registry_, e);
    EXPECT_TRUE(type->storage->has(registry_, e));
    EnemyConfig value{};
    type->storage->get(registry_, e, &value);
    EXPECT_EQ(std::memcmp(&value, &defaults, sizeof(EnemyConfig)), 0);
}

TEST_F(GameTypesTest, ComponentWithoutDefaultValueIsZeroedAndReferencesNothing)
{
    const auto* type = register_enemy_config();
    const auto e = registry_.create();
    type->storage->add(registry_, e);
    EnemyConfig value;
    std::memset(&value, 0xff, sizeof(value));
    type->storage->get(registry_, e, &value);
    const EnemyConfig zero{};
    EXPECT_EQ(std::memcmp(&value, &zero, sizeof(EnemyConfig)), 0);
    // no asset or game object has the handle 0
    EXPECT_EQ(value.weapon, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(value.skin, ENGINE_INVALID_OBJECT_HANDLE);
    EXPECT_EQ(value.target, ENGINE_INVALID_GAME_OBJECT_ID);
}

TEST_F(GameTypesTest, UpdateChangesTheValue)
{
    const auto* type = register_enemy_config();
    const auto e = registry_.create();
    type->storage->add(registry_, e);
    EnemyConfig value{};
    value.level = 7;
    value.target = static_cast<engine_game_object_t>(e);
    type->storage->update(registry_, e, &value);
    EnemyConfig read{};
    type->storage->get(registry_, e, &read);
    EXPECT_EQ(read.level, 7);
    EXPECT_EQ(read.target, static_cast<engine_game_object_t>(e));
}

TEST_F(GameTypesTest, GetCopiesOnlyTheSizeOfTheType)
{
    // a 4 byte type in a 16 byte storage: nothing after the value is written
    const std::vector<engine_field_desc_t> fields = { field("radius", ENGINE_FIELD_KIND_FLOAT, 0) };
    const Circle defaults{ 2.5f };
    const auto* type = engine::register_game_type(types_, desc<Circle>("ring", fields, &defaults), true);
    const auto e = registry_.create();
    type->storage->add(registry_, e);
    struct { Circle value; std::uint32_t canary; } out{ {}, 0xdeadbeef };
    type->storage->get(registry_, e, &out.value);
    EXPECT_EQ(out.value.radius, 2.5f);
    EXPECT_EQ(out.canary, 0xdeadbeefu);
}

TEST_F(GameTypesTest, TypesOfTheSameSizeHaveTheirOwnStorage)
{
    const std::vector<engine_field_desc_t> fields = { field("radius", ENGINE_FIELD_KIND_FLOAT, 0) };
    const Circle a_default{ 1.0f };
    const Circle b_default{ 2.0f };
    const auto* a = engine::register_game_type(types_, desc<Circle>("ring_a", fields, &a_default), true);
    const auto* b = engine::register_game_type(types_, desc<Circle>("ring_b", fields, &b_default), true);
    const auto e = registry_.create();
    a->storage->add(registry_, e);
    EXPECT_FALSE(b->storage->has(registry_, e));
    b->storage->add(registry_, e);
    Circle value{};
    a->storage->get(registry_, e, &value);
    EXPECT_EQ(value.radius, 1.0f);
    b->storage->get(registry_, e, &value);
    EXPECT_EQ(value.radius, 2.0f);
}

TEST_F(GameTypesTest, ComponentIsRemovedAlsoWithItsGameObject)
{
    const auto* type = register_enemy_config();
    const auto removed = registry_.create();
    const auto destroyed = registry_.create();
    const auto kept = registry_.create();
    for (const auto e : { removed, destroyed, kept })
    {
        type->storage->add(registry_, e);
    }
    type->storage->remove(registry_, removed);
    registry_.destroy(destroyed);
    EXPECT_FALSE(type->storage->has(registry_, removed));
    EXPECT_FALSE(type->storage->has(registry_, destroyed));
    EXPECT_TRUE(type->storage->has(registry_, kept));
    // removing what isn't there does nothing
    type->storage->remove(registry_, removed);
    // a game object reusing the destroyed one's slot starts without it
    const auto reused = registry_.create();
    EXPECT_FALSE(type->storage->has(registry_, reused));
}

TEST_F(GameTypesTest, RegistryWithoutAnyComponentOfTheTypeHasNone)
{
    // the storage is created when the first component is added
    const auto* type = register_enemy_config();
    const auto e = registry_.create();
    EXPECT_FALSE(type->storage->has(registry_, e));
    type->storage->remove(registry_, e);
    EXPECT_FALSE(type->storage->has(registry_, e));
}

// N floats, for components of different sizes
template<std::size_t N>
struct Floats
{
    float values[N];
};

template<std::size_t N>
void expect_round_trip(engine::TypeRegistry& types, entt::registry& registry)
{
    const std::vector<engine_field_desc_t> fields = { field("values", ENGINE_FIELD_KIND_FLOAT, 0, N) };
    engine_type_desc_t d{};
    const auto name = "floats_" + std::to_string(N);
    d.name = name.c_str();
    d.size = sizeof(Floats<N>);
    d.alignment = alignof(Floats<N>);
    d.fields = fields.data();
    d.fields_count = 1;
    const auto* type = engine::register_game_type(types, d, true);
    ASSERT_NE(type, nullptr) << N;

    Floats<N> value{};
    for (std::size_t i = 0; i < N; i++)
    {
        value.values[i] = static_cast<float>(i) + 0.5f;
    }
    const auto e = registry.create();
    type->storage->add(registry, e);
    type->storage->update(registry, e, &value);
    // a value read back is the value written, and nothing after it is touched
    struct { Floats<N> value; std::uint32_t canary; } out{ {}, 0xdeadbeef };
    type->storage->get(registry, e, &out.value);
    EXPECT_EQ(std::memcmp(&out.value, &value, sizeof(value)), 0) << N;
    EXPECT_EQ(out.canary, 0xdeadbeefu) << N;
}

TEST_F(GameTypesTest, SlotIsTheSizeRoundedUpTo16Bytes)
{
    EXPECT_EQ(engine::data_slot_size(1), 16u);
    EXPECT_EQ(engine::data_slot_size(16), 16u);
    EXPECT_EQ(engine::data_slot_size(17), 32u);
    EXPECT_EQ(engine::data_slot_size(20), 32u);
    EXPECT_EQ(engine::data_slot_size(70), 80u);
    EXPECT_EQ(engine::data_slot_size(ENGINE_COMPONENT_TYPE_MAX_SIZE), static_cast<std::size_t>(ENGINE_COMPONENT_TYPE_MAX_SIZE));
    static_assert(sizeof(EnemyConfig) <= ENGINE_COMPONENT_TYPE_MAX_SIZE);
}

TEST_F(GameTypesTest, ComponentsOfEverySizeKeepTheirValues)
{
    // one per slot size range: 16, 32, 80, 128, 256 bytes
    expect_round_trip<1>(types_, registry_);
    expect_round_trip<5>(types_, registry_);
    expect_round_trip<17>(types_, registry_);
    expect_round_trip<32>(types_, registry_);
    expect_round_trip<ENGINE_COMPONENT_TYPE_MAX_SIZE / 4>(types_, registry_);
    EXPECT_TRUE(test_logger::messages().empty());
}
}  // namespace
