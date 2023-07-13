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
    
    template<typename T, typename ...TUserArgs>
    engine_result_code_t register_scene(TUserArgs... args)
    {
        if (scenes_.contains(T::get_name()))
        {
            return ENGINE_RESULT_CODE_FAIL;
        }
        engine_result_code_t err_code = ENGINE_RESULT_CODE_FAIL;
        scenes_[T::get_name()] = std::make_unique<T>(app_, this, err_code, args...);
        return err_code;
    }

    IScene* get_scene(std::string_view name);

private:
    std::unordered_map<std::string, std::unique_ptr<IScene>> scenes_;
    engine_application_t app_;
};


} // namespace engine