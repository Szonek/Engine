#include "scene_serializer.h"
#include "../logger.h"

#include <engine.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
using Writer = rapidjson::PrettyWriter<rapidjson::StringBuffer>;
using Json = rapidjson::Value;

template<typename T>
T read_as(const std::byte* p)
{
    T value;
    std::memcpy(&value, p, sizeof(T));
    return value;
}

template<typename T>
void write_as(std::byte* p, T value)
{
    std::memcpy(p, &value, sizeof(T));
}

void log_error(std::string_view message)
{
    engine::log::log(engine::log::LogLevel::eError, message);
}

// ---------------------------------------------------------------------------------------------------------------------
// saving

class SceneWriter
{
public:
    SceneWriter(const engine::EntityIdRegistry& ids, const engine::IAssetResolver& assets)
        : ids_(ids), assets_(assets)
    {
        writer_.SetIndent(' ', 2);
        // vectors and colours stay on one line
        writer_.SetFormatOptions(rapidjson::kFormatSingleLineArray);
    }

    Writer& writer() { return writer_; }
    std::string result() const { return std::string(buffer_.GetString(), buffer_.GetSize()); }

    // Every field of `value`, or with a `base` (a value of the same type: what a prefab gave, for an override) only the
    // fields which differ from it: a field is written whole (an array is one field), a nested struct with the fields
    // which differ in it. A union writes its active case under the name of its tag's value; it is one value, so when
    // anything in it differs from the base (its tag or its case), its tag and its whole case are written, and the
    // override applies whatever case the base has when it is loaded.
    void write_struct(const engine::TypeInfo& type, const std::byte* value, std::string_view path, const std::byte* base = nullptr)
    {
        const auto differs = [&](const engine::FieldInfo& field) { return !base || !engine::equal_field(type, field, value, base); };
        // the tag of a union which differs is written with it
        const auto tag_of_differing_union = [&](const engine::FieldInfo& field) {
            return base && std::ranges::any_of(type.fields, [&](const engine::FieldInfo& u) {
                return u.kind == engine::FieldKind::eUnion && u.union_tag == field.name && differs(u);
            });
        };
        writer_.StartObject();
        for (const auto& field : type.fields)
        {
            if (!differs(field) && !tag_of_differing_union(field))
            {
                continue;
            }
            if (field.kind == engine::FieldKind::eUnion)
            {
                // only the active case has data; a tag value without a case has none
                if (const auto* active = engine::find_active_case(type, field, value))
                {
                    key(active->name);
                    write_struct(*active->type, value + field.offset, fmt::format("{}.{}", path, active->name));
                }
                continue;
            }
            key(field.name);
            const auto field_path = fmt::format("{}.{}", path, field.name);
            if (base && field.kind == engine::FieldKind::eStruct && !field.is_array)
            {
                write_struct(*field.struct_type, value + field.offset, field_path, base + field.offset);
            }
            else
            {
                write_field(field, value, field_path);
            }
        }
        writer_.EndObject();
    }

private:
    void write_field(const engine::FieldInfo& field, const std::byte* value, const std::string& path)
    {
        if (!field.is_array)
        {
            write_element(field, value + field.offset, path);
            return;
        }
        writer_.StartArray();
        for (std::size_t i = 0; i < field.count; i++)
        {
            write_element(field, value + field.offset + i * field.size, path);
        }
        writer_.EndArray();
    }

    void key(std::string_view name)
    {
        writer_.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
    }

    void string(std::string_view s)
    {
        writer_.String(s.data(), static_cast<rapidjson::SizeType>(s.size()));
    }

    void write_element(const engine::FieldInfo& field, const std::byte* p, std::string_view path)
    {
        using engine::FieldKind;
        switch (field.kind)
        {
        case FieldKind::eBool: writer_.Bool(read_as<bool>(p)); break;
        case FieldKind::eInt32: writer_.Int(read_as<std::int32_t>(p)); break;
        case FieldKind::eUint32: writer_.Uint(read_as<std::uint32_t>(p)); break;
        case FieldKind::eFloat:
        {
            const auto f = read_as<float>(p);
            if (!std::isfinite(f))
            {
                log_error(fmt::format("Scene save: {} is not a finite number, saved as null.\n", path));
                writer_.Null();
                break;
            }
            // the shortest text which reads back as the same float ("0.1", not the double "0.10000000149011612")
            const auto text = fmt::format("{}", f);
            writer_.RawValue(text.data(), text.size(), rapidjson::kNumberType);
            break;
        }
        case FieldKind::eString:
        {
            const auto* chars = reinterpret_cast<const char*>(p);
            string(std::string_view(chars, strnlen(chars, field.size)));
            break;
        }
        case FieldKind::eEngineString:
        {
            const auto* str = read_as<const engine_string_t*>(p);
            string(str ? std::string_view(str->str) : std::string_view());
            break;
        }
        case FieldKind::eEnum:
        {
            const auto value = read_as<std::int32_t>(p);
            const auto it = std::ranges::find(field.enum_values, value, &engine::EnumValue::value);
            if (it == field.enum_values.end())
            {
                log_error(fmt::format("Scene save: {} has the value {} which isn't one of its enum's, saved as null.\n", path, value));
                writer_.Null();
                break;
            }
            string(it->name);
            break;
        }
        case FieldKind::eStruct: write_struct(*field.struct_type, p, path); break;
        case FieldKind::eUnion: break;  // written by write_struct
        case FieldKind::eAsset:
        {
            const auto handle = read_as<std::uint32_t>(p);
            if (handle == ENGINE_INVALID_OBJECT_HANDLE)
            {
                writer_.Null();
                break;
            }
            const auto id = assets_.get_asset_id(field.asset_kind, handle);
            if (id.empty())
            {
                log_error(fmt::format("Scene save: {} uses an asset which doesn't exist any more, saved as null.\n", path));
                writer_.Null();
                break;
            }
            string(id);
            break;
        }
        case FieldKind::eGameObject:
        {
            const auto go = read_as<std::uint32_t>(p);
            if (go == ENGINE_INVALID_GAME_OBJECT_ID)
            {
                writer_.Null();
                break;
            }
            const auto id = ids_.get_id(static_cast<entt::entity>(go));
            if (id == engine::EntityIdRegistry::INVALID_ID)
            {
                log_error(fmt::format("Scene save: {} references a game object which isn't saved (transient or destroyed), saved as null.\n", path));
                writer_.Null();
                break;
            }
            writer_.Uint64(id);
            break;
        }
        }
    }

