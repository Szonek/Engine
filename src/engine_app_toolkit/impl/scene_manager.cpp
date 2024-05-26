#include "scene_manager.h"
#include "iscene.h"

engine::SceneManager::SceneManager(IApplication* app)
    : app_(app)
{
}

void engine::SceneManager::update(float dt)
{
    for (auto& [_, scene] : scenes_)
    {
        scene->update(dt);
    }
}

engine::IScene* engine::SceneManager::get_scene(std::string_view name)
{
    if (!scenes_.contains(name.data()))
    {
        return nullptr;
    }
    return scenes_[name.data()].get();
}