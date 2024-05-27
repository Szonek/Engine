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
class IApplication;
class ENGINE_APP_TOOLKIT_API SceneManager
{
public:
    SceneManager(IApplication* app);
    ~SceneManager() = default;
    void update(float dt);
    
    template<typename T, typename ...TUserArgs>
    T* register_scene(TUserArgs&&... args)
    {
        if (get_scene(T::get_name()) != nullptr)
        {
            throw std::runtime_error("Scene already exists");
        }
        auto ret = std::make_shared<T>(app_, args...);
        scenes_[T::get_name()] = ret;
        return ret.get();
    }

    IScene* get_scene(std::string_view name);
    void unregister_scene(std::string_view name);

private:
    IApplication* app_ = nullptr;
    std::unordered_map<std::string,std::shared_ptr<IScene>> scenes_;  //ToDo: should use unique_ptr
};


} // namespace engine