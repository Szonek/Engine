#pragma once
#include "engine.h"
#include "utils.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <string_view>
#include <stdexcept>

namespace engine
{
class IScene;

class ENGINE_APP_TOOLKIT_API SceneManager
{
public:
    SceneManager(engine_application_t& app);
    void update(float dt);
    
    template<typename T, typename ...TUserArgs>
    T* register_scene(TUserArgs&&... args)
    {
        if (scenes_.contains(T::get_name()))
        {
            throw std::runtime_error("Scene already exists");
        }
        //scenes_[T::get_name()] = std::make_unique<T>(app_, this, err_code, args...);
        auto ret = new T(app_, this, args...);
        scenes_[T::get_name()] = ret;
        return ret;
    }

    IScene* get_scene(std::string_view name);

private:
    std::unordered_map<std::string, IScene*> scenes_;  //ToDo: make IScene std::unique_ptr !!
    engine_application_t app_;
};


} // namespace engine