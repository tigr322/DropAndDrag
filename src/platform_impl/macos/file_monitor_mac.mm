// file_monitor_mac.mm — macOS FileSystemMonitor via FSEvents.
//
// FSEvents delivers change notifications at the directory level with a latency
// of ~0.5s (configurable).  The callback is dispatched on a background serial
// queue and must not touch UI directly — callers should dispatch to main.
//
// FSEventStreamCreate uses kFSEventStreamCreateFlagFileEvents so individual
// file-level changes (create, modify, delete, rename) are reported separately.

#include "platform/fs_monitor/fs_monitor.hpp"

#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

namespace dd {
namespace {

struct FSEventContext {
    FileChangeCallback callback;
    std::chrono::milliseconds debounceInterval{100};
    std::chrono::steady_clock::time_point lastFlush;
    std::vector<FileChangeEvent> pendingEvents;
    std::mutex mutex;
};

FSEventStreamRef gEventStream = nullptr;
FSEventContext *gContext = nullptr;
std::set<std::string> gWatchedPaths;
std::mutex gPathsMutex;

void fsEventCallback(ConstFSEventStreamRef /*streamRef*/,
                     void *clientCallBackInfo,
                     size_t numEvents,
                     void *eventPaths,
                     const FSEventStreamEventFlags eventFlags[],
                     const FSEventStreamEventId /*eventIds*/[]) {
    auto *ctx = static_cast<FSEventContext *>(clientCallBackInfo);
    if (!ctx) return;

    char **paths = static_cast<char **>(eventPaths);

    std::lock_guard<std::mutex> lock(ctx->mutex);

    auto now = std::chrono::steady_clock::now();
    bool shouldFlush = (now - ctx->lastFlush) >= ctx->debounceInterval;

    for (size_t i = 0; i < numEvents; ++i) {
        FileChangeEvent evt;
        evt.path = std::string(paths[i]);
        evt.timestamp = now;

        FSEventStreamEventFlags flags = eventFlags[i];

        bool isCreated = (flags & kFSEventStreamEventFlagItemCreated) != 0;
        bool isRemoved = (flags & kFSEventStreamEventFlagItemRemoved) != 0;
        bool isModified = (flags & kFSEventStreamEventFlagItemModified) != 0;
        bool isRenamed = (flags & kFSEventStreamEventFlagItemRenamed) != 0;
        bool isDir = (flags & kFSEventStreamEventFlagItemIsDir) != 0;
        // bool isFile = (flags & kFSEventStreamEventFlagItemIsFile) != 0;

        // Skip directory-level events that don't represent real changes
        if (isDir && !isCreated && !isRemoved && !isRenamed) {
            continue;
        }

        if (isRenamed) {
            evt.type = FileChangeType::Renamed;
        } else if (isRemoved) {
            evt.type = FileChangeType::Removed;
        } else if (isCreated) {
            evt.type = FileChangeType::Added;
        } else if (isModified) {
            evt.type = FileChangeType::Modified;
        } else {
            // Generic change detected
            if (flags & kFSEventStreamEventFlagMustScanSubDirs) {
                evt.type = FileChangeType::Modified;
            } else {
                evt.type = FileChangeType::Modified;
            }
        }

        ctx->pendingEvents.push_back(std::move(evt));
    }

    ctx->lastFlush = now;

    if (shouldFlush && !ctx->pendingEvents.empty()) {
        std::vector<FileChangeEvent> events;
        events.swap(ctx->pendingEvents);

        if (ctx->callback) {
            ctx->callback(std::move(events));
        }
    }
}

} // anonymous namespace

FileSystemMonitor& FileSystemMonitor::instance() {
    static FileSystemMonitor instance;
    return instance;
}

void FileSystemMonitor::startWatching(std::string_view path) {
    @autoreleasepool {
        stopWatching();

        std::string pathStr{path};

        gContext = new FSEventContext();
        gContext->debounceInterval = debounce_interval_;

        CFStringRef cfPath = CFStringCreateWithCString(
            kCFAllocatorDefault, pathStr.c_str(), kCFStringEncodingUTF8);
        CFArrayRef pathsToWatch = CFArrayCreate(
            kCFAllocatorDefault, (const void **)&cfPath, 1, &kCFTypeArrayCallBacks);

        FSEventStreamContext streamCtx = {};
        streamCtx.version = 0;
        streamCtx.info = gContext;
        streamCtx.retain = nullptr;
        streamCtx.release = nullptr;
        streamCtx.copyDescription = nullptr;

        gEventStream = FSEventStreamCreate(
            kCFAllocatorDefault,
            &fsEventCallback,
            &streamCtx,
            pathsToWatch,
            kFSEventStreamEventIdSinceNow,
            debounce_interval_.count() / 1000.0,
            kFSEventStreamCreateFlagFileEvents
                | kFSEventStreamCreateFlagNoDefer
                | kFSEventStreamCreateFlagWatchRoot);

        if (gEventStream) {
            FSEventStreamScheduleWithRunLoop(
                gEventStream,
                CFRunLoopGetCurrent(),
                kCFRunLoopDefaultMode);

            FSEventStreamStart(gEventStream);

            {
                std::lock_guard<std::mutex> lock(gPathsMutex);
                gWatchedPaths.insert(pathStr);
            }

            watching_ = true;
        }

        CFRelease(pathsToWatch);
        CFRelease(cfPath);
    }
}

void FileSystemMonitor::stopWatching() {
    @autoreleasepool {
        if (gEventStream) {
            FSEventStreamStop(gEventStream);
            FSEventStreamUnscheduleFromRunLoop(
                gEventStream,
                CFRunLoopGetCurrent(),
                kCFRunLoopDefaultMode);
            FSEventStreamInvalidate(gEventStream);
            FSEventStreamRelease(gEventStream);
            gEventStream = nullptr;
        }

        if (gContext) {
            delete gContext;
            gContext = nullptr;
        }

        {
            std::lock_guard<std::mutex> lock(gPathsMutex);
            gWatchedPaths.clear();
        }

        watching_ = false;
    }
}

void FileSystemMonitor::addPath(std::string_view path) {
    @autoreleasepool {
        std::string pathStr{path};

        {
            std::lock_guard<std::mutex> lock(gPathsMutex);
            if (gWatchedPaths.count(pathStr) > 0) return;
            gWatchedPaths.insert(pathStr);
        }

        // Rebuild stream with updated paths
        std::vector<CFStringRef> cfPaths;
        {
            std::lock_guard<std::mutex> lock(gPathsMutex);
            for (const auto& p : gWatchedPaths) {
                cfPaths.push_back(CFStringCreateWithCString(
                    kCFAllocatorDefault, p.c_str(), kCFStringEncodingUTF8));
            }
        }

        if (gEventStream) {
            FSEventStreamStop(gEventStream);
            FSEventStreamUnscheduleFromRunLoop(
                gEventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
            FSEventStreamInvalidate(gEventStream);
            FSEventStreamRelease(gEventStream);
            gEventStream = nullptr;
        }

        CFArrayRef pathsToWatch = CFArrayCreate(
            kCFAllocatorDefault,
            (const void **)cfPaths.data(),
            (CFIndex)cfPaths.size(),
            &kCFTypeArrayCallBacks);

        FSEventStreamContext streamCtx = {};
        streamCtx.info = gContext;

        gEventStream = FSEventStreamCreate(
            kCFAllocatorDefault,
            &fsEventCallback,
            &streamCtx,
            pathsToWatch,
            kFSEventStreamEventIdSinceNow,
            debounce_interval_.count() / 1000.0,
            kFSEventStreamCreateFlagFileEvents
                | kFSEventStreamCreateFlagNoDefer
                | kFSEventStreamCreateFlagWatchRoot);

        if (gEventStream) {
            FSEventStreamScheduleWithRunLoop(
                gEventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
            FSEventStreamStart(gEventStream);
        }

        CFRelease(pathsToWatch);
        for (auto cfPath : cfPaths) {
            CFRelease(cfPath);
        }
    }
}

void FileSystemMonitor::removePath(std::string_view path) {
    @autoreleasepool {
        std::string pathStr{path};

        {
            std::lock_guard<std::mutex> lock(gPathsMutex);
            gWatchedPaths.erase(pathStr);
            if (gWatchedPaths.empty()) {
                stopWatching();
                return;
            }
        }

        // Rebuild stream without the removed path
        std::vector<CFStringRef> cfPaths;
        {
            std::lock_guard<std::mutex> lock(gPathsMutex);
            for (const auto& p : gWatchedPaths) {
                cfPaths.push_back(CFStringCreateWithCString(
                    kCFAllocatorDefault, p.c_str(), kCFStringEncodingUTF8));
            }
        }

        if (gEventStream) {
            FSEventStreamStop(gEventStream);
            FSEventStreamUnscheduleFromRunLoop(
                gEventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
            FSEventStreamInvalidate(gEventStream);
            FSEventStreamRelease(gEventStream);
            gEventStream = nullptr;
        }

        CFArrayRef pathsToWatch = CFArrayCreate(
            kCFAllocatorDefault,
            (const void **)cfPaths.data(),
            (CFIndex)cfPaths.size(),
            &kCFTypeArrayCallBacks);

        FSEventStreamContext streamCtx = {};
        streamCtx.info = gContext;

        gEventStream = FSEventStreamCreate(
            kCFAllocatorDefault,
            &fsEventCallback,
            &streamCtx,
            pathsToWatch,
            kFSEventStreamEventIdSinceNow,
            debounce_interval_.count() / 1000.0,
            kFSEventStreamCreateFlagFileEvents
                | kFSEventStreamCreateFlagNoDefer
                | kFSEventStreamCreateFlagWatchRoot);

        if (gEventStream) {
            FSEventStreamScheduleWithRunLoop(
                gEventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
            FSEventStreamStart(gEventStream);
        }

        CFRelease(pathsToWatch);
        for (auto cfPath : cfPaths) {
            CFRelease(cfPath);
        }
    }
}

void FileSystemMonitor::setCallback(FileChangeCallback cb) {
    callback_ = std::move(cb);
    if (gContext) {
        std::lock_guard<std::mutex> lock(gContext->mutex);
        gContext->callback = callback_;
    }
}

void FileSystemMonitor::setDebounceInterval(std::chrono::milliseconds interval) {
    debounce_interval_ = interval;
    if (gContext) {
        std::lock_guard<std::mutex> lock(gContext->mutex);
        gContext->debounceInterval = interval;
    }
}

} // namespace dd
