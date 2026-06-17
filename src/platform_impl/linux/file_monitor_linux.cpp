#include "platform/fs_monitor/fs_monitor.hpp"

#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace dd {

FileSystemMonitor& FileSystemMonitor::instance() {
    static FileSystemMonitor monitor;
    return monitor;
}

FileSystemMonitor::~FileSystemMonitor() {
    stop_watching();
}

void FileSystemMonitor::start_watching(std::string_view path) {
    if (is_watching_) return;

    watched_dir_ = path;
    inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd_ < 0) return;

    uint32_t mask = IN_CREATE | IN_DELETE | IN_DELETE_SELF |
                    IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO |
                    IN_CLOSE_WRITE | IN_ATTRIB;
    wd_ = inotify_add_watch(inotify_fd_, watched_dir_.c_str(), mask);
    if (wd_ < 0) {
        close(inotify_fd_);
        inotify_fd_ = -1;
        return;
    }

    is_watching_ = true;
    watch_thread_ = std::jthread([this](std::stop_token stoken) {
        watch_loop(stoken);
    });
}

void FileSystemMonitor::stop_watching() {
    if (!is_watching_) return;

    is_watching_ = false;

    if (watch_thread_.joinable()) {
        watch_thread_.request_stop();
        watch_thread_.join();
    }

    if (inotify_fd_ >= 0) {
        if (wd_ >= 0) inotify_rm_watch(inotify_fd_, wd_);
        close(inotify_fd_);
        inotify_fd_ = -1;
        wd_ = -1;
    }
}

void FileSystemMonitor::watch_loop(std::stop_token stoken) {
    constexpr size_t BUF_SIZE = 4096 * sizeof(inotify_event);
    alignas(inotify_event) char buffer[BUF_SIZE];

    struct timespec timeout{.tv_sec = 0, .tv_nsec = 200000000};

    using clock = std::chrono::steady_clock;
    std::map<std::string, clock::time_point> pending_events;

    while (!stoken.stop_requested()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(inotify_fd_, &readfds);

        int ret = pselect(inotify_fd_ + 1, &readfds, nullptr, nullptr, &timeout, nullptr);
        if (ret <= 0) {
            process_debounced(pending_events);
            continue;
        }

        ssize_t length = read(inotify_fd_, buffer, BUF_SIZE);
        if (length <= 0) continue;

        auto now = clock::now();
        ssize_t i = 0;
        while (i < length) {
            auto* event = reinterpret_cast<inotify_event*>(&buffer[i]);
            if (event->len > 0) {
                std::string name(event->name);

                if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                    pending_events[name] = now;
                    pending_event_types_[name] = static_cast<uint32_t>(FileEventType::Added);
                } else if (event->mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF)) {
                    pending_events[name] = now;
                    pending_event_types_[name] = static_cast<uint32_t>(FileEventType::Removed);
                } else if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE | IN_ATTRIB)) {
                    pending_events[name] = now;
                    pending_event_types_[name] = static_cast<uint32_t>(FileEventType::Modified);
                }
            }
            i += sizeof(inotify_event) + event->len;
        }

        process_debounced(pending_events);
    }
}

void FileSystemMonitor::process_debounced(std::map<std::string, std::chrono::steady_clock::time_point>& events) {
    using clock = std::chrono::steady_clock;
    auto now = clock::now();
    auto threshold = std::chrono::milliseconds(500);

    auto it = events.begin();
    while (it != events.end()) {
        if (now - it->second >= threshold) {
            std::string path = it->first;
            auto type = static_cast<FileEventType>(pending_event_types_[path]);

            if (on_file_added_ && type == FileEventType::Added) on_file_added_(path);
            else if (on_file_removed_ && type == FileEventType::Removed) on_file_removed_(path);
            else if (on_file_modified_ && type == FileEventType::Modified) on_file_modified_(path);

            pending_event_types_.erase(path);
            it = events.erase(it);
        } else {
            ++it;
        }
    }
}

void FileSystemMonitor::add_path(std::string_view path) {
}

void FileSystemMonitor::remove_path(std::string_view path) {
}

void FileSystemMonitor::set_on_added(FileEventCallback callback) {
    on_file_added_ = std::move(callback);
}

void FileSystemMonitor::set_on_removed(FileEventCallback callback) {
    on_file_removed_ = std::move(callback);
}

void FileSystemMonitor::set_on_modified(FileEventCallback callback) {
    on_file_modified_ = std::move(callback);
}

void FileSystemMonitor::set_on_renamed(FileEventCallback callback) {
    on_file_renamed_ = std::move(callback);
}

} // namespace dd
