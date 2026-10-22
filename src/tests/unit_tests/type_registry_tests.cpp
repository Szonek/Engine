#include "serialization/builtin_types.h"
#include "serialization/type_registry.h"
#include "test_logger.h"

#include <engine.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
using engine::FieldKind;
using engine::TypeBuilder;
using engine::TypeInfo;
using engine::TypeRegistry;

class TypeRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override { test_logger::messages().clear(); }

    TypeRegistry types_;
};

// A description which leaves out a field leaves a gap in its struct bigger than the padding the compiler would put
// there (padding before a field is smaller than its alignment, padding at the end smaller than the struct's). Missing
// fields hidden in padding (a bool next to another one) can't be found this way.
std::vector<std::string> find_undescribed_bytes(const TypeInfo& type)
{
    struct Range { std::size_t begin; std::size_t end; std::size_t alignment; std::string name; };
    std::vector<Range> ranges;
    for (const auto& f : type.fields)
    {
        ranges.push_back({ f.offset, f.offset + f.byte_size(), f.alignment, f.name });
    }
    std::ranges::sort(ranges, {}, &Range::begin);

    std::vector<std::string> problems;
    std::size_t covered = 0;
    for (const auto& r : ranges)
    {
        if (r.begin < covered)
        {
            problems.push_back(type.name + "." + r.name + " overlaps the field before it");
        }
        else if (r.begin - covered >= r.alignment)
        {
            problems.push_back(type.name + ": bytes before " + r.name + " are not described");
        }
        covered = std::max(covered, r.end);
    }
    if (type.size - covered >= type.alignment)
    {
        problems.push_back(type.name + ": bytes at the end are not described");
    }
    return problems;
}

struct Point
{
    float x;
    float y;
};

struct Shape
{
    std::int32_t kind;
    union
    {
        Point point;
        float radius;
    } data;
};

TEST_F(TypeRegistryTest, BuiltinTypesRegisterWithoutErrors)
{
    engine::register_builtin_types(types_);
    EXPECT_TRUE(test_logger::messages().empty());

    std::vector<std::string> components;
    for (const auto* t : types_.get_components())
    {
        components.push_back(t->name);
    }
    // the load order: transforms and the hierarchy before what reads world matrices, the rigid body before the collider
    const std::vector<std::string> expected = { "name", "folder", "transform", "parent", "mesh", "material", "light",
        "nav_surface", "nav_modifier", "rigid_body", "collider", "character_controller" };
    EXPECT_EQ(components, expected);
}

TEST_F(TypeRegistryTest, BuiltinTypesDescribeEveryField)
{
    engine::register_builtin_types(types_);
    for (const auto* type : types_.get_types())
    {
        EXPECT_EQ(find_undescribed_bytes(*type), std::vector<std::string>{}) << type->name;
    }
}

TEST_F(TypeRegistryTest, CoverageCheckFindsAMissingField)
{
    TypeBuilder<Point>(types_, "point").field("x", &Point::x).structure();
    ASSERT_NE(types_.find("point"), nullptr);
    EXPECT_FALSE(find_undescribed_bytes(*types_.find("point")).empty());
}

TEST_F(TypeRegistryTest, FieldKindsAreDeducedFromTheMembers)
{
    engine::register_builtin_types(types_);
    const auto* transform = types_.find("transform");
    ASSERT_NE(transform, nullptr);
    const auto* position = transform->find_field("position");
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(position->kind, FieldKind::eFloat);
    EXPECT_TRUE(position->is_array);
    EXPECT_EQ(position->count, 3u);

    const auto* name = types_.find("name")->find_field("name");
    EXPECT_EQ(name->kind, FieldKind::eString);
    EXPECT_FALSE(name->is_array);
    EXPECT_EQ(name->size, 128u);

    // a one-element array of structs is still an array
    const auto* children = types_.find("compound_collider")->find_field("children");
    EXPECT_EQ(children->kind, FieldKind::eStruct);
    EXPECT_TRUE(children->is_array);
    EXPECT_EQ(children->count, 1u);
}

TEST_F(TypeRegistryTest, UnionCasesAreNamedByTheirTagValue)
{
    engine::register_builtin_types(types_);
    const auto* collider = types_.find("collider")->find_field("collider");
    ASSERT_NE(collider, nullptr);
    EXPECT_EQ(collider->kind, FieldKind::eUnion);
    std::vector<std::string> names;
    for (const auto& c : collider->union_cases)
    {
        names.push_back(c.name);
    }
    EXPECT_EQ(names, (std::vector<std::string>{ "box", "sphere", "compound" }));
}

TEST_F(TypeRegistryTest, ATypeNameIsRegisteredOnce)
{
    EXPECT_NE(TypeBuilder<Point>(types_, "point").field("x", &Point::x).field("y", &Point::y).structure(), nullptr);
    EXPECT_EQ(TypeBuilder<Point>(types_, "point").field("x", &Point::x).field("y", &Point::y).structure(), nullptr);
    EXPECT_TRUE(test_logger::logged("registered already"));
}

TEST_F(TypeRegistryTest, AFieldIsDescribedOnce)
{
    EXPECT_EQ(TypeBuilder<Point>(types_, "point").field("x", &Point::x).field("x", &Point::y).structure(), nullptr);
    EXPECT_EQ(types_.find("point"), nullptr);
    EXPECT_TRUE(test_logger::logged("described twice"));
}

