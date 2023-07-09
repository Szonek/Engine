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
