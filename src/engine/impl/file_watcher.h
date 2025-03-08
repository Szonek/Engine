#pragma once
#include "logger.h"

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

    void register_callback(const std::filesystem::path& path, const std::function<void()>& callback);
    void unregister_callback(const std::filesystem::path& path);

private:
    void start();
    void stop();

private:
    FileWatcher()
        : background_thread_(&FileWatcher::start, this)
    {
    }

    ~FileWatcher()
    {
        stop();
        background_thread_.join();
    }

private:
    struct FileInfo
    {
        std::function<void()> callback;
        std::filesystem::file_time_type last_write_time;
    };

private:
    std::thread background_thread_;
    std::mutex mutex_;
    bool running_ = true;
    std::unordered_map<std::filesystem::path, FileInfo> files_;  // map file_name -> last time was modified
};

}
