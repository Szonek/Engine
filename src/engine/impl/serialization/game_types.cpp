#include "game_types.h"

#include <fmt/format.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
using engine::FieldInfo;
using engine::FieldKind;
using engine::TypeInfo;

// the value of a game-defined component; each type has a storage of these under its own id, sized for it
template<std::size_t N>
struct alignas(engine::DATA_SLOT_ALIGNMENT) DataBlob
{
    std::byte bytes[N];
};

// how many times components of each game type were added, updated or removed in a registry (kept in its context), by
// storage id
struct DataComponentChangeCounts
{
    std::unordered_map<entt::id_type, std::uint64_t> counts;
};

void count_change(entt::registry& r, entt::id_type id)
{
    auto* changes = r.ctx().find<DataComponentChangeCounts>();
    if (!changes)
    {
        changes = &r.ctx().emplace<DataComponentChangeCounts>();
    }
    changes->counts[id]++;
}

template<std::size_t N>
std::unique_ptr<engine::ComponentStorage> make_storage(entt::id_type id, std::vector<std::byte> default_value)
{
    using Blob = DataBlob<N>;
    const auto size = default_value.size();
    auto s = std::make_unique<engine::ComponentStorage>();
    s->has = [id](const entt::registry& r, entt::entity e)
    {
        const auto* pool = r.storage(id);
        return pool && pool->contains(e);
    };
    s->add = [id, default_value = std::move(default_value)](entt::registry& r, entt::entity e)
    {
        Blob blob{};
        std::memcpy(blob.bytes, default_value.data(), default_value.size());
        r.storage<Blob>(id).emplace(e, blob);
        count_change(r, id);
    };
    s->get = [id, size](const entt::registry& r, entt::entity e, void* out)
    {
        std::memcpy(out, r.storage<Blob>(id)->get(e).bytes, size);
    };
    s->update = [id, size](entt::registry& r, entt::entity e, const void* value)
    {
        r.storage<Blob>(id).patch(e, [value, size](Blob& blob) { std::memcpy(blob.bytes, value, size); });
        count_change(r, id);
    };
    s->remove = [id](entt::registry& r, entt::entity e)
    {
        if (auto* pool = r.storage(id); pool && pool->remove(e))
        {
            count_change(r, id);
        }
    };
    s->pool = [id](entt::registry& r) -> entt::sparse_set& { return r.storage<Blob>(id); };
    s->change_count = [id](const entt::registry& r) -> std::uint64_t
    {
        const auto* changes = r.ctx().find<DataComponentChangeCounts>();
        if (!changes)
        {
            return 0;
        }
        const auto it = changes->counts.find(id);
        return it == changes->counts.end() ? 0 : it->second;
    };
    return s;
}

// make_storage for the slot size `slot` (one of the data_slot_size values), chosen at runtime: the I-th slot type holds
// I + 1 alignments
template<std::size_t... I>
std::unique_ptr<engine::ComponentStorage> make_storage_for_slot(std::size_t slot, entt::id_type id, std::vector<std::byte> default_value, std::index_sequence<I...>)
{
    using engine::DATA_SLOT_ALIGNMENT;
    std::unique_ptr<engine::ComponentStorage> storage;
    ((slot == (I + 1) * DATA_SLOT_ALIGNMENT ? (storage = make_storage<(I + 1) * DATA_SLOT_ALIGNMENT>(id, std::move(default_value)), true) : false) || ...);
    return storage;
}

std::string_view text(const char* s)
{
    return s ? std::string_view(s) : std::string_view();
}

class GameTypeBuilder
{
public:
    GameTypeBuilder(engine::TypeRegistry& registry, const engine_type_desc_t& desc)
        : registry_(registry), desc_(desc)
    {
        type_.name = std::string(text(desc.name));
        // 0 is "not given" (a zeroed description)
        type_.version = desc.version == 0 ? engine::FIRST_TYPE_VERSION : desc.version;
        type_.size = desc.size;
        type_.alignment = desc.alignment;
        type_.game_defined = true;
    }

    const TypeInfo* build(bool component)
    {
        if (type_.name.empty())
        {
            error("it has no name");
        }
        if (desc_.size == 0 || desc_.alignment == 0 || !std::has_single_bit(desc_.alignment) || desc_.alignment > engine::DATA_SLOT_ALIGNMENT || desc_.size % desc_.alignment != 0)
        {
            error(fmt::format("size {} and alignment {} aren't the size and alignment of a struct (aligned to at most {})", desc_.size, desc_.alignment, engine::DATA_SLOT_ALIGNMENT));
        }
        if (component && desc_.size > ENGINE_COMPONENT_TYPE_MAX_SIZE)
        {
            error(fmt::format("a component is at most {} bytes, it has {}", ENGINE_COMPONENT_TYPE_MAX_SIZE, desc_.size));
        }
        if (desc_.fields_count > 0 && !desc_.fields)
        {
            error("it has no fields array");
        }
        else
        {
            for (std::uint32_t i = 0; i < desc_.fields_count; i++)
            {
                add_field(desc_.fields[i]);
            }
        }
        check_overlaps();
        const auto id = storage_id(type_.name);
        if (component)
        {
            // two names with the same hash would share a storage (of another size)
            for (const auto* other : registry_.get_components())
            {
                if (other->game_defined && storage_id(other->name) == id)
                {
                    error(fmt::format("its storage id is the one of {}, rename one of them", other->name));
                }
            }
        }
        if (failed_)
        {
            return nullptr;
        }
        if (component)
        {
            // no default value is zeroes: a value holding nothing (no asset or game object has the handle 0)
            std::vector<std::byte> default_value(type_.size);
            if (desc_.default_value)
            {
                std::memcpy(default_value.data(), desc_.default_value, type_.size);
            }
            type_.storage = make_storage_for_slot(engine::data_slot_size(type_.size), id, std::move(default_value),
                std::make_index_sequence<ENGINE_COMPONENT_TYPE_MAX_SIZE / engine::DATA_SLOT_ALIGNMENT>{});
        }
        return registry_.add(std::move(type_));
    }

private:
    static entt::id_type storage_id(const std::string& name)
    {
        return entt::hashed_string::value(("game_component/" + name).c_str());
    }

