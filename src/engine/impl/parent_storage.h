#pragma once
// Force-included into every C++ source of the engine and of the tests compiling engine sources (see the CMakeLists):
// the storage EnTT uses for a component type must be the same in every translation unit.
#include "components/parent_component.h"

#include <entt/entity/fwd.hpp>
#include <entt/entity/mixin.hpp>
#include <entt/entity/storage.hpp>
#include <entt/signal/sigh.hpp>

#include <utility>

namespace engine
{
// The storage of the parent component: a change of a parent component (replace, patch, emplace_or_replace) publishes
// the value it had, after the change and before the registry's update signal, like Bevy's on_replace hook. So the
// children lists, derived from the parent components, are moved from the old parent to the new one (hierarchy.h)
// without keeping a copy of the parent anywhere.
template<typename Storage>
class parent_storage_mixin : public Storage
{
public:
    using change_signal = entt::sigh<void(entt::entity, const engine_parent_component_t& previous)>;

    using Storage::Storage;

    // (entity, its parent component before the change); the component holds the new value
    entt::sink<change_signal> on_change()
    {
        return entt::sink{ change_ };
    }

    template<typename... Func>
    decltype(auto) patch(const typename Storage::entity_type entity, Func&&... func)
    {
        const auto previous = this->get(entity);
        Storage::patch(entity, std::forward<Func>(func)...);
        change_.publish(entity, previous);
        return this->get(entity);
    }

private:
    change_signal change_;
};
}  // namespace engine

template<typename Entity, typename Allocator>
struct entt::storage_type<engine_parent_component_t, Entity, Allocator>
{
    using type = entt::sigh_mixin<engine::parent_storage_mixin<entt::basic_storage<engine_parent_component_t, Entity, Allocator>>>;
};
