#pragma once
#include <string>
#include <span>
#include <map>
#include <functional>

#include "engine.h"

#include <RmlUi/Core.h>

namespace Rml
{
    class ElementDocument;
    class Element;
    class DataModelConstructor;
    class DataModelHandle;
}

namespace engine
{
    class UiManager;
}


namespace engine
{

class UiDataHandle
{
public:
    UiDataHandle(Rml::DataModelConstructor* constructor, std::span<const engine_ui_document_data_binding_t> bindings);
    UiDataHandle(const UiDataHandle& rhs) = delete;
    UiDataHandle& operator=(const UiDataHandle& rhs) = delete;
    UiDataHandle(UiDataHandle&&);
    UiDataHandle& operator=(UiDataHandle&& rhs);
    ~UiDataHandle();

    void dirty_all_variables();
    void dirty_variable(std::string_view name);

private:
    Rml::DataModelHandle* handle_ = nullptr;
};

class UiElement
{
private:
    using fnCallbackT = std::function<void(const engine_ui_event_t*, void*)>;

    class BasicEventListener : public Rml::EventListener {
    public:
        BasicEventListener() = default;
        BasicEventListener(fnCallbackT&& callback, void* user_data)
            : callback_(std::move(callback))
            , user_data_(user_data)
        {

        }
    protected:
        void ProcessEvent(Rml::Event& event) override
        {
            const auto ev = parse_rml_event_to_engine_event(event);
            callback_(&ev, user_data_);
        }

    private:
        engine_ui_event_t parse_rml_event_to_engine_event(const Rml::Event& event);

    private:
        fnCallbackT callback_;
        void* user_data_;
    };

public:
    UiElement(Rml::Element* element, engine_result_code_t& err_out);
    UiElement(const UiElement& rhs) = delete;
    UiElement& operator=(const UiElement& rhs) = delete;
    UiElement(UiElement&& rhs);
    UiElement& operator=(UiElement&& rhs);
    ~UiElement();

    // return true if overwriten previously existing callback
    bool register_callback(engine_ui_event_type_t type, void* user_data, fnCallbackT&& callback);

private:
    Rml::Element* element_ = nullptr;

    std::map<engine_ui_event_type_t, BasicEventListener> listeners_;
};

class UiDocument
{
public:
    UiDocument(Rml::ElementDocument* doc);
    UiDocument(const UiDocument& rhs) = delete;
    UiDocument& operator=(const UiDocument& rhs) = delete;
    UiDocument(UiDocument&&);
    UiDocument& operator=(UiDocument&& rhs);
    ~UiDocument();

    void show();
    void hide();

    UiElement get_element_by_id(std::string_view id, engine_result_code_t& err_out);

private:
    UiManager* ui_mng_ = nullptr;
    Rml::ElementDocument* doc_ = nullptr;
};
}