    const engine::EntityIdRegistry& ids_;
    const engine::IAssetResolver& assets_;
    rapidjson::StringBuffer buffer_;
    Writer writer_{ buffer_ };
};

// ---------------------------------------------------------------------------------------------------------------------
// loading
// Numbers are parsed as text (kParseNumbersAsStringsFlag) and converted with from_chars, which is exact: a float reads
// back as the float it was saved from.

template<typename T>
bool parse_number(const Json& json, T& out)
{
    if (!json.IsString())
    {
        return false;
    }
    const auto* begin = json.GetString();
    const auto* end = begin + json.GetStringLength();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc() && ptr == end;
}

class SceneReader
{
public:
    // `outside`: where references to game objects which aren't in the file are looked up, nullptr when they are none
    SceneReader(std::string_view file_name, const std::unordered_map<std::uint64_t, entt::entity>& entities, const engine::EntityIdRegistry* outside,
        engine::IAssetResolver& assets)
        : file_name_(file_name), entities_(entities), outside_(outside), assets_(assets)
    {
    }

    // reads the fields the json object has into `value`, which holds the defaults; the engine strings read point at
    // strings `value` owns
    void read_struct(const engine::TypeInfo& type, const Json& json, engine::ValueBuffer& value, std::string_view path)
    {
        strings_ = &value;
        read_struct(type, json, value.get(), path);
        strings_ = nullptr;
    }

    void set_entity(std::uint64_t id) { entity_id_ = id; }
    // the value read is a data asset's, which isn't in a scene: messages name the file only
    void set_data_asset() { data_asset_ = true; }

private:
    void read_struct(const engine::TypeInfo& type, const Json& json, std::byte* value, std::string_view path)
    {
        if (!json.IsObject())
        {
            error(path, "is not an object, the defaults are kept");
            return;
        }
        // the default case of each union, before the tags are read
        std::vector<const engine::UnionCase*> default_cases;
        for (const auto& field : type.fields)
        {
            if (field.kind == engine::FieldKind::eUnion)
            {
                default_cases.push_back(engine::find_active_case(type, field, value));
            }
        }

        std::set<std::string_view> used_keys;
        for (const auto& field : type.fields)
        {
            if (field.kind == engine::FieldKind::eUnion)
            {
                continue;
            }
            const auto it = json.FindMember(field.name.c_str());
            if (it == json.MemberEnd())
            {
                continue;
            }
            used_keys.insert(field.name);
            read_field(field, it->value, value + field.offset, fmt::format("{}.{}", path, field.name));
        }
        // the unions once their tags are read
        std::size_t union_index = 0;
        for (const auto& field : type.fields)
        {
            if (field.kind != engine::FieldKind::eUnion)
            {
                continue;
            }
            const auto* active = engine::find_active_case(type, field, value);
            if (active != default_cases[union_index++])
            {
                // another case than the default one: the defaults in the union are another case's, so it starts zeroed
                engine::clear_union(field, value);
            }
            if (!active)
            {
                continue;
            }
            const auto it = json.FindMember(active->name.c_str());
            if (it == json.MemberEnd())
            {
                continue;
            }
            used_keys.insert(active->name);
            read_struct(*active->type, it->value, value + field.offset, fmt::format("{}.{}", path, active->name));
        }
        for (auto it = json.MemberBegin(); it != json.MemberEnd(); ++it)
        {
            const std::string_view key(it->name.GetString(), it->name.GetStringLength());
            if (!used_keys.contains(key))
            {
                error(path, fmt::format("has an unknown field \"{}\" (or the data of an inactive union case), ignored", key));
            }
        }
    }

    void error(std::string_view path, std::string_view message)
    {
        if (data_asset_)
        {
            log_error(fmt::format("Data asset {}: {} {}.\n", file_name_, path, message));
            return;
        }
        log_error(fmt::format("Scene {}, game object {}: {} {}.\n", file_name_, entity_id_, path, message));
    }

    void read_field(const engine::FieldInfo& field, const Json& json, std::byte* p, const std::string& path)
    {
        if (!field.is_array)
        {
            read_element(field, json, p, path);
            return;
        }
        if (!json.IsArray())
        {
            error(path, "is not an array, the default is kept");
            return;
        }
        if (json.Size() != field.count)
        {
            error(path, fmt::format("has {} elements instead of {}, the ones which fit are read", json.Size(), field.count));
        }
        const auto count = std::min<std::size_t>(json.Size(), field.count);
        for (std::size_t i = 0; i < count; i++)
        {
            read_element(field, json[static_cast<rapidjson::SizeType>(i)], p + i * field.size, fmt::format("{}[{}]", path, i));
        }
    }

