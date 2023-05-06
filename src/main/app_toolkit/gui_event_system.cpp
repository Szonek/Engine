#include "gui_event_system.h"

#include "iscene.h"

#include <iostream>

void engine::GuiEventSystem::update(IScene* iscene)
{
    auto scene = iscene->get_handle();
    engine_component_view_t rect_tranform_view{};
    engineCreateComponentView(&rect_tranform_view);
    engineSceneComponentViewAttachRectTransformComponent(scene, rect_tranform_view);

    engine_component_iterator_t begin_it{};
    engineComponentViewCreateBeginComponentIterator(rect_tranform_view, &begin_it);
    engine_component_iterator_t end_it{};
    engineComponentViewCreateEndComponentIterator(rect_tranform_view, &end_it);

    size_t idx = 0;
    while (engineComponentIteratorCheckEqual(begin_it, end_it) == false)
    {
        std::cout << idx++ << std::endl;
        const auto game_obj = engineComponentIteratorGetGameObject(begin_it);
        const auto rect_transform = engineSceneGetRectTransformComponent(scene, game_obj);
        std::cout << rect_transform.position[0] << ", " << rect_transform.position[1] << std::endl;
        engineComponentIteratorNext(begin_it);
    }

    if (begin_it)
    {
        engineDeleteComponentIterator(begin_it);
    }

    if (end_it)
    {
        engineDeleteComponentIterator(end_it);
    }

    if (rect_tranform_view)
    {
        engineDestroyComponentView(rect_tranform_view);
    }
}
