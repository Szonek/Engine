#include "file_io.h"

#include "logger.h"

#include <fmt/format.h>

#include <fstream>
#include <iterator>
#include <system_error>

std::optional<std::string> engine::read_text_file(const std::filesystem::path& path, std::string_view what)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        log::log(log::LogLevel::eError, fmt::format("{}: can't read {}.\n", what, path.string()));
        return std::nullopt;
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool engine::write_file_atomic(const std::filesystem::path& path, std::string_view content, std::string_view what)
{
    auto temp_path = path;
    temp_path += ".tmp";
    {
        std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
        const bool written = file && file.write(content.data(), static_cast<std::streamsize>(content.size())) && file.flush();
        if (!written)
        {
            file.close();
            std::error_code ignored;
            std::filesystem::remove(temp_path, ignored);
            log::log(log::LogLevel::eError, fmt::format("{}: can't write {}.\n", what, path.string()));
            return false;
        }
    }
    // replaces the old file in one step (MoveFileEx with MOVEFILE_REPLACE_EXISTING on Windows, rename on POSIX)
    std::error_code error;
    std::filesystem::rename(temp_path, path, error);
    if (error)
    {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        log::log(log::LogLevel::eError, fmt::format("{}: can't write {} ({}).\n", what, path.string(), error.message()));
        return false;
    }
    return true;
}