    void read_element(const engine::FieldInfo& field, const Json& json, std::byte* p, const std::string& path)
    {
        using engine::FieldKind;
        switch (field.kind)
        {
        case FieldKind::eBool:
            if (!json.IsBool())
            {
                return error(path, "is not true or false, the default is kept");
            }
            write_as(p, json.GetBool());
            return;
        case FieldKind::eInt32: return read_number<std::int32_t>(json, p, path);
        case FieldKind::eUint32: return read_number<std::uint32_t>(json, p, path);
        case FieldKind::eFloat: return read_number<float>(json, p, path);
        case FieldKind::eString:
        {
            if (!json.IsString())
            {
                return error(path, "is not a string, the default is kept");
            }
            const auto length = std::min<std::size_t>(json.GetStringLength(), field.size - 1);
            if (length < json.GetStringLength())
            {
                error(path, fmt::format("is longer than {} characters, it is cut", field.size - 1));
            }
            std::memset(p, 0, field.size);
            std::memcpy(p, json.GetString(), length);
            return;
        }
        case FieldKind::eEngineString:
        {
            if (!json.IsString())
            {
                return error(path, "is not a string, the default is kept");
            }
            write_as(p, strings_->make_string(std::string_view(json.GetString(), json.GetStringLength())));
            return;
        }
        case FieldKind::eEnum:
        {
            if (!json.IsString())
            {
                return error(path, "is not the name of a value, the default is kept");
            }
            const std::string_view name(json.GetString(), json.GetStringLength());
            const auto it = std::ranges::find(field.enum_values, name, &engine::EnumValue::name);
            if (it == field.enum_values.end())
            {
                return error(path, fmt::format("has an unknown value \"{}\", the default is kept", name));
            }
            write_as(p, it->value);
            return;
        }
        case FieldKind::eStruct:
            read_struct(*field.struct_type, json, p, path);
            return;
        case FieldKind::eUnion:
            return;  // read by read_struct
        case FieldKind::eAsset:
        {
            if (json.IsNull())
            {
                write_as(p, static_cast<std::uint32_t>(ENGINE_INVALID_OBJECT_HANDLE));
                return;
            }
            if (!json.IsString())
            {
                return error(path, "is not an asset ID, the default is kept");
            }
            auto handle = assets_.resolve(field.asset_kind, std::string_view(json.GetString(), json.GetStringLength()));
            if (handle == ENGINE_INVALID_OBJECT_HANDLE)
            {
                error(path, fmt::format("uses the asset \"{}\" which can't be found, it has none", json.GetString()));
            }
            else if (const auto* held = field.asset_kind == engine::AssetKind::eDataAsset ? assets_.get_data_asset_type(handle) : nullptr; held != field.data_type)
            {
                error(path, fmt::format("uses the data asset \"{}\" which holds a {}, not a {}, it has none", json.GetString(), held ? held->name : "nothing",
                    field.data_type ? field.data_type->name : "nothing"));
                handle = ENGINE_INVALID_OBJECT_HANDLE;
            }
            write_as(p, handle);
            return;
        }
        case FieldKind::eGameObject:
        {
            if (json.IsNull())
            {
                write_as(p, static_cast<std::uint32_t>(ENGINE_INVALID_GAME_OBJECT_ID));
                return;
            }
            std::uint64_t id = 0;
            if (!parse_number(json, id))
            {
                return error(path, "is not a persistent ID, the default is kept");
            }
            const auto it = entities_.find(id);
            if (it != entities_.end())
            {
                write_as(p, static_cast<std::uint32_t>(it->second));
                return;
            }
            const auto outside = outside_ ? outside_->find(id) : entt::entity{ entt::null };
            if (outside != entt::null)
            {
                write_as(p, static_cast<std::uint32_t>(outside));
                return;
            }
            error(path, fmt::format("references the game object {} which isn't in the file (or no node of a prefab instance in it), it references none", id));
            write_as(p, static_cast<std::uint32_t>(ENGINE_INVALID_GAME_OBJECT_ID));
            return;
        }
        }
    }

    template<typename T>
    void read_number(const Json& json, std::byte* p, const std::string& path)
    {
        T value{};
        if (!parse_number(json, value))
        {
            return error(path, "is not a number of its type, the default is kept");
        }
        write_as(p, value);
    }

    std::string_view file_name_;
    const std::unordered_map<std::uint64_t, entt::entity>& entities_;
    const engine::EntityIdRegistry* outside_;
    engine::IAssetResolver& assets_;
    std::uint64_t entity_id_ = 0;
    bool data_asset_ = false;
    // the value being read, which owns the engine strings read into it
    engine::ValueBuffer* strings_ = nullptr;
};

struct FileEntity
{
    std::uint64_t id;
    const Json* components;
    // an instance root: its prefab's asset ID, and node path ("" for the root) -> what differs from the prefab on that
    // game object
    std::string_view prefab;
    const Json* nodes = nullptr;
};

// what the file has for one game object, and the game object it goes to: the components to add whole, the overrides of
// the ones its prefab gave it, the names of the ones its prefab gave it to remove (each nullptr when there are none)
struct FileComponents
{
    std::uint64_t id;
    std::string_view node_path;
    const Json* components;
    entt::entity entity;
    const Json* overrides = nullptr;
    const Json* removed = nullptr;
    std::string_view prefab;
};

// the member `key` of a node of an instance when it is an object (an array for "removed"), else nullptr
const Json* node_member(const Json& node, const char* key)
{
    const auto it = node.FindMember(key);
    if (it == node.MemberEnd())
    {
        return nullptr;
    }
    const auto is_removed = std::string_view(key) == "removed";
    return (is_removed ? it->value.IsArray() : it->value.IsObject()) ? &it->value : nullptr;
}

void check_components(const Json& components, std::string_view file_name, const engine::TypeRegistry& types, std::set<std::string_view>& unknown_components)
{
    for (const auto& c : components.GetObject())
    {
        const std::string_view name(c.name.GetString(), c.name.GetStringLength());
        const auto* type = types.find(name);
        if ((!type || !type->is_component()) && unknown_components.insert(name).second)
        {
            log_error(fmt::format("Scene {}: unknown component {}, it is ignored.\n", file_name, name));
        }
    }
}
}  // namespace

