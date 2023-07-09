#pragma once
#include <string>

namespace Rml
{
    class ElementDocument;
}

namespace engine
{
    class UiManager;
}

namespace engine
{
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