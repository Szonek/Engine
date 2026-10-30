#pragma once
#include "logger.h"

#include <atomic>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <functional>
#include <format>
#include <chrono>
#include <thread>
#include <mutex>

namespace engine
{

class FileWatcher
{ 
public:

public:
    static FileWatcher& get_instance()
    {
        static FileWatcher instance;
        return instance;
    }
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

    // files can be registered whether or not the watcher runs; their callbacks are only called while it does. False
    // (logged) when the path is watched already: only the first callback is called, so only its owner unregisters it.
    bool register_callback(const std::filesystem::path& path, const std::function<void()>& callback);
    void unregister_callback(const std::filesystem::path& path);

    // starts the background thread checking the registered files (off by default: only the editor needs it)
    void start();

private:
    void run();

private:
    FileWatcher() = default;

    ~FileWatcher()
    {
        running_ = false;
        if (background_thread_.joinable())
        {
            background_thread_.join();
        }
    }

private:
    struct FileInfo
    {
        std::function<void()> callback;
        std::filesystem::file_time_type last_write_time;
    };

private:
    std::mutex mutex_;
    std::atomic<bool> running_ = false;
    std::unordered_map<std::filesystem::path, FileInfo> files_;  // map file_name -> last time was modified
    std::thread background_thread_;  // last: the thread uses the members above
};

}