void engine::capture_prefab_components(const TypeRegistry& types, const entt::registry& registry, PrefabInstanceInfo& instance)
{
    for (auto& node : instance.nodes)
    {
        node.created.clear();
        for (const auto* type : types.get_components())
        {
            if (registry.valid(node.entity) && type->storage->has(registry, node.entity))
            {
                PrefabComponentValue created{ type, ValueBuffer(type->size) };
                type->storage->get(registry, node.entity, created.value.get());
                // the strings it had, not the component's, which change with it
                own_engine_strings(*type, created.value);
                node.created.push_back(std::move(created));
            }
        }
    }
}

namespace
{
// A component as it is written, its value read once: whole, or as the fields which differ from the value its prefab
// gave it (`created`).
struct ComponentToWrite
{
    const engine::TypeInfo* type;
    engine::ValueBuffer value;
    const engine::PrefabComponentValue* created = nullptr;
};

ComponentToWrite read_component(const engine::TypeInfo* type, const entt::registry& registry, entt::entity entity)
{
    ComponentToWrite component{ type, engine::ValueBuffer(type->size) };
    type->storage->get(registry, entity, component.value.get());
    return component;
}

// What is written of a game object of a prefab instance (the root too, node path ""): the components written whole,
// the overrides of the ones its prefab gave it, and the names of the ones its prefab gave it which were removed.
struct NodeToWrite
{
    const engine::PrefabInstanceNode* node;
    std::vector<ComponentToWrite> components;
    std::vector<ComponentToWrite> overrides;
    std::vector<const engine::TypeInfo*> removed;

    bool empty() const { return components.empty() && overrides.empty() && removed.empty(); }
};

// A game object as it is written: its components, or for the root of a prefab instance what differs from its prefab
// on each game object of the instance.
struct EntityToWrite
{
    std::uint64_t id;
    entt::entity entity;
    std::vector<ComponentToWrite> components;
    const engine::PrefabInstanceInfo* instance = nullptr;
    std::vector<NodeToWrite> nodes;
};

const engine::PrefabComponentValue* find_created(const engine::PrefabInstanceNode& node, const engine::TypeInfo* type)
{
    const auto it = std::ranges::find(node.created, type, &engine::PrefabComponentValue::type);
    return it == node.created.end() ? nullptr : &*it;
}

// The root's components which are always saved whole, like Unity saves an instance root's name and transform: what the
// level does with every instance (place it, name it), kept even when its prefab can't be found.
bool is_root_placement(const engine::TypeInfo& type)
{
    return type.name == "name" || type.name == "transform";
}

// What is saved of a game object of an instance: the components its prefab didn't give it (and the root's name and
// transform) whole, the ones it gave it which changed since as overrides (only what changed), the ones it gave it which
// were removed by name. Adds to `changes` what scene files can't save yet: a game object which was destroyed.
NodeToWrite instance_node_components(const std::vector<const engine::TypeInfo*>& components, const entt::registry& registry,
    const engine::PrefabInstanceNode& node, bool is_root, std::vector<std::string>& changes)
{
    NodeToWrite written{ &node, {}, {}, {} };
    if (!registry.valid(node.entity))
    {
        changes.push_back(fmt::format("node {} was destroyed", node.path));
        return written;
    }
    for (const auto* type : components)
    {
        const auto has = type->storage->has(registry, node.entity);
        const auto* created = find_created(node, type);
        if (!created || (is_root && is_root_placement(*type)))
        {
            if (has)
            {
                written.components.push_back(read_component(type, registry, node.entity));
            }
            continue;
        }
        if (!has)
        {
            written.removed.push_back(type);
            continue;
        }
        auto component = read_component(type, registry, node.entity);
        if (!engine::equal_values(*type, created->value.get(), component.value.get()))
        {
            component.created = created;
            written.overrides.push_back(std::move(component));
        }
    }
    return written;
}

void write_components(SceneWriter& out, const std::vector<ComponentToWrite>& components, std::string_view path)
{
    auto& w = out.writer();
    w.StartObject();
    for (const auto& component : components)
    {
        w.Key(component.type->name.c_str());
        out.write_struct(*component.type, component.value.get(), fmt::format("{}: {}", path, component.type->name),
            component.created ? component.created->value.get() : nullptr);
    }
    w.EndObject();
}

// { "components": { ... }, "overrides": { ... }, "removed": [ ... ] }, each only when it isn't empty
void write_node(SceneWriter& out, const NodeToWrite& node, std::string_view path)
{
    auto& w = out.writer();
    w.StartObject();
    if (!node.components.empty())
    {
        w.Key("components");
        write_components(out, node.components, path);
    }
    if (!node.overrides.empty())
    {
        w.Key("overrides");
        write_components(out, node.overrides, path);
    }
    if (!node.removed.empty())
    {
        w.Key("removed");
        w.StartArray();
        for (const auto* type : node.removed)
        {
            w.String(type->name.c_str(), static_cast<rapidjson::SizeType>(type->name.size()));
        }
        w.EndArray();
    }
    w.EndObject();
}

// `entities`: the game objects to save with their persistent IDs, sorted by ID; `snapshot`: what is written is loaded
// back into the same scene (save_game_objects), not saved to a scene file
std::string write_scene(const engine::TypeRegistry& types, const entt::registry& registry, const engine::EntityIdRegistry& ids, const engine::IAssetResolver& assets,
    const engine::IPrefabInstances& prefabs, const std::vector<std::pair<std::uint64_t, entt::entity>>& entities, bool snapshot)
{
    using namespace engine;
    const auto& components = types.get_components();

    // the game objects of an instance (but its root) are made by its prefab: only what differs from it is saved, with
    // the root
    std::set<entt::entity> instance_nodes;
    for (const auto& [id, entity] : entities)
    {
        if (const auto* instance = prefabs.find_instance(entity))
        {
            for (const auto& node : instance->nodes)
            {
                if (node.entity != entity)
                {
                    instance_nodes.insert(node.entity);
                }
            }
        }
    }

    std::vector<EntityToWrite> to_write;
    for (const auto& [id, entity] : entities)
    {
        if (instance_nodes.contains(entity))
        {
            continue;
        }
        EntityToWrite e{ id, entity, {}, nullptr, {} };
        e.instance = prefabs.find_instance(entity);
        if (!e.instance)
        {
            for (const auto* type : components)
            {
                if (type->storage->has(registry, entity))
                {
                    e.components.push_back(read_component(type, registry, entity));
                }
            }
            to_write.push_back(std::move(e));
            continue;
        }
        std::vector<std::string> changes;
        for (const auto& node : e.instance->nodes)
        {
            auto written = instance_node_components(components, registry, node, node.entity == entity, changes);
            if (!written.empty())
            {
                e.nodes.push_back(std::move(written));
            }
        }
        if (!changes.empty() && snapshot)
        {
            log_error(fmt::format("Game object {} is an instance of {}; a game object removed from what its prefab created can't be kept yet, it "
                "comes back with it (e.g. undo of its delete): {}.\n", id, e.instance->prefab, fmt::join(changes, ", ")));
        }
        else if (!changes.empty())
        {
            log_error(fmt::format("Scene save: game object {} is an instance of {}; a game object removed from what its prefab created isn't "
                "saved: {}.\n", id, e.instance->prefab, fmt::join(changes, ", ")));
        }
        std::ranges::sort(e.nodes, {}, [](const NodeToWrite& n) { return std::string_view(n.node->path); });
        to_write.push_back(std::move(e));
    }

    SceneWriter out(ids, assets);
    auto& w = out.writer();
    w.StartObject();
    w.Key("version");
    w.Uint(SCENE_FILE_VERSION);
    w.Key("types");
    w.StartObject();
    for (const auto* type : components)
    {
        const auto written = [type](const std::vector<ComponentToWrite>& list) { return std::ranges::find(list, type, &ComponentToWrite::type) != list.end(); };
        const auto used = std::ranges::any_of(to_write, [&](const EntityToWrite& e) {
            return written(e.components) || std::ranges::any_of(e.nodes, [&](const NodeToWrite& n) { return written(n.components) || written(n.overrides); });
        });
        if (used)
        {
            w.Key(type->name.c_str());
            w.Uint(type->version);
        }
    }
    w.EndObject();
    w.Key("entities");
    w.StartArray();
    for (const auto& e : to_write)
    {
        const auto path = fmt::format("game object {}", e.id);
        w.StartObject();
        w.Key("id");
        w.Uint64(e.id);
        if (!e.instance)
        {
            w.Key("components");
            write_components(out, e.components, path);
        }
        else
        {
            w.Key("prefab");
            w.String(e.instance->prefab.c_str(), static_cast<rapidjson::SizeType>(e.instance->prefab.size()));
            w.Key("nodes");
            w.StartObject();
            for (const auto& node : e.nodes)
            {
                w.Key(node.node->path.c_str(), static_cast<rapidjson::SizeType>(node.node->path.size()));
                write_node(out, node, node.node->path.empty() ? path : fmt::format("{}, node {}", path, node.node->path));
            }
            w.EndObject();
        }
        w.EndObject();
    }
    w.EndArray();
    w.EndObject();
    return out.result() + "\n";
}
}  // namespace

