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
};
} // namespace project_c