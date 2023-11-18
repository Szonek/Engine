#pragma once
#include "engine.h"
#include "utils.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <string_view>

namespace engine
{
class IScene;

class ENGINE_APP_TOOLKIT_API SceneManager
{
public:
    SceneManager(engine_application_t& app);
    void update(float dt);
    
    template<typename T, typename ...TUserArgs>
    engine_result_code_t register_scene(TUserArgs&&... args)
    {
        if (scenes_.contains(T::get_name()))
        {
            return ENGINE_RESULT_CODE_FAIL;
        }
        engine_result_code_t err_code = ENGINE_RESULT_CODE_FAIL;
        //scenes_[T::get_name()] = std::make_unique<T>(app_, this, err_code, args...);
        scenes_[T::get_name()] = new T(app_, this, err_code, args...);
        return err_code;
    }

    IScene* get_scene(std::string_view name);

private:
    std::unordered_map<std::string, IScene*> scenes_;  //ToDo: make IScene std::unique_ptr !!
    engine_application_t app_;
};


} // namespace engine