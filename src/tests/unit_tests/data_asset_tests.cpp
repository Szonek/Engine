#include "data_asset.h"
#include "serialization/game_types.h"
#include "serialization/scene_serializer.h"
#include "test_logger.h"

#include <engine.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <string_view>

// Data assets: a value of a game-defined struct type in its own file (save_data_asset / load_data_asset), and the
// DataAsset holding it, read again when its file changes.
namespace
{
using engine::AssetKind;

struct Tuning
{
    float speed;
    std::uint32_t hp;
    float color[3];
    engine_texture2d_t icon;
};

// asset IDs <-> handles of textures, nothing is ever loaded; counts how many resolvers are alive (the assets they hold)
class FakeAssets : public engine::IAssetResolver
{
public:
    explicit FakeAssets(int* alive = nullptr) : alive_(alive)
    {
        if (alive_)
        {
            (*alive_)++;
        }
    }
    ~FakeAssets() override
    {
        if (alive_)
        {
            (*alive_)--;
        }
    }

    std::string get_asset_id(AssetKind kind, std::uint32_t handle) const override
    {
        return kind == AssetKind::eTexture && handle == 4 ? "textures/icon.png" : "";
    }
    std::uint32_t resolve(AssetKind kind, std::string_view asset_id) override
    {
        return kind == AssetKind::eTexture && asset_id == "textures/icon.png" ? 4 : ENGINE_INVALID_OBJECT_HANDLE;
    }
    const engine::TypeInfo* get_data_asset_type(std::uint32_t) const override { return nullptr; }

private:
    int* alive_;
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

class DataAssetTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_logger::messages().clear();
        const engine_field_desc_t fields[] = {
            field("speed", ENGINE_FIELD_KIND_FLOAT, offsetof(Tuning, speed)),
            field("hp", ENGINE_FIELD_KIND_UINT32, offsetof(Tuning, hp)),
            field("color", ENGINE_FIELD_KIND_COLOR, offsetof(Tuning, color), 3),
            field("icon", ENGINE_FIELD_KIND_TEXTURE, offsetof(Tuning, icon)),
        };
        const Tuning defaults{ 2.0f, 20, { 1.0f, 0.5f, 0.25f }, ENGINE_INVALID_OBJECT_HANDLE };
        const engine_type_desc_t desc{ "tuning", 1, sizeof(Tuning), alignof(Tuning), &defaults, fields, 4 };
        tuning_ = engine::register_game_type(types_, desc, false);
        ASSERT_NE(tuning_, nullptr);
        file_ = std::filesystem::temp_directory_path() / "engine_logic_tests_data_asset.json";
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove(file_, error);
    }

    void write_file(std::string_view json) const
    {
        std::ofstream(file_, std::ios::binary) << json;
    }

    std::unique_ptr<engine::DataAsset> load_asset()
    {
        return engine::DataAsset::load("tuning.json", file_, types_, [this] { return std::make_unique<FakeAssets>(&alive_resolvers_); });
    }

    static Tuning value_of(const engine::DataAsset& asset)
    {
        Tuning t{};
        std::memcpy(&t, asset.get_value(), sizeof(t));
        return t;
    }

    engine::TypeRegistry types_;
    const engine::TypeInfo* tuning_ = nullptr;
    FakeAssets assets_;
    std::filesystem::path file_;
    int alive_resolvers_ = 0;
};

TEST_F(DataAssetTest, ValueRoundTrips)
{
    const Tuning original{ 0.1f, 7, { 0.0f, 1.0f, 0.5f }, 4 };
    const auto file = engine::save_data_asset(*tuning_, reinterpret_cast<const std::byte*>(&original), assets_);
    EXPECT_NE(file.find(R"("type": "tuning")"), std::string::npos) << file;
    EXPECT_NE(file.find(R"("icon": "textures/icon.png")"), std::string::npos) << file;

    const auto loaded = engine::load_data_asset(file, "tuning.json", types_, assets_);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->type, tuning_);
    EXPECT_EQ(engine::save_data_asset(*loaded->type, loaded->value->get(), assets_), file);
    EXPECT_TRUE(test_logger::messages().empty()) << test_logger::messages().front();
}

TEST_F(DataAssetTest, MissingFieldsKeepTheTypesDefault)
{
    const auto loaded = engine::load_data_asset(R"({ "version": 1, "type": "tuning", "value": { "hp": 50 } })", "tuning.json", types_, assets_);
    ASSERT_TRUE(loaded.has_value());
    Tuning t{};
    std::memcpy(&t, loaded->value->get(), sizeof(t));
    EXPECT_EQ(t.hp, 50u);
    EXPECT_EQ(t.speed, 2.0f);
    EXPECT_EQ(t.color[1], 0.5f);
    EXPECT_EQ(t.icon, ENGINE_INVALID_OBJECT_HANDLE);
}

