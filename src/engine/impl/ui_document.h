#pragma once
#include <string>
#include <span>
#include <map>
#include <functional>

#include "engine.h"

#include <RmlUi/Core.h>

namespace Rml
{
    class Context;
    class ElementDocument;
    class Element;
    class DataModelConstructor;
    class DataModelHandle;
}

namespace engine
{
    class UiManager;
    class UiDocument;
}


namespace engine
{

class UiDataHandle
{
public:
    UiDataHandle(Rml::Context* ctx, std::string_view name, std::span<const engine_ui_document_data_binding_t> bindings);
    UiDataHandle(const UiDataHandle& rhs) = delete;
    UiDataHandle& operator=(const UiDataHandle& rhs) = delete;
    UiDataHandle(UiDataHandle&&);
    UiDataHandle& operator=(UiDataHandle&& rhs);
    ~UiDataHandle();

    void dirty_all_variables();
    void dirty_variable(std::string_view name);

private:
    Rml::Context* context_ = nullptr;
    Rml::DataModelHandle* handle_ = nullptr;
    std::string name_{};
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
        void ProcessEvent(Rml::Event& event) override;

    private:
        engine_ui_event_t parse_rml_event_to_engine_event(const Rml::Event& event);

    private:
        fnCallbackT callback_;
        void* user_data_;
    };

public:
    UiElement() = default;
    UiElement(Rml::Element* element, engine_result_code_t& err_out);
    UiElement(const UiElement& rhs) = delete;
    UiElement& operator=(const UiElement& rhs) = delete;
    UiElement(UiElement&& rhs);
    UiElement& operator=(UiElement&& rhs);
    ~UiElement();

    // return true if overwriten previously existing callback
    bool register_callback(engine_ui_event_type_t type, void* user_data, fnCallbackT&& callback);

    bool set_property(std::string_view name, std::string_view value);
    void remove_property(std::string_view name);

private:
    Rml::Element* element_ = nullptr;
    const UiDocument* const parent_doc_ = nullptr;
    std::map<engine_ui_event_type_t, BasicEventListener> listeners_;
};

class UiDocument
{
public:
    UiDocument(Rml::Context* ctx, std::string_view file_name);
    UiDocument(const UiDocument& rhs) = delete;
    UiDocument& operator=(const UiDocument& rhs) = delete;
    UiDocument(UiDocument&&);
    UiDocument& operator=(UiDocument&& rhs);
    ~UiDocument();

    void show();
    void hide();

    UiElement* get_element_by_id(std::string_view id, engine_result_code_t& err_out);

private:
    Rml::ElementDocument* doc_ = nullptr;
    Rml::Context* context_;
    std::map<std::string, UiElement> cached_ui_elements_;
};
}