std::string engine::save_scene(const TypeRegistry& types, const entt::registry& registry, const EntityIdRegistry& ids, const IAssetResolver& assets,
    const IPrefabInstances& prefabs)
{
    std::vector<std::pair<std::uint64_t, entt::entity>> entities;
    for (const auto [entity, id] : registry.view<const entity_id_component_t>().each())
    {
        entities.emplace_back(id.id, entity);
    }
    std::ranges::sort(entities);
    return write_scene(types, registry, ids, assets, prefabs, entities, false);
}

std::string engine::save_game_objects(const TypeRegistry& types, const entt::registry& registry, const EntityIdRegistry& ids, const IAssetResolver& assets,
    const IPrefabInstances& prefabs, std::span<const entt::entity> game_objects)
{
    std::vector<std::pair<std::uint64_t, entt::entity>> entities;
    for (const auto entity : game_objects)
    {
        // transient game objects aren't saved, as in save_scene
        if (const auto id = ids.get_id(entity); id != EntityIdRegistry::INVALID_ID)
        {
            entities.emplace_back(id, entity);
        }
    }
    std::ranges::sort(entities);
    entities.erase(std::unique(entities.begin(), entities.end()), entities.end());
    return write_scene(types, registry, ids, assets, prefabs, entities, true);
}

