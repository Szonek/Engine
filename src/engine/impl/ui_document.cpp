#include "ui_document.h"
#include "ui_manager.h"
#include "asset_store.h"
#include "engine.h"
#include "logger.h"
#include "math_helpers.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ID.h>
#include <RmlUi/Core/DataModelHandle.h>

engine::UiDocument::UiDocument(Rml::ElementDocument* doc)
    : doc_(doc)
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
    // ToDo: release document!!
}

void engine::UiDocument::show()
{
    doc_->Show();
}

void engine::UiDocument::hide()
{
    doc_->Hide();
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
