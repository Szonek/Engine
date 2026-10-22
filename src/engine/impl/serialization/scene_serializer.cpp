#include "scene_serializer.h"
#include "../logger.h"

#include <engine.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
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

    void write_struct(const engine::TypeInfo& type, const std::byte* value, std::string_view path)
    {
        writer_.StartObject();
        for (const auto& field : type.fields)
        {
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
            if (field.is_array)
            {
                writer_.StartArray();
                for (std::size_t i = 0; i < field.count; i++)
                {
                    write_element(field, value + field.offset + i * field.size, field_path);
                }
                writer_.EndArray();
            }
            else
            {
                write_element(field, value + field.offset, field_path);
            }
        }
        writer_.EndObject();
    }

private:
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
    SceneReader(std::string_view file_name, const std::unordered_map<std::uint64_t, entt::entity>& entities, engine::IAssetResolver& assets)
        : file_name_(file_name), entities_(entities), assets_(assets)
    {
    }

    // reads the fields the json object has into `value`, which holds the defaults
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

    void set_entity(std::uint64_t id) { entity_id_ = id; }

private:
    void error(std::string_view path, std::string_view message)
    {
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
            const auto handle = assets_.resolve(field.asset_kind, std::string_view(json.GetString(), json.GetStringLength()));
            if (handle == ENGINE_INVALID_OBJECT_HANDLE)
            {
                error(path, fmt::format("uses the asset \"{}\" which can't be found, it has none", json.GetString()));
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
            if (it == entities_.end())
            {
                error(path, fmt::format("references the game object {} which isn't in the file, it references none", id));
                write_as(p, static_cast<std::uint32_t>(ENGINE_INVALID_GAME_OBJECT_ID));
                return;
            }
            write_as(p, static_cast<std::uint32_t>(it->second));
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
    engine::IAssetResolver& assets_;
    std::uint64_t entity_id_ = 0;
};

struct FileEntity
{
    std::uint64_t id;
    const Json* components;
};
}  // namespace

std::string engine::save_scene(const TypeRegistry& types, const entt::registry& registry, const EntityIdRegistry& ids, const IAssetResolver& assets)
{
    std::vector<std::pair<std::uint64_t, entt::entity>> entities;
    for (const auto [entity, id] : registry.view<const entity_id_component_t>().each())
    {
        entities.emplace_back(id.id, entity);
    }
    std::ranges::sort(entities);
    const auto components = types.get_components();

    SceneWriter out(ids, assets);
    auto& w = out.writer();
    w.StartObject();
    w.Key("version");
    w.Uint(SCENE_FILE_VERSION);
    w.Key("types");
    w.StartObject();
    for (const auto* type : components)
    {
        const auto used = std::ranges::any_of(entities, [&](const auto& e) { return type->storage->has(registry, e.second); });
        if (used)
        {
            w.Key(type->name.c_str());
            w.Uint(type->version);
        }
    }
    w.EndObject();
    w.Key("entities");
    w.StartArray();
    for (const auto& [id, entity] : entities)
    {
        w.StartObject();
        w.Key("id");
        w.Uint64(id);
        w.Key("components");
        w.StartObject();
        for (const auto* type : components)
        {
            if (!type->storage->has(registry, entity))
            {
                continue;
            }
            engine::ValueBuffer value(type->size);
            type->storage->get(registry, entity, value.get());
            w.Key(type->name.c_str());
            out.write_struct(*type, value.get(), fmt::format("game object {}: {}", id, type->name));
        }
        w.EndObject();
        w.EndObject();
    }
    w.EndArray();
    w.EndObject();
    return out.result() + "\n";
}

bool engine::load_scene(std::string_view json, std::string_view file_name, const TypeRegistry& types, entt::registry& registry, EntityIdRegistry& ids, IAssetResolver& assets)
{
    // 1. read and check the file; nothing in the registry changes until it can be used
    rapidjson::Document doc;
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
    if (version > SCENE_FILE_VERSION || version == 0)
    {
        log_error(fmt::format("Scene {}: version {} is not supported (this engine reads up to {}). Nothing is loaded.\n", file_name, version, SCENE_FILE_VERSION));
        return false;
    }
    if (!doc.HasMember("entities") || !doc["entities"].IsArray())
    {
        log_error(fmt::format("Scene {}: it has no entity list. Nothing is loaded.\n", file_name));
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
        const Json* components = nullptr;
        if (e.HasMember("components"))
        {
            if (!e["components"].IsObject())
            {
                log_error(fmt::format("Scene {}, game object {}: its components are not an object, it is loaded without them.\n", file_name, id));
            }
            else
            {
                components = &e["components"];
                for (const auto& c : components->GetObject())
                {
                    const std::string_view name(c.name.GetString(), c.name.GetStringLength());
                    const auto* type = types.find(name);
                    if ((!type || !type->is_component()) && unknown_components.insert(name).second)
                    {
                        log_error(fmt::format("Scene {}: unknown component {}, it is ignored.\n", file_name, name));
                    }
                }
            }
        }
        file_entities.push_back({ id, components });
    }

    // 2. every game object exists before any reference to one is read
    // the IDs of the file first, so a game object which needs a new one can't take the ID of another one in the file
    std::unordered_map<std::uint64_t, entt::entity> entities;
    std::vector<std::uint64_t> used_ids;
    for (const auto& e : file_entities)
    {
        const auto entity = registry.create();
        if (!ids.assign(entity, e.id))
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

    // 3. the components, one type after the other: the listeners of a type see the components registered before it
    SceneReader reader(file_name, entities, assets);
    for (const auto* type : types.get_components())
    {
        if (skipped_components.contains(type->name))
        {
            continue;
        }
        for (const auto& e : file_entities)
        {
            if (!e.components)
            {
                continue;
            }
            const auto it = e.components->FindMember(type->name.c_str());
            if (it == e.components->MemberEnd())
            {
                continue;
            }
            const auto entity = entities.at(e.id);
            reader.set_entity(e.id);
            if (!type->storage->has(registry, entity))
            {
                type->storage->add(registry, entity);
            }
            engine::ValueBuffer value(type->size);
            type->storage->get(registry, entity, value.get());
            reader.read_struct(*type, it->value, value.get(), type->name);
            type->storage->update(registry, entity, value.get());
        }
    }
    return true;
}
