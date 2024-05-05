#include "ui_document.h"
#include "ui_manager.h"
#include "asset_store.h"
#include "engine.h"
#include "logger.h"
#include "math_helpers.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ID.h>
#include <RmlUi/Core/DataModelHandle.h>

engine::UiDocument::UiDocument(Rml::Context* ctx, std::string_view file_name)
    : doc_(ctx->LoadDocument((AssetStore::get_instance().get_ui_docs_base_path() / file_name).string()))
    , context_(ctx)
{   
}

engine::UiDocument::UiDocument(UiDocument&& rhs)
{
    std::swap(doc_, rhs.doc_);
}

engine::UiDocument& engine::UiDocument::operator=(UiDocument&& rhs)
{
    if (this != &rhs)
    {
        std::swap(doc_, rhs.doc_);
    }
    return *this;
}

engine::UiDocument::~UiDocument()
{
    context_->UnloadDocument(doc_);
}

void engine::UiDocument::show()
{
    doc_->Show();
}

void engine::UiDocument::hide()
{
    doc_->Hide();
}

engine::UiElement* engine::UiDocument::get_element_by_id(std::string_view id, engine_result_code_t& err_out)
{
    err_out = ENGINE_RESULT_CODE_OK;
    if (!cached_ui_elements_.contains(id.data()))
    {
        cached_ui_elements_[id.data()] = UiElement(doc_->GetElementById(id.data()), err_out);
        if (err_out != ENGINE_RESULT_CODE_OK)
        {
            cached_ui_elements_.erase(id.data());

        }
    }
    return err_out == ENGINE_RESULT_CODE_OK ? &cached_ui_elements_[id.data()] : nullptr;
}

engine::UiDataHandle::UiDataHandle(Rml::DataModelConstructor* constructor, std::span<const engine_ui_document_data_binding_t> bindings)
{
    if (!constructor)
    {
        return;
    }

    for (const auto& bind : bindings)
    {
        switch (bind.type)
        {
        case ENGINE_DATA_TYPE_BOOL:
        {
            constructor->Bind(bind.name, bind.data_bool);
            break;
        }
        case ENGINE_DATA_TYPE_UINT32:
        {
            constructor->Bind(bind.name, bind.data_uint32_t);
            break;
        }
        default:
            log::log(log::LogLevel::eError, "Unknown engine data type. Cant create data binding for UI.");
        }
    }
    handle_ = new Rml::DataModelHandle(constructor->GetModelHandle());
}

engine::UiDataHandle::UiDataHandle(UiDataHandle&& rhs)
{
    std::swap(handle_, rhs.handle_);
}

engine::UiDataHandle& engine::UiDataHandle::operator=(UiDataHandle&& rhs)
{
    if (this != &rhs)
    {
        std::swap(handle_, rhs.handle_);
    }
    return *this;
}

engine::UiDataHandle::~UiDataHandle()
{
    if (handle_)
    {
        delete handle_;
    }
}

void engine::UiDataHandle::dirty_all_variables()
{
    handle_->DirtyAllVariables();
}

void engine::UiDataHandle::dirty_variable(std::string_view name)
{
    handle_->DirtyVariable(name.data());
}

engine::UiElement::UiElement(Rml::Element* element, engine_result_code_t& err_out)
    : element_(element)
{
    err_out = element_ ? ENGINE_RESULT_CODE_OK : ENGINE_RESULT_CODE_FAIL;
}

engine::UiElement::UiElement(UiElement&& rhs)
{
    std::swap(element_, rhs.element_);
}

engine::UiElement& engine::UiElement::operator=(UiElement&& rhs)
{
    if (this != &rhs)
    {
        std::swap(element_, rhs.element_);
    }
    return *this;
}

engine::UiElement::~UiElement()
{
    element_ = nullptr;
}

bool engine::UiElement::register_callback(engine_ui_event_type_t type, void* user_data, fnCallbackT&& callback)
{
    Rml::EventId rml_ev_id = Rml::EventId::Invalid;
    switch (type)
    {
    case ENGINE_UI_EVENT_TYPE_POINTER_CLICK:
    {
        rml_ev_id = Rml::EventId::Click;
        break;
    }
    case ENGINE_UI_EVENT_TYPE_POINTER_DOWN:
    {
        rml_ev_id = Rml::EventId::Mousedown;
        break;
    }
    case ENGINE_UI_EVENT_TYPE_POINTER_UP:
    {
        rml_ev_id = Rml::EventId::Mouseup;
        break;
    }
    case ENGINE_UI_EVENT_TYPE_POINTER_MOVE:
    {
        rml_ev_id = Rml::EventId::Mousemove;
        break;
    }
    default:
        engine::log::log(log::LogLevel::eCritical, "Unknown engine_ui_event_type_t. Cant creatre UI callback!");
    }

    if (rml_ev_id == Rml::EventId::Invalid)
    {
        return false;
    }

    const bool ret = listeners_.count(type);
    if (ret)
    {
        engine::log::log(log::LogLevel::eError, "Overwritting callback function for UiElement!");
    }

    listeners_[type] = BasicEventListener(std::move(callback), user_data);
    element_->AddEventListener(rml_ev_id, &listeners_[type]);
    return ret;
}

bool engine::UiElement::set_property(std::string_view name, std::string_view value)
{
    return element_->SetProperty(name.data(), value.data());
}

void engine::UiElement::remove_property(std::string_view name)
{
    element_->RemoveProperty(name.data());
}

void engine::UiElement::BasicEventListener::ProcessEvent(Rml::Event& event)
{
    const auto ev = parse_rml_event_to_engine_event(event);
    callback_(&ev, user_data_);
}

engine_ui_event_t engine::UiElement::BasicEventListener::parse_rml_event_to_engine_event(const Rml::Event& event)
{
    engine_ui_event_t ev{};
    switch (event.GetId())
    {
    case Rml::EventId::Click:
    {
        ev.type = ENGINE_UI_EVENT_TYPE_POINTER_CLICK;
        break;
    }
    case Rml::EventId::Mousedown:
    {
        ev.type = ENGINE_UI_EVENT_TYPE_POINTER_DOWN;
        break;
    }
    case Rml::EventId::Mouseup:
    {
        ev.type = ENGINE_UI_EVENT_TYPE_POINTER_UP;
        break;
    }
    case Rml::EventId::Mousemove:
    {
        ev.type = ENGINE_UI_EVENT_TYPE_POINTER_MOVE;
        break;
    }
    default:
        ev.type = ENGINE_UI_EVENT_TYPE_UNKNOWN;
        engine::log::log(log::LogLevel::eCritical, "Unknown engine_ui_event_type_t. Cant process event correctly.");
    }

    const auto context_dims = event.GetCurrentElement()->GetContext()->GetDimensions();
    ev.normalized_screen_position = { event.GetUnprojectedMouseScreenPos().x / context_dims.x, (context_dims.y - event.GetUnprojectedMouseScreenPos().y) / context_dims.y };
    return ev;
}
