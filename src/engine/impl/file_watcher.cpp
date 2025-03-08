#include "file_watcher.h"
#include <chrono>
#include <thread>

engine::FileWatcher::FileWatcher(const std::filesystem::path& path_to_watch)
    : path_to_watch_(path_to_watch)
{
}

void engine::FileWatcher::start(const std::function<void(std::string, FileStatus)>& action)
{
    while (running_)
    {
        constexpr const auto delay = std::chrono::milliseconds{ 100 };
        std::this_thread::sleep_for(delay);
        
        // file deletion
        auto it = paths_.begin();
        while (it != paths_.end())
        {
            if (!std::filesystem::exists(it->first))
            {
                action(it->first, FileStatus::eDeleted);
                it = paths_.erase(it);
            }
            else
            {
                it++;
            }
        }

        // file creation or modification
        for (const auto& file : std::filesystem::directory_iterator(path_to_watch_))
        {
            const auto file_last_write_time = std::filesystem::last_write_time(file);

            // creation
            const auto file_path = file.path().string();
            if (!paths_.contains(file_path))
            {
                paths_[file_path] = file_last_write_time;
                action(file_path, FileStatus::eCreated);
            }
            else // modified
            {
                if (paths_.at(file_path) != file_last_write_time)
                {
                    paths_[file_path] = file_last_write_time;
                    action(file_path, FileStatus::eModified);
                }
            }
        }
    }
}
