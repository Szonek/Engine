#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace engine
{
// the whole file, nullopt (logged, `what` names the reader) when it can't be read
std::optional<std::string> read_text_file(const std::filesystem::path& path, std::string_view what);

// Writes `content` to `path` atomically: into a temporary file next to it, then renamed over it, so a crash or a full
// disk never leaves half a file (the old one stays). False (logged, `what` names the writer) when it can't be written;
// the temporary file is removed then.
bool write_file_atomic(const std::filesystem::path& path, std::string_view content, std::string_view what);
}  // namespace engine
