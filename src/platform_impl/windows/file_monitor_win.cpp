#include "platform/fs_monitor/fs_monitor.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace dd {
namespace win {
namespace {

std::wstring toWide(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

std::string toUtf8(std::wstring_view s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len, nullptr, nullptr);
    return result;
}

constexpr size_t kBufSize = 64 * 1024;
constexpr DWORD kNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                 FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;

FileChangeType mapAction(DWORD action) {
    switch (action) {
    case FILE_ACTION_ADDED:            return FileChangeType::Added;
    case FILE_ACTION_REMOVED:          return FileChangeType::Removed;
    case FILE_ACTION_MODIFIED:         return FileChangeType::Modified;
    case FILE_ACTION_RENAMED_OLD_NAME: return FileChangeType::Renamed;
    case FILE_ACTION_RENAMED_NEW_NAME: return FileChangeType::Renamed;
    default:                           return FileChangeType::Modified;
    }
}

} // namespace

class WinFileMonitor {
public:
    WinFileMonitor() = default;
    ~WinFileMonitor() { stop(); }

    void start(std::string_view path, FileChangeCallback cb, std::chrono::milliseconds debounce) {
        stop();
        callback_ = std::move(cb);
        debounce_ms_ = debounce;
        dir_path_ = toWide(path);
        running_.store(true);
        worker_ = std::thread(&WinFileMonitor::run, this);
    }

    void stop() {
        running_.store(false);
        wakeup();
        if (worker_.joinable()) {
            worker_.join();
        }
        if (dir_handle_ != INVALID_HANDLE_VALUE) {
            CancelIoEx(dir_handle_, &overlapped_);
            CloseHandle(dir_handle_);
            dir_handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    void wakeup() {
        if (dir_handle_ != INVALID_HANDLE_VALUE) {
            CancelIoEx(dir_handle_, &overlapped_);
        }
        cv_.notify_all();
    }

    void run() {
        dir_handle_ = CreateFileW(dir_path_.c_str(), FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                   nullptr);
        if (dir_handle_ == INVALID_HANDLE_VALUE) return;

        overlapped_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        alignas(DWORD) uint8_t buffer[kBufSize];

        std::vector<FileChangeEvent> pending;
        auto last_flush = std::chrono::steady_clock::now();

        while (running_.load()) {
            DWORD bytes_returned = 0;
            BOOL ok = ReadDirectoryChangesW(dir_handle_, buffer, kBufSize, TRUE,
                                            kNotifyFilter, &bytes_returned,
                                            &overlapped_, nullptr);
            if (!ok) break;

            DWORD wait_result = WaitForSingleObject(overlapped_.hEvent, 500);
            if (wait_result == WAIT_TIMEOUT) {
                std::lock_guard lock(mutex_);
                auto now = std::chrono::steady_clock::now();
                if (!pending.empty() && (now - last_flush) >= debounce_ms_) {
                    flushPending(pending);
                    last_flush = now;
                }
                continue;
            }
            if (wait_result != WAIT_OBJECT_0) continue;

            DWORD result_bytes = 0;
            if (!GetOverlappedResult(dir_handle_, &overlapped_, &result_bytes, FALSE))
                continue;

            ResetEvent(overlapped_.hEvent);

            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
            for (;;) {
                std::wstring wfname{info->FileName, info->FileNameLength / sizeof(wchar_t)};
                auto fname = toUtf8(wfname);

                FileChangeEvent evt;
                evt.type = mapAction(info->Action);
                evt.path = toUtf8(dir_path_) + "\\" + fname;
                evt.timestamp = std::chrono::steady_clock::now();

                if (evt.type == FileChangeType::Renamed && info->Action == FILE_ACTION_RENAMED_OLD_NAME) {
                    rename_old_path_ = evt.path;
                } else if (evt.type == FileChangeType::Renamed && info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                    evt.old_path = rename_old_path_;
                    rename_old_path_.clear();
                }

                {
                    std::lock_guard lock(mutex_);
                    pending.push_back(std::move(evt));
                }

                if (info->NextEntryOffset == 0) break;
                info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<uint8_t*>(info) + info->NextEntryOffset);
            }

            std::lock_guard lock(mutex_);
            auto now = std::chrono::steady_clock::now();
            if (!pending.empty() && (now - last_flush) >= debounce_ms_) {
                flushPending(pending);
                last_flush = now;
            }
        }

        {
            std::lock_guard lock(mutex_);
            if (!pending.empty()) {
                flushPending(pending);
            }
        }

        CloseHandle(overlapped_.hEvent);
        overlapped_.hEvent = nullptr;
        CloseHandle(dir_handle_);
        dir_handle_ = INVALID_HANDLE_VALUE;
    }

    void flushPending(std::vector<FileChangeEvent>& pending) {
        if (pending.empty() || !callback_) return;
        auto evts = std::move(pending);
        pending.clear();
        callback_(evts);
    }

    std::atomic<bool> running_{false};
    std::thread worker_;
    std::wstring dir_path_;
    HANDLE dir_handle_ = INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped_{};
    FileChangeCallback callback_;
    std::chrono::milliseconds debounce_ms_{500};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::string rename_old_path_;
};

static WinFileMonitor g_monitor;

} // namespace win

FileSystemMonitor& FileSystemMonitor::instance() {
    static FileSystemMonitor mgr;
    return mgr;
}

void FileSystemMonitor::startWatching(std::string_view path) {
    watching_ = true;
    stopWatching();
    win::g_monitor.start(path, callback_, debounce_interval_);
}

void FileSystemMonitor::stopWatching() {
    win::g_monitor.stop();
    watching_ = false;
}

void FileSystemMonitor::addPath(std::string_view /*path*/) {
}

void FileSystemMonitor::removePath(std::string_view /*path*/) {
}

void FileSystemMonitor::setCallback(FileChangeCallback cb) {
    callback_ = std::move(cb);
}

void FileSystemMonitor::setDebounceInterval(std::chrono::milliseconds interval) {
    debounce_interval_ = interval;
}

} // namespace dd
