#pragma once
#include "engine.h"

#include <unordered_map>
#include <string>
#include <memory>

namespace engine
{
class IScene;

class SceneManager
{
public:
    SceneManager(engine_application_t& app);
    void update(float dt);
    
    template<typename T>
    engine_result_code_t register_scene(std::string_view scene_name)
    {
        if (scenes_.contains(scene_name.data()))
        {
            return ENGINE_RESULT_CODE_FAIL;
        }
        engine_result_code_t err_code = ENGINE_RESULT_CODE_FAIL;
        scenes_[scene_name.data()] = std::make_unique<T>(app_, this, err_code);
        return err_code;
    }

    IScene* get_scene(std::string_view name);

private:
    std::unordered_map<std::string, std::unique_ptr<IScene>> scenes_;
    engine_application_t app_;
};


} // namespace engine