TEST_F(TypeRegistryTest, AUnionNeedsAnEnumTagBeforeIt)
{
    TypeBuilder<Point>(types_, "point").field("x", &Point::x).field("y", &Point::y).structure();
    // kind is an int, not an enum
    const auto* shape = TypeBuilder<Shape>(types_, "shape")
        .field("kind", &Shape::kind)
        .tagged_union("data", &Shape::data, "kind", { { 0, "point" } })
        .component();
    EXPECT_EQ(shape, nullptr);
    EXPECT_TRUE(test_logger::logged("is not an enum field"));
}

TEST_F(TypeRegistryTest, AStructFieldNeedsItsTypeRegistered)
{
    struct Line { Point a; Point b; };
    EXPECT_EQ(TypeBuilder<Line>(types_, "line").structure("a", &Line::a, "point").structure("b", &Line::b, "point").structure(), nullptr);
    EXPECT_TRUE(test_logger::logged("is not a registered type"));
}

TEST_F(TypeRegistryTest, AUnionCaseNamedLikeAFieldIsRejected)
{
    enum Kind { KIND_POINT, KIND_RADIUS };
    struct Tagged
    {
        Kind kind;
        Point point;
        union { Point point; float radius; } data;
    };
    TypeBuilder<Point>(types_, "point").field("x", &Point::x).field("y", &Point::y).structure();
    // the case for "point" is written under the key of the field point
    const auto* tagged = TypeBuilder<Tagged>(types_, "tagged")
        .enumeration("kind", &Tagged::kind, { { "point", KIND_POINT }, { "radius", KIND_RADIUS } })
        .structure("point", &Tagged::point, "point")
        .tagged_union("data", &Tagged::data, "kind", { { KIND_POINT, "point" } })
        .component();
    EXPECT_EQ(tagged, nullptr);
    EXPECT_TRUE(test_logger::logged("written under the key of another field"));
}

// -- unions whose tag changed ---------------------------------------------------------------------------------------

TEST_F(TypeRegistryTest, ClearedUnionHoldsTheNewCaseZeroed)
{
    engine::register_builtin_types(types_);
    const auto& collider = *types_.find("collider");
    const auto* union_field = collider.find_field("collider");
    engine_collider_component_t value{};
    value.type = ENGINE_COLLIDER_TYPE_BOX;
    value.collider.box.size[0] = 3.0f;
    value.bounciness = 0.5f;
    // the tag changed: the box's bytes are no sphere
    value.type = ENGINE_COLLIDER_TYPE_SPHERE;
    engine::clear_union(*union_field, reinterpret_cast<std::byte*>(&value));
    EXPECT_EQ(value.collider.sphere.radius, 0.0f);
    EXPECT_EQ(value.collider.box.size[0], 0.0f);
    // the rest of the struct is kept
    EXPECT_EQ(value.bounciness, 0.5f);
    EXPECT_EQ(engine::find_active_case(collider, *union_field, reinterpret_cast<const std::byte*>(&value))->name, "sphere");
}

// -- comparing values --------------------------------------------------------------------------------------------------

template<typename T>
bool equal(const engine::TypeInfo& type, const T& a, const T& b)
{
    return engine::equal_values(type, reinterpret_cast<const std::byte*>(&a), reinterpret_cast<const std::byte*>(&b));
}

TEST_F(TypeRegistryTest, ValuesCompareFieldByField)
{
    engine::register_builtin_types(types_);
    const auto& transform = *types_.find("transform");
    engine_transform_component_t a{ { 1.0f, 2.0f, 3.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
    auto b = a;
    EXPECT_TRUE(equal(transform, a, b));
    b.scale[2] = 2.0f;
    EXPECT_FALSE(equal(transform, a, b));
}

TEST_F(TypeRegistryTest, BytesAfterAStringsTerminatorDontCount)
{
    engine::register_builtin_types(types_);
    const auto& name = *types_.find("name");
    engine_name_component_t a{};
    engine_name_component_t b{};
    std::strcpy(a.name, "crate");
    std::memset(b.name, 'x', sizeof(b.name));
    std::strcpy(b.name, "crate");
    EXPECT_TRUE(equal(name, a, b));
    std::strcpy(b.name, "crates");
    EXPECT_FALSE(equal(name, a, b));
}

TEST_F(TypeRegistryTest, OnlyTheActiveUnionCaseIsCompared)
{
    engine::register_builtin_types(types_);
    const auto& collider = *types_.find("collider");
    engine_collider_component_t a{};
    a.type = ENGINE_COLLIDER_TYPE_SPHERE;
    a.collider.sphere.radius = 0.5f;
    auto b = a;
    // bytes of the union the sphere doesn't use
    b.collider.box.size[2] = 7.0f;
    EXPECT_TRUE(equal(collider, a, b));
    b.collider.sphere.radius = 0.25f;
    EXPECT_FALSE(equal(collider, a, b));
    b = a;
    b.type = ENGINE_COLLIDER_TYPE_BOX;
    EXPECT_FALSE(equal(collider, a, b));
}
}  // namespace
