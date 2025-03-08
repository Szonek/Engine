#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <functional>

namespace engine
{
enum class FileStatus
{
    eCreated,
    eModified,
    eDeleted,
};

class FileWatcher
{
public:
    FileWatcher(const std::filesystem::path& path_to_watch);
    void start(const std::function<void(std::string, FileStatus)>& action);
private:
    bool running_ = true;
    std::filesystem::path path_to_watch_;
    std::unordered_map<std::string, std::filesystem::file_time_type> paths_;  // map file_name -> last time was modified
};
}