namespace
{
// Parses the file into `doc` and checks that it is a scene file this engine reads (logged when not).
bool parse_scene_file(std::string_view json, std::string_view file_name, rapidjson::Document& doc)
{
    doc.Parse<rapidjson::kParseNumbersAsStringsFlag>(json.data(), json.size());
    if (doc.HasParseError())
    {
        log_error(fmt::format("Scene {}: not valid JSON at offset {}: {} Nothing is loaded.\n", file_name, doc.GetErrorOffset(), rapidjson::GetParseError_En(doc.GetParseError())));
        return false;
    }
    std::uint32_t version = 0;
    if (!doc.IsObject() || !doc.HasMember("version") || !parse_number(doc["version"], version))
    {
        log_error(fmt::format("Scene {}: it has no version, it isn't a scene file. Nothing is loaded.\n", file_name));
        return false;
    }
    if (version > engine::SCENE_FILE_VERSION || version == 0)
    {
        log_error(fmt::format("Scene {}: version {} is not supported (this engine reads up to {}). Nothing is loaded.\n", file_name, version, engine::SCENE_FILE_VERSION));
        return false;
    }
    if (!doc.HasMember("entities") || !doc["entities"].IsArray())
    {
        log_error(fmt::format("Scene {}: it has no entity list. Nothing is loaded.\n", file_name));
        return false;
    }
    return true;
}

// Sets the parent of a game object the C API way (add it with its defaults when it has none, then update it), so the
// scene's listeners run.
void set_parent(const engine::TypeRegistry& types, entt::registry& registry, entt::entity entity, entt::entity parent)
{
    const auto* type = types.find("parent");
    const engine_parent_component_t value{ static_cast<engine_game_object_t>(parent) };
    if (!type->storage->has(registry, entity))
    {
        type->storage->add(registry, entity);
    }
    type->storage->update(registry, entity, &value);
}

// A game object of a scene prefab's file, `file_id` in the file, loaded into an instance: it gets the hashed ID of its
// node path, its ID in the file (no ID when the instance is transient).
entt::entity create_instance_node(const engine::SceneInstanceTarget& instance, std::uint64_t file_id, entt::registry& registry, engine::EntityIdRegistry& ids,
    const engine::EntityHandles& node_handles, std::string_view file_name)
{
    if (instance.ids.root_id == engine::EntityIdRegistry::INVALID_ID)
    {
        return registry.create();
    }
    const auto id = engine::EntityIdRegistry::hashed_id(instance.ids.root_id, fmt::format("{}{}", instance.ids.prefix, file_id));
    const auto entity = ids.create_hashed(id, &node_handles);
    if (const auto new_id = ids.get_id(entity); new_id != id)
    {
        log_error(fmt::format("Scene {}: game object {}, placed in the prefab instance {}, gets the ID {}: its hashed ID {} is used already.\n", file_name, file_id,
            instance.ids.root_id, new_id, id));
    }
    return entity;
}
}  // namespace

bool engine::check_scene_file(std::string_view json, std::string_view file_name)
{
    rapidjson::Document doc;
    return parse_scene_file(json, file_name, doc);
}