    void add_field(const engine_field_desc_t& d)
    {
        const auto name = text(d.name);
        if (name.empty())
        {
            return error("a field has no name");
        }
        if (type_.find_field(name))
        {
            return error(fmt::format("field {} is described twice", name));
        }
        FieldInfo f{};
        f.name = std::string(name);
        f.offset = d.offset;
        f.size = 4;
        f.alignment = 4;
        f.is_array = d.count > 0;
        f.count = d.count > 0 ? d.count : 1;
        switch (d.kind)
        {
        case ENGINE_FIELD_KIND_BOOL: f.kind = FieldKind::eBool; f.size = f.alignment = sizeof(bool); break;
        case ENGINE_FIELD_KIND_INT32: f.kind = FieldKind::eInt32; break;
        case ENGINE_FIELD_KIND_UINT32: f.kind = FieldKind::eUint32; break;
        case ENGINE_FIELD_KIND_FLOAT: f.kind = FieldKind::eFloat; break;
        case ENGINE_FIELD_KIND_STRING:
            if (d.count == 0)
            {
                return error(fmt::format("field {}: a string needs the size of its char array as its count", name));
            }
            f.kind = FieldKind::eString;
            f.size = d.count;
            f.alignment = 1;
            f.is_array = false;
            f.count = 1;
            break;
        case ENGINE_FIELD_KIND_ENUM:
            f.kind = FieldKind::eEnum;
            if (!read_enum_values(d, f))
            {
                return;
            }
            break;
        case ENGINE_FIELD_KIND_STRUCT:
        {
            f.kind = FieldKind::eStruct;
            // a struct type of the game: the engine's may hold unions, which game types don't support
            f.struct_type = registry_.find(text(d.type_name));
            if (!f.struct_type || !f.struct_type->game_defined || f.struct_type->is_component())
            {
                return error(fmt::format("field {}: \"{}\" is not a struct type registered by the game", name, text(d.type_name)));
            }
            f.size = f.struct_type->size;
            f.alignment = f.struct_type->alignment;
            break;
        }
        case ENGINE_FIELD_KIND_GEOMETRY: f.kind = FieldKind::eAsset; f.asset_kind = engine::AssetKind::eGeometry; break;
        case ENGINE_FIELD_KIND_TEXTURE: f.kind = FieldKind::eAsset; f.asset_kind = engine::AssetKind::eTexture; break;
        case ENGINE_FIELD_KIND_GAME_OBJECT: f.kind = FieldKind::eGameObject; break;
        default:
            return error(fmt::format("field {} has an unknown kind {}", name, static_cast<int>(d.kind)));
        }
        if (f.alignment > type_.alignment)
        {
            return error(fmt::format("field {} is aligned to {}, more than the struct's alignment {}", name, f.alignment, type_.alignment));
        }
        if (f.offset % f.alignment != 0)
        {
            return error(fmt::format("field {}: offset {} is not aligned for its kind (to {})", name, f.offset, f.alignment));
        }
        if (f.offset + f.byte_size() > type_.size)
        {
            return error(fmt::format("field {}: bytes {} to {} are outside the struct of {} bytes", name, f.offset, f.offset + f.byte_size(), type_.size));
        }
        type_.fields.push_back(std::move(f));
    }

    bool read_enum_values(const engine_field_desc_t& d, FieldInfo& f)
    {
        if (d.enum_values_count == 0 || !d.enum_values)
        {
            error(fmt::format("enum field {} has no values", f.name));
            return false;
        }
        for (std::uint32_t i = 0; i < d.enum_values_count; i++)
        {
            const auto value_name = text(d.enum_values[i].name);
            const auto value = d.enum_values[i].value;
            // each value is saved by its name and read back from it
            if (value_name.empty()
                || std::ranges::find(f.enum_values, value_name, &engine::EnumValue::name) != f.enum_values.end()
                || std::ranges::find(f.enum_values, value, &engine::EnumValue::value) != f.enum_values.end())
            {
                error(fmt::format("enum field {}: each value needs a name, and names and values are unique", f.name));
                return false;
            }
            f.enum_values.push_back({ std::string(value_name), value });
        }
        return true;
    }

    // a field offset by mistake overlaps another one
    void check_overlaps()
    {
        std::vector<const FieldInfo*> sorted;
        for (const auto& f : type_.fields)
        {
            sorted.push_back(&f);
        }
        std::ranges::sort(sorted, {}, &FieldInfo::offset);
        for (std::size_t i = 1; i < sorted.size(); i++)
        {
            if (sorted[i]->offset < sorted[i - 1]->offset + sorted[i - 1]->byte_size())
            {
                error(fmt::format("fields {} and {} overlap", sorted[i - 1]->name, sorted[i]->name));
            }
        }
    }

    void error(std::string_view message)
    {
        failed_ = true;
        engine::type_builder_detail::report_error(type_.name, message);
    }

    engine::TypeRegistry& registry_;
    const engine_type_desc_t& desc_;
    TypeInfo type_;
    bool failed_ = false;
};
}  // namespace

const engine::TypeInfo* engine::register_game_type(TypeRegistry& registry, const engine_type_desc_t& desc, bool component)
{
    return GameTypeBuilder(registry, desc).build(component);
}
