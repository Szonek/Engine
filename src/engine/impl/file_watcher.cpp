#include "file_watcher.h"
#include <chrono>
#include <thread>

bool engine::FileWatcher::register_callback(const std::filesystem::path& path, const std::function<void()>& callback)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (files_.contains(path))
    {
        log::log(log::LogLevel::eError, std::format("Path: {} already exists in watcher!\n", path.string()));
        return false;
    }
    // a file which can't be found (yet) gets a time when it shows up, which counts as a change
    std::error_code error;
    files_[path] = FileInfo{ callback, std::filesystem::last_write_time(path, error) };
    return true;
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
    if (background_thread_.joinable())
    {
        return;
    }
    running_ = true;
    background_thread_ = std::thread(&FileWatcher::run, this);
}

void engine::FileWatcher::run()
{
    while (running_)
    {
        constexpr const auto delay = std::chrono::milliseconds{ 150 };
        std::this_thread::sleep_for(delay);

        std::unique_lock<std::mutex> lock(mutex_);
        for (auto& [path, file_info] : files_)
        {
            // a file which is missing for now (deleted, renamed, or replaced by an editor's atomic save) is checked
            // again on the next tick
            std::error_code error;
            const auto file_last_write_time = std::filesystem::last_write_time(path, error);
            if (!error && file_info.last_write_time != file_last_write_time)
            {
                file_info.last_write_time = file_last_write_time;
                file_info.callback();
            }
        }
    }
}