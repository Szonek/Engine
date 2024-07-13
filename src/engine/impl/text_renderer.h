#pragma once

#include <string_view>

namespace engine
{
class Font
{
public:
    Font();
    Font(std::string_view file_name);
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&&) noexcept;
    Font& operator=(Font&&) noexcept;
    ~Font();

private:
    void* ft_handle_;
};


}  // namespace engine