bool engine::load_scene(std::string_view json, std::string_view file_name, const TypeRegistry& types, entt::registry& registry, EntityIdRegistry& ids, IAssetResolver& assets,
    IPrefabInstances& prefabs, const SceneLoadOptions& options)
{
    // 1. read and check the file; nothing in the registry changes until it can be used
    rapidjson::Document doc;
    if (!parse_scene_file(json, file_name, doc))
    {
        return false;
    }

    // the versions the components were saved with; a component saved by a newer engine is skipped
    std::set<std::string_view> skipped_components;
    if (doc.HasMember("types") && doc["types"].IsObject())
    {
        for (const auto& member : doc["types"].GetObject())
        {
            const std::string_view type_name(member.name.GetString(), member.name.GetStringLength());
            const auto* type = types.find(type_name);
            std::uint32_t v = 0;
            if (type && type->is_component() && parse_number(member.value, v) && v > type->version)
            {
                log_error(fmt::format("Scene {}: component {} was saved in version {}, this engine reads up to {}. It is not loaded.\n", file_name, type_name, v, type->version));
                skipped_components.insert(type_name);
            }
        }
    }

    std::vector<FileEntity> file_entities;
    std::set<std::uint64_t> file_ids;
    std::set<std::string_view> unknown_components;
    for (const auto& e : doc["entities"].GetArray())
    {
        std::uint64_t id = 0;
        if (!e.IsObject() || !e.HasMember("id") || !parse_number(e["id"], id) || id == EntityIdRegistry::INVALID_ID || id > EntityIdRegistry::MAX_ID)
        {
            log_error(fmt::format("Scene {}: a game object has no valid persistent ID, it is not loaded.\n", file_name));
            continue;
        }
        if (!file_ids.insert(id).second)
        {
            log_error(fmt::format("Scene {}: the persistent ID {} is used twice, the second game object is not loaded.\n", file_name, id));
            continue;
        }
        FileEntity file_entity{ id, nullptr, {}, nullptr };
        if (e.HasMember("components"))
        {
            if (!e["components"].IsObject())
            {
                log_error(fmt::format("Scene {}, game object {}: its components are not an object, it is loaded without them.\n", file_name, id));
            }
            else
            {
                file_entity.components = &e["components"];
                check_components(*file_entity.components, file_name, types, unknown_components);
            }
        }
        if (e.HasMember("prefab"))
        {
            if (!e["prefab"].IsString())
            {
                log_error(fmt::format("Scene {}, game object {}: its prefab is not an asset ID, it is loaded as a game object of its own.\n", file_name, id));
            }
            else
            {
                file_entity.prefab = std::string_view(e["prefab"].GetString(), e["prefab"].GetStringLength());
            }
        }
        if (!file_entity.prefab.empty() && file_entity.components)
        {
            log_error(fmt::format("Scene {}, game object {}: a prefab instance has its components under its nodes (\"\" for the root), they are "
                "ignored.\n", file_name, id));
            file_entity.components = nullptr;
        }
        if (e.HasMember("nodes"))
        {
            if (file_entity.prefab.empty() || !e["nodes"].IsObject())
            {
                log_error(fmt::format("Scene {}, game object {}: its nodes are not an object of a prefab instance, they are ignored.\n", file_name, id));
            }
            else
            {
                file_entity.nodes = &e["nodes"];
                for (const auto& node : file_entity.nodes->GetObject())
                {
                    const std::string_view path(node.name.GetString(), node.name.GetStringLength());
                    if (!node.value.IsObject())
                    {
                        log_error(fmt::format("Scene {}, game object {}: node {} is not an object, it is ignored.\n", file_name, id, path));
                        continue;
                    }
                    for (const auto& member : node.value.GetObject())
                    {
                        const std::string_view key(member.name.GetString(), member.name.GetStringLength());
                        if (key != "components" && key != "overrides" && key != "removed")
                        {
                            log_error(fmt::format("Scene {}, game object {}: node {} has an unknown member \"{}\", it is ignored.\n", file_name, id, path, key));
                        }
                        else if (!node_member(node.value, member.name.GetString()))
                        {
                            log_error(fmt::format("Scene {}, game object {}: the {} of node {} are not {}, they are ignored.\n", file_name, id, key, path,
                                key == "removed" ? "a list of component names" : "an object"));
                        }
                        else if (key == "removed")
                        {
                            for (const auto& name : member.value.GetArray())
                            {
                                const auto* type = name.IsString() ? types.find(std::string_view(name.GetString(), name.GetStringLength())) : nullptr;
                                if (!type || !type->is_component())
                                {
                                    log_error(fmt::format("Scene {}, game object {}: node {} removes something which isn't a known component, it is "
                                        "ignored.\n", file_name, id, path));
                                }
                            }
                        }
                        else
                        {
                            check_components(member.value, file_name, types, unknown_components);
                        }
                    }
                }
            }
        }
        file_entities.push_back(file_entity);
    }

    // 2. every game object exists before any reference to one is read
    // the handles to restore first, each one whose slot is free, so no other game object (a new one, or one whose
    // handle is taken) gets the slot a later one needs; the game objects of the prefab instances take theirs when they
    // are created
    // (in a scene prefab's file, the ones the load of the file placing it reserved for the instance's game objects)
    auto* const instance = options.instance;
    const EntityHandles no_handles;
    EntityHandles reserved;
    if (instance && instance->node_handles)
    {
        reserved = *instance->node_handles;
    }
    for (const auto& [id, handle] : instance ? no_handles : options.handles)
    {
        const auto entity = registry.create(handle);
        if (entity == handle)
        {
            reserved.emplace(id, entity);
        }
        else
        {
            registry.destroy(entity);
        }
    }
    // the IDs of the file first, so a game object which needs a new one can't take the ID of another one in the file
    std::unordered_map<std::uint64_t, entt::entity> entities;
    std::vector<std::uint64_t> used_ids;
    for (const auto& e : file_entities)
    {
        if (instance)
        {
            entities[e.id] = create_instance_node(*instance, e.id, registry, ids, reserved, file_name);
            continue;
        }
        const auto handle = reserved.find(e.id);
        const auto entity = handle == reserved.end() ? registry.create() : handle->second;
        if (options.new_ids)
        {
            ids.assign_next(entity);
        }
        else if (!ids.assign(entity, e.id))
        {
            used_ids.push_back(e.id);
        }
        entities[e.id] = entity;
    }
    for (const auto id : used_ids)
    {
        const auto new_id = ids.assign_next(entities.at(id));
        log_error(fmt::format("Scene {}: the persistent ID {} is used already, its game object gets {}.\n", file_name, id, new_id));
    }
    // a scene prefab's game objects are under its instance's root; the parents the file has are read over it
    if (instance)
    {
        for (const auto& e : file_entities)
        {
            set_parent(types, registry, entities.at(e.id), instance->root);
        }
    }

    // the prefab instances: the file references the game objects of an instance by the IDs they had when it was saved,
    // the hashed IDs of the file's root ID (they are new ones when the root got a new ID)
    std::vector<FileComponents> file_components;
    for (const auto& e : file_entities)
    {
        const auto root = entities.at(e.id);
        if (instance)
        {
            instance->nodes.push_back({ std::to_string(e.id), root, {} });
        }
        if (e.components)
        {
            file_components.push_back({ e.id, {}, e.components, root, nullptr, nullptr, {} });
        }
        if (e.prefab.empty())
        {
            continue;
        }
        // in a scene prefab's file, the game objects of an instance are game objects of the outer instance: its IDs
        const auto instance_ids = instance ? InstanceIds{ instance->ids.root_id, fmt::format("{}{}/", instance->ids.prefix, e.id) } : InstanceIds{ ids.get_id(root), {} };
        const auto* prefab_instance = prefabs.instantiate(e.prefab, root, reserved, instance_ids);
        if (!prefab_instance)
        {
            log_error(fmt::format("Scene {}, game object {}: its prefab {} can't be instantiated, it is loaded without it.\n", file_name, e.id, e.prefab));
        }
        else
        {
            for (const auto& node : prefab_instance->nodes)
            {
                if (node.entity == root)
                {
                    continue;
                }
                if (!entities.emplace(EntityIdRegistry::hashed_id(e.id, node.path), node.entity).second)
                {
                    log_error(fmt::format("Scene {}, game object {}: the ID of its node {} is used by another game object of the file, references to "
                        "the node find the other one.\n", file_name, e.id, node.path));
                }
                if (instance)
                {
                    instance->nodes.push_back({ fmt::format("{}/{}", e.id, node.path), node.entity, {} });
                }
            }
        }
        if (!e.nodes)
        {
            continue;
        }
        for (const auto& node : e.nodes->GetObject())
        {
            if (!node.value.IsObject())
            {
                continue;  // logged when the file was checked
            }
            const std::string_view path(node.name.GetString(), node.name.GetStringLength());
            // the root is there even when its prefab can't be found: what is saved whole of it is loaded
            auto entity = path.empty() ? root : entt::entity{ entt::null };
            if (prefab_instance && !path.empty())
            {
                const auto it = std::ranges::find(prefab_instance->nodes, path, &PrefabInstanceNode::path);
                entity = it == prefab_instance->nodes.end() ? entt::entity{ entt::null } : it->entity;
            }
            if (entity == entt::null)
            {
                log_error(fmt::format("Scene {}, game object {}: its prefab {} has no node {}, what the file has for it is ignored.\n", file_name, e.id, e.prefab, path));
                continue;
            }
            file_components.push_back({ e.id, path, node_member(node.value, "components"), entity, node_member(node.value, "overrides"),
                node_member(node.value, "removed"), e.prefab });
        }
    }
    // reserved for a game object which didn't come (a node the model doesn't have any more); in a scene prefab's file
    // they are the file placing it's, which destroys them once the whole instance is there
    for (const auto& [id, entity] : instance ? no_handles : reserved)
    {
        if (ids.get_id(entity) == EntityIdRegistry::INVALID_ID)
        {
            registry.destroy(entity);
        }
    }
    if (options.loaded && !instance)
    {
        options.loaded->insert(entities.begin(), entities.end());
    }

    // 3. the components, one type after the other: the listeners of a type see the components registered before it
    SceneReader reader(file_name, entities, options.find_references_outside_file && !instance ? &ids : nullptr, assets);
    for (const auto* type : types.get_components())
    {
        if (skipped_components.contains(type->name))
        {
            continue;
        }
        for (const auto& e : file_components)
        {
            const auto find = [&](const Json* components) -> const Json* {
                if (!components)
                {
                    return nullptr;
                }
                const auto it = components->FindMember(type->name.c_str());
                return it == components->MemberEnd() ? nullptr : &it->value;
            };
            const auto read = [&](const Json& json) {
                engine::ValueBuffer value(type->size);
                type->storage->get(registry, e.entity, value.get());
                reader.read_struct(*type, json, value, e.node_path.empty() ? type->name : fmt::format("node {}: {}", e.node_path, type->name));
                type->storage->update(registry, e.entity, value.get());
            };
            reader.set_entity(e.id);
            // removed from what its prefab gave it; nothing to do when the prefab doesn't give it (any more)
            if (e.removed && std::ranges::any_of(e.removed->GetArray(), [&](const Json& name) { return name.IsString() && type->name == name.GetString(); }))
            {
                if (type->storage->has(registry, e.entity))
                {
                    type->storage->remove(registry, e.entity);
                }
            }
            // whole: added with its defaults when the game object doesn't have it, then every field is read (over the
            // prefab's value when the prefab gives it, e.g. the root's transform, or a component the model got since)
            if (const auto* json = find(e.components))
            {
                if (!type->storage->has(registry, e.entity))
                {
                    type->storage->add(registry, e.entity);
                }
                read(*json);
            }
            // an override: the fields which differ, read over the prefab's value; without it there is nothing to override
            if (const auto* json = find(e.overrides))
            {
                if (!type->storage->has(registry, e.entity))
                {
                    log_error(fmt::format("Scene {}, game object {}: {} has an override of {}, which its prefab {} doesn't give it (any more), the "
                        "override is ignored.\n", file_name, e.id, e.node_path.empty() ? std::string("the root") : fmt::format("node {}", e.node_path),
                        type->name, e.prefab));
                    continue;
                }
                read(*json);
            }
        }
        // a scene prefab's game object whose parent isn't in the file (logged, it is read as none) stays under the
        // instance's root
        if (instance && type->name == "parent")
        {
            for (const auto& e : file_entities)
            {
                const auto entity = entities.at(e.id);
                engine_parent_component_t parent{};
                if (type->storage->has(registry, entity))
                {
                    type->storage->get(registry, entity, &parent);
                }
                if (parent.parent == ENGINE_INVALID_GAME_OBJECT_ID || !registry.valid(static_cast<entt::entity>(parent.parent)))
                {
                    set_parent(types, registry, entity, instance->root);
                }
            }
        }
    }
    return true;
}

