// file_monitor_linux.cpp — Linux filesystem monitor via inotify.

#include "platform/fs_monitor/fs_monitor.hpp"

#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace dd {

namespace {

// Per-watch entry: fd returned by inotify_add_watch + the path it tracks.
struct WatchEntry {
    int wd;
    std::string path;
};

int                g_inotify_fd{-1};
std::vector<WatchEntry> g_watches;
std::jthread       g_watch_thread;

// Events that arrived but are waiting for the debounce window to close.
struct Pending {
    FileChangeEvent ev;
    std::chrono::steady_clock::time_point when;
};
std::map<std::string, Pending> g_pending;

void do_stop() {
    if (g_watch_thread.joinable()) {
        g_watch_thread.request_stop();
        g_watch_thread.join();
    }
    for (auto& w : g_watches) inotify_rm_watch(g_inotify_fd, w.wd);
    g_watches.clear();
    if (g_inotify_fd >= 0) { close(g_inotify_fd); g_inotify_fd = -1; }
    g_pending.clear();
}

void watch_loop(std::stop_token stop, FileChangeCallback cb,
                std::chrono::milliseconds debounce) {
    constexpr size_t kBuf = 4096 * (sizeof(inotify_event) + 16);
    alignas(inotify_event) char buf[kBuf];
    struct timespec ts{0, 100'000'000}; // 100 ms poll

    using clock = std::chrono::steady_clock;

    auto flush_debounced = [&]() {
        if (!cb) return;
        auto now = clock::now();
        std::vector<FileChangeEvent> ready;
        for (auto it = g_pending.begin(); it != g_pending.end(); ) {
            if (now - it->second.when >= debounce) {
                ready.push_back(std::move(it->second.ev));
                it = g_pending.erase(it);
            } else {
                ++it;
            }
        }
        if (!ready.empty()) cb(std::move(ready));
    };

    while (!stop.stop_requested()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_inotify_fd, &rfds);
        int ret = pselect(g_inotify_fd + 1, &rfds, nullptr, nullptr, &ts, nullptr);
        if (ret <= 0) { flush_debounced(); continue; }

        ssize_t len = read(g_inotify_fd, buf, kBuf);
        auto now = clock::now();

        for (ssize_t i = 0; i < len; ) {
            auto* ev = reinterpret_cast<inotify_event*>(&buf[i]);
            if (ev->len > 0) {
                std::string name(ev->name);
                FileChangeType type = FileChangeType::Modified;
                if (ev->mask & (IN_CREATE | IN_MOVED_TO))               type = FileChangeType::Added;
                else if (ev->mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF)) type = FileChangeType::Removed;
                else if (ev->mask & (IN_MODIFY | IN_CLOSE_WRITE | IN_ATTRIB))     type = FileChangeType::Modified;

                g_pending[name] = Pending{
                    FileChangeEvent{type, name, {}, now},
                    now
                };
            }
            i += static_cast<ssize_t>(sizeof(inotify_event)) + ev->len;
        }
        flush_debounced();
    }
    flush_debounced();
}

} // namespace

FileSystemMonitor& FileSystemMonitor::instance() {
    static FileSystemMonitor monitor;
    return monitor;
}

void FileSystemMonitor::startWatching(std::string_view path) {
    if (watching_) return;

    g_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (g_inotify_fd < 0) return;

    addPath(path);

    watching_ = true;
    auto cb = callback_;
    auto deb = debounce_interval_;
    g_watch_thread = std::jthread([cb, deb](std::stop_token stop) {
        watch_loop(stop, cb, deb);
    });
}

void FileSystemMonitor::stopWatching() {
    if (!watching_) return;
    do_stop();
    watching_ = false;
}

void FileSystemMonitor::addPath(std::string_view path) {
    if (g_inotify_fd < 0) return;
    uint32_t mask = IN_CREATE | IN_DELETE | IN_DELETE_SELF |
                    IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB;
    int wd = inotify_add_watch(g_inotify_fd, std::string(path).c_str(), mask);
    if (wd >= 0) g_watches.push_back({wd, std::string(path)});
}

void FileSystemMonitor::removePath(std::string_view path) {
    if (g_inotify_fd < 0) return;
    auto it = std::find_if(g_watches.begin(), g_watches.end(),
                           [&](const WatchEntry& w) { return w.path == path; });
    if (it != g_watches.end()) {
        inotify_rm_watch(g_inotify_fd, it->wd);
        g_watches.erase(it);
    }
}

void FileSystemMonitor::setCallback(FileChangeCallback cb) {
    callback_ = std::move(cb);
}

void FileSystemMonitor::setDebounceInterval(std::chrono::milliseconds interval) {
    debounce_interval_ = interval;
}

} // namespace dd