TEST_F(DataAssetTest, ProblemsInsideTheValueAreLoggedAndSkipped)
{
    const auto loaded = engine::load_data_asset(R"({ "version": 1, "type": "tuning", "value": { "hp": "many", "range": 3 } })", "tuning.json", types_, assets_);
    ASSERT_TRUE(loaded.has_value());
    Tuning t{};
    std::memcpy(&t, loaded->value->get(), sizeof(t));
    EXPECT_EQ(t.hp, 20u);
    EXPECT_TRUE(test_logger::logged("Data asset tuning.json: tuning.hp is not a number"));
    EXPECT_TRUE(test_logger::logged("unknown field \"range\""));
}

TEST_F(DataAssetTest, FileWhichIsNoDataAssetIsNotLoaded)
{
    EXPECT_FALSE(engine::load_data_asset("{ not json", "a.json", types_, assets_));
    EXPECT_FALSE(engine::load_data_asset(R"({ "type": "tuning", "value": {} })", "b.json", types_, assets_));
    EXPECT_FALSE(engine::load_data_asset(R"({ "version": 2, "type": "tuning", "value": {} })", "c.json", types_, assets_));
    EXPECT_FALSE(engine::load_data_asset(R"({ "version": 1, "type": "nothing", "value": {} })", "d.json", types_, assets_));
    EXPECT_FALSE(engine::load_data_asset(R"({ "version": 1, "type": "tuning" })", "e.json", types_, assets_));
    EXPECT_TRUE(test_logger::logged("Data asset d.json: its type isn't a struct type"));
}

TEST_F(DataAssetTest, TypeWithGameObjectReferencesCantBeADataAsset)
{
    struct Target
    {
        engine_game_object_t go;
    };
    const engine_field_desc_t go = field("go", ENGINE_FIELD_KIND_GAME_OBJECT, offsetof(Target, go));
    const engine_type_desc_t desc{ "target", 1, sizeof(Target), alignof(Target), nullptr, &go, 1 };
    ASSERT_NE(engine::register_game_type(types_, desc, false), nullptr);
    EXPECT_FALSE(engine::load_data_asset(R"({ "version": 1, "type": "target", "value": {} })", "t.json", types_, assets_));
}

TEST_F(DataAssetTest, ChangedFileIsReadAgainIntoTheSameAsset)
{
    write_file(R"({ "version": 1, "type": "tuning", "value": { "hp": 30, "icon": "textures/icon.png" } })");
    auto asset = load_asset();
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(value_of(*asset).hp, 30u);
    EXPECT_EQ(alive_resolvers_, 1);

    // nothing happens until the watcher reports a change
    write_file(R"({ "version": 1, "type": "tuning", "value": { "hp": 40 } })");
    EXPECT_FALSE(asset->reload_if_changed());
    EXPECT_EQ(value_of(*asset).hp, 30u);

    asset->mark_changed();
    EXPECT_TRUE(asset->reload_if_changed());
    EXPECT_EQ(value_of(*asset).hp, 40u);
    EXPECT_EQ(value_of(*asset).icon, ENGINE_INVALID_OBJECT_HANDLE);
    // the old value's assets were released with its resolver
    EXPECT_EQ(alive_resolvers_, 1);

    asset.reset();
    EXPECT_EQ(alive_resolvers_, 0);
}

TEST_F(DataAssetTest, BrokenFileKeepsTheValue)
{
    write_file(R"({ "version": 1, "type": "tuning", "value": { "hp": 30 } })");
    auto asset = load_asset();
    ASSERT_NE(asset, nullptr);

    write_file(R"({ "version": 1, "type": "tuning", "value": { "hp": )");
    asset->mark_changed();
    EXPECT_FALSE(asset->reload_if_changed());
    EXPECT_EQ(value_of(*asset).hp, 30u);
    EXPECT_TRUE(test_logger::logged("keeps its value until its file can be read again"));

    // fixed: read on the next change
    write_file(R"({ "version": 1, "type": "tuning", "value": { "hp": 31 } })");
    asset->mark_changed();
    EXPECT_TRUE(asset->reload_if_changed());
    EXPECT_EQ(value_of(*asset).hp, 31u);
}

TEST_F(DataAssetTest, FileHoldingAnotherTypeKeepsTheValue)
{
    struct Other
    {
        float x;
    };
    const engine_field_desc_t x = field("x", ENGINE_FIELD_KIND_FLOAT, offsetof(Other, x));
    const engine_type_desc_t desc{ "other", 1, sizeof(Other), alignof(Other), nullptr, &x, 1 };
    ASSERT_NE(engine::register_game_type(types_, desc, false), nullptr);

    write_file(R"({ "version": 1, "type": "tuning", "value": { "hp": 30 } })");
    auto asset = load_asset();
    ASSERT_NE(asset, nullptr);
    write_file(R"({ "version": 1, "type": "other", "value": { "x": 1 } })");
    asset->mark_changed();
    EXPECT_FALSE(asset->reload_if_changed());
    EXPECT_EQ(&asset->get_type(), tuning_);
    EXPECT_EQ(value_of(*asset).hp, 30u);
    EXPECT_TRUE(test_logger::logged("now holds a other instead of a tuning"));
}

TEST_F(DataAssetTest, MissingFileIsNotLoaded)
{
    EXPECT_EQ(load_asset(), nullptr);
    EXPECT_TRUE(test_logger::logged("can't read"));
}
}  // namespace
