#include "file_watcher.h"
#include <chrono>
#include <thread>

void engine::FileWatcher::register_callback(const std::filesystem::path& path, const std::function<void()>& callback)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (files_.contains(path))
    {
        log::log(log::LogLevel::eError, std::format("Path: {} already exists in watcher!\n", path.string()));
        return;
    }
    files_[path] = FileInfo{ callback, std::filesystem::last_write_time(path) };
}


void engine::FileWatcher::unregister_callback(const std::filesystem::path& path)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (files_.contains(path))
    {
        files_.erase(path);
        return;
    }
    else
    {
        log::log(log::LogLevel::eError, std::format("Path: {} did not exists in watcher. Cant unregister it!\n", path.string()));
    }
}


void engine::FileWatcher::start()
{
    while (running_)
    {
        constexpr const auto delay = std::chrono::milliseconds{ 150 };
        std::this_thread::sleep_for(delay);

        std::unique_lock<std::mutex> lock(mutex_);
        for (auto& [path, file_info] : files_)
        {
            const auto file_last_write_time = std::filesystem::last_write_time(path);
            if (file_info.last_write_time != file_last_write_time)
            {
                file_info.last_write_time = file_last_write_time;
                file_info.callback();
            }
        }
    }
}

void engine::FileWatcher::stop()
{
    running_ = false;
}