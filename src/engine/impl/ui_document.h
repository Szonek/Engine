#pragma once
#include <string>
#include <span>

#include "engine.h"

namespace Rml
{
    class ElementDocument;
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

private:
    UiManager* ui_mng_ = nullptr;
    Rml::ElementDocument* doc_ = nullptr;
};
}