std::string engine::save_data_asset(const TypeInfo& type, const std::byte* value, const IAssetResolver& assets)
{
    // a data asset holds no game object references (holds_game_object), so no persistent IDs are needed
    entt::registry no_game_objects;
    const EntityIdRegistry no_ids(no_game_objects);
    SceneWriter out(no_ids, assets);
    auto& w = out.writer();
    w.StartObject();
    w.Key("version");
    w.Uint(DATA_ASSET_FILE_VERSION);
    w.Key("type");
    w.String(type.name.c_str(), static_cast<rapidjson::SizeType>(type.name.size()));
    w.Key("value");
    out.write_struct(type, value, type.name);
    w.EndObject();
    return out.result();
}

std::optional<engine::DataAssetValue> engine::load_data_asset(std::string_view json, std::string_view file_name, const TypeRegistry& types, IAssetResolver& assets)
{
    rapidjson::Document doc;
    doc.Parse<rapidjson::kParseNumbersAsStringsFlag>(json.data(), json.size());
    if (doc.HasParseError())
    {
        log_error(fmt::format("Data asset {}: not valid JSON at offset {}: {}\n", file_name, doc.GetErrorOffset(), rapidjson::GetParseError_En(doc.GetParseError())));
        return std::nullopt;
    }
    std::uint32_t version = 0;
    if (!doc.IsObject() || !doc.HasMember("version") || !parse_number(doc["version"], version) || version == 0 || version > DATA_ASSET_FILE_VERSION)
    {
        log_error(fmt::format("Data asset {}: it has no version this engine reads (up to {}), it isn't a data asset file.\n", file_name, DATA_ASSET_FILE_VERSION));
        return std::nullopt;
    }
    const auto type_it = doc.FindMember("type");
    const auto* type = type_it != doc.MemberEnd() && type_it->value.IsString() ? types.find(type_it->value.GetString()) : nullptr;
    if (!type || !type->game_defined || type->is_component() || holds_game_object(*type))
    {
        log_error(fmt::format("Data asset {}: its type isn't a struct type the game registered without game object references.\n", file_name));
        return std::nullopt;
    }
    const auto value_it = doc.FindMember("value");
    if (value_it == doc.MemberEnd() || !value_it->value.IsObject())
    {
        log_error(fmt::format("Data asset {}: it has no value object.\n", file_name));
        return std::nullopt;
    }
    DataAssetValue result{ type, std::make_unique<ValueBuffer>(type->size) };
    if (type->default_value.size() == type->size)
    {
        std::memcpy(result.value->get(), type->default_value.data(), type->size);
    }
    const std::unordered_map<std::uint64_t, entt::entity> no_game_objects;
    SceneReader reader(file_name, no_game_objects, nullptr, assets);
    reader.set_data_asset();
    reader.read_struct(*type, value_it->value, *result.value, type->name);
    return result;
}
