#pragma once
#include <string>
#include <span>

#include "engine.h"

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
public:
    UiElement(Rml::Element* element, engine_result_code_t& err_out);
    UiElement(const UiElement& rhs) = delete;
    UiElement& operator=(const UiElement& rhs) = delete;
    UiElement(UiElement&& rhs);
    UiElement& operator=(UiElement&& rhs);
    ~UiElement();

private:
    Rml::Element* element_ = nullptr;
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