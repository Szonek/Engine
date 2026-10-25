#pragma once
#include <engine.h>

#include <entt/entt.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace engine
{
// What a field holds, and so how it is written to a file.
enum class FieldKind
{
    eBool,
    eInt32,
    eUint32,
    eFloat,
    // a fixed size char array, null terminated
    eString,
    // a C enum (int sized), written by the name of its value
    eEnum,
    // a registered struct type, see FieldInfo::struct_type
    eStruct,
    // a union whose active case is selected by an enum field of the same struct, see FieldInfo::union_cases
    eUnion,
    // an asset handle, written as the asset ID (see IAssetResolver)
    eAsset,
    // an engine_game_object_t, written as the persistent ID of the game object
    eGameObject,
};

// The atlases an asset reference can point into.
enum class AssetKind
{
    eGeometry,
    eTexture,
};

struct EnumValue
{
    std::string name;
    std::int32_t value;
};

struct TypeInfo;

// One case of a tagged union: its data when the tag field has the value `tag`, a registered struct type at the
// union's offset. It is written next to the tag, under the name of the tag's value.
struct UnionCase
{
    std::int32_t tag;
    std::string name;
    const TypeInfo* type;
};

// How the editor shows a field (game-defined types, engine_field_flags_t); they never change what is saved.
struct FieldHints
{
    bool read_only = false;
    bool hidden = false;
    // a float[3] / float[4] colour
    bool color = false;
    // numbers: the editor keeps the value in [min, max]
    std::optional<std::pair<float, float>> range;
    std::string tooltip;
};

struct FieldInfo
{
    std::string name;
    FieldKind kind;
    std::size_t offset;
    // of one element; for eString its capacity (the char array, terminator included)
    std::size_t size;
    std::size_t alignment;
    // a fixed size array (float[3], an array of structs) of `count` elements is written as a JSON array
    bool is_array = false;
    std::size_t count = 1;

    std::vector<EnumValue> enum_values;                // eEnum
    const TypeInfo* struct_type = nullptr;             // eStruct
    std::string union_tag;                             // eUnion: the enum field (before it) selecting the case
    std::vector<UnionCase> union_cases;                // eUnion: a tag value without a case has no data
    AssetKind asset_kind = AssetKind::eGeometry;       // eAsset
    FieldHints hints;

    // bytes the field covers in its struct
    std::size_t byte_size() const { return size * count; }
};

// How to reach the components of a type in a registry without knowing the type; values are passed as a pointer to a
// T. Like the C API: add() adds the component with its defaults (the registry's construction listeners initialize it,
// engineGameObjectAdd*Component), update() replaces it so that the update listeners run (engineGameObjectUpdate*Component).
struct ComponentStorage
{
    std::function<bool(const entt::registry&, entt::entity)> has;
    std::function<void(entt::registry&, entt::entity)> add;
    std::function<void(const entt::registry&, entt::entity, void*)> get;
    std::function<void(entt::registry&, entt::entity, const void*)> update;
    std::function<void(entt::registry&, entt::entity)> remove;

    // Game-defined types only (empty for the engine's): the type's storage in the registry, created when needed (to
    // iterate the game objects which have the component), and how many times a component of the type was added,
    // updated or removed in the registry, ever (to see that something changed without iterating them).
    std::function<entt::sparse_set&(entt::registry&)> pool;
    std::function<std::uint64_t(const entt::registry&)> change_count;
};

// A type's handle (the engine_component_type_t of the C API) is its index in the registry plus this, so no type has
// ENGINE_INVALID_OBJECT_HANDLE (0).
inline constexpr std::uint32_t FIRST_TYPE_HANDLE = ENGINE_INVALID_OBJECT_HANDLE + 1;
static_assert(FIRST_TYPE_HANDLE != ENGINE_INVALID_OBJECT_HANDLE);

// The version a type starts at; it goes up when the type's layout changes, and a scene file stores the version each
// component was saved with.
inline constexpr std::uint32_t FIRST_TYPE_VERSION = 1;

// Describes a C struct field by field, so that values of it can be saved and loaded (and later edited) without code
// for the type. A component is a type with storage; the other types are only used inside components.
struct TypeInfo
{
    // set by the registry (FIRST_TYPE_HANDLE and up); ENGINE_INVALID_OBJECT_HANDLE until the type is registered
    std::uint32_t handle = ENGINE_INVALID_OBJECT_HANDLE;
    std::string name;
    // of the components: a scene file stores the version each component was saved with
    std::uint32_t version = FIRST_TYPE_VERSION;
    std::size_t size = 0;
    std::size_t alignment = 0;
    std::vector<FieldInfo> fields;
    std::unique_ptr<ComponentStorage> storage;
    // registered by the game through the C API (see game_types.h), not by the engine; the editor has no panel for it
    bool game_defined = false;

    bool is_component() const { return storage != nullptr; }
    const FieldInfo* find_field(std::string_view field_name) const
    {
        const auto it = std::ranges::find(fields, field_name, &FieldInfo::name);
        return it == fields.end() ? nullptr : &*it;
    }
};

// A buffer for one value of a registered type, aligned for any of them (C structs, aligned to at most
// ENGINE_COMPONENT_TYPE_MAX_ALIGNMENT, the engine's like the game's).
class ValueBuffer
{
public:
    explicit ValueBuffer(std::size_t size) : data_((size + sizeof(Chunk) - 1) / sizeof(Chunk)) {}
    std::byte* get() { return data_.empty() ? nullptr : data_.front().bytes; }
    const std::byte* get() const { return data_.empty() ? nullptr : data_.front().bytes; }

private:
    struct alignas(ENGINE_COMPONENT_TYPE_MAX_ALIGNMENT) Chunk
    {
        std::byte bytes[ENGINE_COMPONENT_TYPE_MAX_ALIGNMENT];
    };
    std::vector<Chunk> data_;
};

// The types the engine can save, by name. Components keep the order they were registered in: scenes are saved and
// loaded in that order, so a component is registered after the ones its listeners read (transform before parent before
// colliders: a body is created at the world matrix of the transforms when its collider is added).
class TypeRegistry
{
public:
    TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    // nullptr (logged) when the name is empty or registered already
    const TypeInfo* add(TypeInfo&& type);
    const TypeInfo* find(std::string_view name) const;
    // nullptr when no type has the handle
    const TypeInfo* get(std::uint32_t handle) const;
    // in registration order
    const std::vector<const TypeInfo*>& get_components() const { return components_; }
    std::vector<const TypeInfo*> get_types() const;

private:
    std::vector<std::unique_ptr<TypeInfo>> types_;
    std::vector<const TypeInfo*> components_;
};

// The case of the union `union_field` (of `type`) its tag selects in `value`, or nullptr when the tag's value has none.
const UnionCase* find_active_case(const TypeInfo& type, const FieldInfo& union_field, const std::byte* value);

// Clears the union `union_field` of `value` when its tag selected another case (a file or the editor changed it): the
// bytes of the old case are no value of the new one, which starts zeroed (holding nothing: no asset or game object
// has the handle 0).
void clear_union(const FieldInfo& union_field, std::byte* value);

// Whether two values of `type` are the same field by field: padding and the bytes after a string's terminator don't
// count, and a union compares only its active case.
bool equal_values(const TypeInfo& type, const std::byte* a, const std::byte* b);
// Whether the field `field` of `type` (every element of an array) is the same in the values `a` and `b` of the type, as
// equal_values compares it; a union is the same when its tag is and its active case is.
bool equal_field(const TypeInfo& type, const FieldInfo& field, const std::byte* a, const std::byte* b);

namespace type_builder_detail
{
template<typename T> struct array_traits { using element = T; static constexpr bool is_array = false; static constexpr std::size_t count = 1; };
template<typename T, std::size_t N> struct array_traits<T[N]> { using element = T; static constexpr bool is_array = true; static constexpr std::size_t count = N; };

// logs a mistake in a type description
void report_error(std::string_view type_name, std::string_view message);

// Adds the case of `union_field` for the value `value` of its enum tag `tag`: the registered type `type_name`, at most
// `max_size` bytes. Returns what is wrong when it can't be one (unknown type, too big, no such value of the tag, a case
// for the value already), or an empty string.
std::string add_union_case(const TypeRegistry& registry, const FieldInfo& tag, std::int32_t value, std::string_view type_name, std::size_t max_size, FieldInfo& union_field);

// What makes two fields of the type written under the same key (a field and a union case named alike, or two unions
// with a case of the same name), or an empty string. A union is written under its active case's name.
std::string find_duplicate_key(const TypeInfo& type);
}  // namespace type_builder_detail

// Offset of a member in a C struct (standard layout), from a member pointer: what offsetof gives, also for the
// members of an anonymous union.
template<typename Owner, typename Member>
std::size_t member_offset(Member Owner::* member)
{
    alignas(Owner) static const std::byte storage[sizeof(Owner)]{};
    const auto* owner = reinterpret_cast<const Owner*>(storage);
    return static_cast<std::size_t>(reinterpret_cast<const std::byte*>(&(owner->*member)) - storage);
}

// Describes the C struct T with member pointers, so the compiler checks the field types:
//   TypeBuilder<engine_rigid_body_component_t>(types, "rigid_body")
//       .enumeration("type", &engine_rigid_body_component_t::type, { { "dynamic", ENGINE_RIGID_BODY_TYPE_DYNAMIC }, ... })
//       .field("mass", &engine_rigid_body_component_t::mass)
//       .component();
// Plain fields (bool, int32, uint32, float, arrays of them and char arrays) are deduced. Handles are uint32 typedefs,
// so asset and game object references are declared with asset() and game_object(). A description with a mistake is
// logged and not registered.
template<typename T>
class TypeBuilder
{
    static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>, "only C structs can be described");

public:
    TypeBuilder(TypeRegistry& registry, std::string_view name, std::uint32_t version = FIRST_TYPE_VERSION)
        : registry_(registry)
    {
        type_.name = std::string(name);
        type_.version = version;
        type_.size = sizeof(T);
        type_.alignment = alignof(T);
    }

    template<typename Member>
    TypeBuilder& field(std::string_view name, Member T::* member)
    {
        using traits = type_builder_detail::array_traits<Member>;
        using E = typename traits::element;
        auto f = make_field<Member>(name, member_offset(member));
        if constexpr (std::is_same_v<E, char>)
        {
            f.kind = FieldKind::eString;
            f.size = traits::count;
            f.is_array = false;
            f.count = 1;
        }
        else
        {
            static_assert(std::is_same_v<E, bool> || std::is_same_v<E, float> || std::is_same_v<E, std::int32_t> || std::is_same_v<E, std::uint32_t>,
                "use enumeration(), structure(), asset() or game_object() for this field");
            f.kind = std::is_same_v<E, bool> ? FieldKind::eBool
                : std::is_same_v<E, float> ? FieldKind::eFloat
                : std::is_same_v<E, std::int32_t> ? FieldKind::eInt32 : FieldKind::eUint32;
        }
        return push(std::move(f));
    }

    template<typename Member>
    TypeBuilder& enumeration(std::string_view name, Member T::* member, std::vector<EnumValue> values)
    {
        static_assert(std::is_enum_v<Member> && sizeof(Member) == sizeof(std::int32_t), "a C enum");
        auto f = make_field<Member>(name, member_offset(member));
        f.kind = FieldKind::eEnum;
        f.enum_values = std::move(values);
        return push(std::move(f));
    }

    // a struct, or an array of them, of a type registered before
    template<typename Member>
    TypeBuilder& structure(std::string_view name, Member T::* member, std::string_view type_name)
    {
        using E = typename type_builder_detail::array_traits<Member>::element;
        auto f = make_field<Member>(name, member_offset(member));
        f.kind = FieldKind::eStruct;
        f.struct_type = registry_.find(type_name);
        if (!f.struct_type || f.struct_type->size != sizeof(E))
        {
            error(fmt::format("field {}: {} is not a registered type of its size", name, type_name));
        }
        return push(std::move(f));
    }

    // The union `member`, whose case is selected by the enum field `tag` (described before it).
    template<typename Member>
    TypeBuilder& tagged_union(std::string_view name, Member T::* member, std::string_view tag, const std::vector<std::pair<std::int32_t, std::string_view>>& cases)
    {
        static_assert(std::is_union_v<Member>);
        return tagged_union(name, member_offset(member), sizeof(Member), alignof(Member), tag, cases);
    }

    // A union without a name (its members are members of T): the cases' offset is the offset of any of its members.
    TypeBuilder& tagged_union(std::string_view name, std::size_t offset, std::string_view tag, const std::vector<std::pair<std::int32_t, std::string_view>>& cases)
    {
        std::size_t size = 0;
        std::size_t alignment = 1;
        for (const auto& [value, type_name] : cases)
        {
            if (const auto* t = registry_.find(type_name))
            {
                size = std::max(size, t->size);
                alignment = std::max(alignment, t->alignment);
            }
        }
        return tagged_union(name, offset, size, alignment, tag, cases);
    }

    TypeBuilder& asset(std::string_view name, std::uint32_t T::* member, AssetKind kind)
    {
        auto f = make_field<std::uint32_t>(name, member_offset(member));
        f.kind = FieldKind::eAsset;
        f.asset_kind = kind;
        return push(std::move(f));
    }

    TypeBuilder& game_object(std::string_view name, std::uint32_t T::* member)
    {
        auto f = make_field<std::uint32_t>(name, member_offset(member));
        f.kind = FieldKind::eGameObject;
        return push(std::move(f));
    }

    // registers T as a type used inside other types
    const TypeInfo* structure()
    {
        return finish();
    }

    // registers T as a component, stored in the registry as T
    const TypeInfo* component()
    {
        auto storage = std::make_unique<ComponentStorage>();
        storage->has = [](const entt::registry& r, entt::entity e) { return r.all_of<T>(e); };
        storage->add = [](entt::registry& r, entt::entity e) { r.emplace<T>(e); };
        storage->get = [](const entt::registry& r, entt::entity e, void* out) { *static_cast<T*>(out) = r.get<T>(e); };
        storage->update = [](entt::registry& r, entt::entity e, const void* value) { r.replace<T>(e, *static_cast<const T*>(value)); };
        storage->remove = [](entt::registry& r, entt::entity e) { r.remove<T>(e); };
        type_.storage = std::move(storage);
        return finish();
    }

private:
    template<typename Member>
    FieldInfo make_field(std::string_view name, std::size_t offset)
    {
        using traits = type_builder_detail::array_traits<Member>;
        FieldInfo f{};
        f.name = std::string(name);
        f.offset = offset;
        f.size = sizeof(typename traits::element);
        f.alignment = alignof(typename traits::element);
        f.is_array = traits::is_array;
        f.count = traits::count;
        return f;
    }

    TypeBuilder& tagged_union(std::string_view name, std::size_t offset, std::size_t size, std::size_t alignment, std::string_view tag, const std::vector<std::pair<std::int32_t, std::string_view>>& cases)
    {
        FieldInfo f{};
        f.name = std::string(name);
        f.kind = FieldKind::eUnion;
        f.offset = offset;
        f.size = size;
        f.alignment = alignment;
        f.union_tag = std::string(tag);
        const auto* tag_field = type_.find_field(tag);
        if (!tag_field || tag_field->kind != FieldKind::eEnum)
        {
            error(fmt::format("union {}: its tag {} is not an enum field described before it", name, tag));
            return push(std::move(f));
        }
        for (const auto& [value, type_name] : cases)
        {
            if (const auto problem = type_builder_detail::add_union_case(registry_, *tag_field, value, type_name, size, f); !problem.empty())
            {
                error(fmt::format("union {}: {}", name, problem));
            }
        }
        return push(std::move(f));
    }

    TypeBuilder& push(FieldInfo&& f)
    {
        if (type_.find_field(f.name))
        {
            error(fmt::format("field {} is described twice", f.name));
        }
        type_.fields.push_back(std::move(f));
        return *this;
    }

    void error(std::string_view message)
    {
        failed_ = true;
        type_builder_detail::report_error(type_.name, message);
    }

    const TypeInfo* finish()
    {
        if (const auto problem = type_builder_detail::find_duplicate_key(type_); !problem.empty())
        {
            error(problem);
        }
        return failed_ ? nullptr : registry_.add(std::move(type_));
    }

    TypeRegistry& registry_;
    TypeInfo type_;
    bool failed_ = false;
};
}  // namespace engine
