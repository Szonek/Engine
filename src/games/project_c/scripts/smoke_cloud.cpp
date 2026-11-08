#include "smoke_cloud.h"
#include "enemy_script.h"
#include "scripts_utils.h"

#include "iscene.h"

#include <glm/gtc/type_ptr.hpp>

#include <cmath>

namespace
{
// the placeholder look: grey spheres, the cloud's own and two smaller puffs turning slowly around its centre
constexpr float PUFF_SCALE = 0.6f;
constexpr float PUFF_OFFSET = 0.45f;
constexpr float PUFF_TURNS_PER_SECOND = 0.15f;
// where an enemy is measured from, above its feet
constexpr float ENEMY_CHEST_HEIGHT = 0.5f;
}  // namespace

project_c::SmokeCloud::SmokeCloud(engine::IScene* my_scene, engine_game_object_t go, const glm::vec3& landing, const SmokeTuning& tuning)
    : BaseNode(my_scene, go, "smoke-cloud")
    , tuning_(tuning)
    , centre_(landing + glm::vec3(0.0f, tuning.centre_height, 0.0f))
{
    count_++;
    auto tc = engineGameObjectGetTransformComponent(go_);
    tc.position[0] = centre_.x;
    tc.position[1] = centre_.y;
    tc.position[2] = centre_.z;
    engineGameObjectUpdateTransformComponent(go_, &tc);
}

project_c::SmokeCloud::~SmokeCloud()
{
    count_--;
}

void project_c::SmokeCloud::on_start()
{
    // the pop: the enemies it bursts on are blinded and lose the player here; the bang is heard farther (once by each)
    const float search_radius = std::max(tuning_.radius, tuning_.noise_radius);
    for (const auto go : utils::characters_around(centre_, search_radius, ENGINE_INVALID_GAME_OBJECT_ID))
    {
        auto* enemy = my_scene_->get_script<Enemy>(go);
        if (!enemy)
        {
            continue;
        }
        // measured to its chest
        const auto tc = engineGameObjectGetTransformComponent(go);
        const float distance = glm::distance(centre_, glm::vec3(tc.position[0], tc.position[1] + ENEMY_CHEST_HEIGHT, tc.position[2]));
        if (distance <= tuning_.radius)
        {
            enemy->blind(tuning_.blind_seconds, centre_);
        }
        if (distance <= tuning_.noise_radius)
        {
            enemy->hear_loud_noise(centre_);
        }
    }
}

void project_c::SmokeCloud::update(float dt)
{
    age_seconds_ += dt / 1000.0f;
    if (age_seconds_ >= tuning_.duration_seconds)
    {
        // gone: its game object (a runtime one) goes with it
        my_scene_->unregister_script(this);
        return;
    }
    draw();
}

void project_c::SmokeCloud::draw() const
{
    const float radius = this->radius();
    if (radius <= 0.0f)
    {
        return;
    }
    const glm::vec3 grey(0.62f, 0.62f, 0.62f);
    const glm::vec3 light_grey(0.78f, 0.78f, 0.78f);
    engineDebugDrawSphere(glm::value_ptr(centre_), radius, glm::value_ptr(grey));
    const float angle = age_seconds_ * PUFF_TURNS_PER_SECOND * 6.2831853f;
    for (int i = 0; i < 2; i++)
    {
        const float a = angle + static_cast<float>(i) * 3.1415927f;
        const glm::vec3 puff = centre_ + glm::vec3(std::cos(a), i == 0 ? 0.15f : -0.2f, std::sin(a)) * (radius * PUFF_OFFSET);
        engineDebugDrawSphere(glm::value_ptr(puff), radius * PUFF_SCALE, glm::value_ptr(light_grey));
    }
}

bool project_c::utils::smoke_blocks(engine::IScene& scene, const glm::vec3& from, const glm::vec3& to)
{
    if (SmokeCloud::count() == 0)
    {
        return false;
    }
    for (const auto* cloud : scene.get_scripts<SmokeCloud>())
    {
        if (segment_touches_sphere(from, to, cloud->centre(), cloud->radius()))
        {
            return true;
        }
    }
    return false;
}
