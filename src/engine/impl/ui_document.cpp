#include "ui_document.h"
#include "ui_manager.h"
#include "asset_store.h"
#include "engine.h"
#include "logger.h"
#include "math_helpers.h"
#include "engine_string_impl_def.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ID.h>
#include <RmlUi/Core/DataModelHandle.h>


namespace
{
inline engine_ui_event_t convet_rml_event_to_engine_event(const Rml::Event& event)
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
    case Rml::EventId::Mouseover:
    {
        ev.type = ENGINE_UI_EVENT_TYPE_POINTER_MOVE;
        break;
    }
    case Rml::EventId::Mouseout:
    {
        ev.type = ENGINE_UI_EVENT_TYPE_POINTER_OUT;
        break;
    }
    default:
        ev.type = ENGINE_UI_EVENT_TYPE_UNKNOWN;
        engine::log::log(engine::log::LogLevel::eCritical, "Unknown engine_ui_event_type_t. Cant process event correctly.");
    }

    ev.normalized_screen_position.x = event.GetParameter("mouse_x", 0.0f);
    ev.normalized_screen_position.y = event.GetParameter("mouse_y", 0.0f);

    return ev;
}
}

engine::UiDocument::UiDocument(Rml::Context* ctx, std::string_view file_name)
    : doc_file_path_(AssetStore::get_instance().get_ui_docs_base_path() / file_name)
    , doc_(ctx->LoadDocument(doc_file_path_.string()))
    , context_(ctx)
{   
}

engine::UiDocument::UiDocument(UiDocument&& rhs) noexcept
{
    std::swap(doc_, rhs.doc_);
}

engine::UiDocument& engine::UiDocument::operator=(UiDocument&& rhs) noexcept
{
    if (this != &rhs)
    {
        std::swap(doc_, rhs.doc_);
    }
    return *this;
}

engine::UiDocument::~UiDocument()
{
    doc_->Close();
}

void engine::UiDocument::show()
{
    doc_->Show();
}

void engine::UiDocument::hide()
{
    doc_->Hide();
}

void engine::UiDocument::reload()
{
    doc_->Close();
    doc_ = context_->LoadDocument(doc_file_path_.string());
    doc_->ReloadStyleSheet();
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

engine::UiDataHandle::UiDataHandle(Rml::Context* ctx, std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings)
    : context_(ctx)
    , name_(name)
{
    auto constructor = ctx->CreateDataModel(name.data());
    if (!constructor)
    {
        return;
    }

    for (const auto& bind : bindings)
    {
        switch (bind.type)
        {
        case ENGINE_UI_DOCUMENT_DATA_TYPE_BOOL:
        {
            constructor.Bind(bind.name, bind.data_bool);
            break;
        }
        case ENGINE_UI_DOCUMENT_DATA_TYPE_UINT32:
        {
            constructor.Bind(bind.name, bind.data_uint32_t);
            break;
        }
        case ENGINE_UI_DOCUMENT_DATA_TYPE_STRING:
        {
            constructor.Bind(bind.name, &bind.data_string->str);
            break;
        }
        case ENGINE_UI_DOCUMENT_DATA_TYPE_EVENT_CALLBACK:
        {
            constructor.BindEventCallback(bind.name, [this, bind](Rml::DataModelHandle data_model, Rml::Event& ev, const Rml::VariantList& args)
                {
                    assert(args.empty());
                    auto handle = reinterpret_cast<engine_ui_data_handle_t>(this);
                    auto event = convet_rml_event_to_engine_event(ev);
                    bind.data_callback.fn_ptr(handle, &event, bind.data_callback.user_data);
                });
            break;
        }
        default:
            log::log(log::LogLevel::eError, "Unknown engine data type. Cant create data binding for UI.");
        }
    }
    handle_ = new Rml::DataModelHandle(constructor.GetModelHandle());
}

engine::UiDataHandle::UiDataHandle(UiDataHandle&& rhs)
{
    std::swap(context_, rhs.context_);
    std::swap(handle_, rhs.handle_);
    std::swap(name_, rhs.name_);
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
        context_->RemoveDataModel(name_);
        delete handle_;
        handle_ = nullptr;
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

    Rml::ObserverPtr<Rml::Element> t;
    t.get();
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
    case ENGINE_UI_EVENT_TYPE_POINTER_OVER:
    {
        rml_ev_id = Rml::EventId::Mouseover;
        break;
    }
    case ENGINE_UI_EVENT_TYPE_POINTER_OUT:
    {
        rml_ev_id = Rml::EventId::Mouseout;
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
    const auto ev = convet_rml_event_to_engine_event(event);
    callback_(&ev, user_data_);
}
