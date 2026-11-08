#pragma once
#include "base_script.h"
#include "smoke.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace engine
{
class IScene;
}

namespace project_c
{
// A smoke bomb's cloud (SMOKE.md): a runtime object (transient, never saved) the player's throw spawns where the bomb
// lands. When it starts it pops: the enemies within its full radius are blinded and lose track of the player, the ones
// within the noise radius hear a loud noise. Then it grows, holds and fades (cloud_radius), hiding whatever is behind or
// inside it from the enemies' sight (smoke_blocks), and goes when its life ends. Drawn as grey wire spheres (a
// placeholder for a particle effect).
class SmokeCloud : public BaseNode
{
public:
    // `landing`: where the bomb landed; the cloud's values are the throw's (`tuning`)
    SmokeCloud(engine::IScene* my_scene, engine_game_object_t go, const glm::vec3& landing, const SmokeTuning& tuning);
    ~SmokeCloud();

    void on_start() override;
    void update(float dt) override;

    const glm::vec3& centre() const { return centre_; }
    // now (0 before it popped and after it faded)
    float radius() const { return cloud_radius(age_seconds_, tuning_); }

    // how many clouds there are (in every scene): sight only asks them while there is one
    static std::uint32_t count() { return count_; }

private:
    void draw() const;

    SmokeTuning tuning_;
    glm::vec3 centre_;
    float age_seconds_ = 0.0f;
    static inline std::uint32_t count_ = 0;
};

namespace utils
{
// whether a smoke cloud of `scene` is on the line of sight from `from` to `to` (or around either end)
bool smoke_blocks(engine::IScene& scene, const glm::vec3& from, const glm::vec3& to);
}  // namespace utils
}  // namespace project_c
