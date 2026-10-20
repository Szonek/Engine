#pragma once
#include <cstdint>

namespace engine
{
class Application;

// Marks the root game object of a prefab instance and links it to the prefab asset it was instantiated from. It holds
// a reference to the prefab, so a prefab (and its assets) lives as long as its instances: the reference is released
// when the component or its game object is destroyed, or with the scene.
class prefab_instance_internal_component_t
{
public:
    // acquires the prefab, which must be valid
    prefab_instance_internal_component_t(Application& app, std::uint32_t prefab);
    prefab_instance_internal_component_t(const prefab_instance_internal_component_t&) = delete;
    prefab_instance_internal_component_t& operator=(const prefab_instance_internal_component_t&) = delete;
    prefab_instance_internal_component_t(prefab_instance_internal_component_t&& rhs) noexcept;
    prefab_instance_internal_component_t& operator=(prefab_instance_internal_component_t&& rhs) noexcept;
    ~prefab_instance_internal_component_t();

    std::uint32_t get_prefab() const { return prefab_; }

private:
    void release();

    Application* app_;
    std::uint32_t prefab_;
};
}  // namespace engine
