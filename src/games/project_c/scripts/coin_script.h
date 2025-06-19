#include <engine.h>

#include "base_script.h"
namespace project_c
{
class Coin : public BaseNode
{
public:
    Coin(engine::IScene* my_scene, engine_game_object_t go);

    void update(float dt) override;
    void on_collision(const collision_t& info) override;

    void push_force(float x, float y, float z, engine_force_type_t type);
};
} // namespace project_c