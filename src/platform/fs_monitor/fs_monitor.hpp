#pragma once

// fs_monitor.hpp — FileSystemMonitor abstract interface.
// Platform implementations: FSEvents (macOS), ReadDirectoryChangesW (Win), inotify (Linux).


#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

enum class FileChangeType : uint8_t {
    Added,
    Removed,
    Modified,
    Renamed,
};

struct FileChangeEvent {
    FileChangeType type;
    std::string path;
    std::string old_path;
    std::chrono::steady_clock::time_point timestamp;
};

using FileChangeCallback = std::function<void(std::vector<FileChangeEvent> events)>;

class FileSystemMonitor {
public:
    static FileSystemMonitor& instance();

    FileSystemMonitor(const FileSystemMonitor&) = delete;
    FileSystemMonitor& operator=(const FileSystemMonitor&) = delete;
    FileSystemMonitor(FileSystemMonitor&&) = delete;
    FileSystemMonitor& operator=(FileSystemMonitor&&) = delete;

    void startWatching(std::string_view path);
    void stopWatching();
    void addPath(std::string_view path);
    void removePath(std::string_view path);

    void setCallback(FileChangeCallback cb);
    void setDebounceInterval(std::chrono::milliseconds interval);

private:
    FileSystemMonitor() = default;
    ~FileSystemMonitor() = default;

    FileChangeCallback callback_;
    std::chrono::milliseconds debounce_interval_{100};
    bool watching_{false};
};

} // namespace dd
