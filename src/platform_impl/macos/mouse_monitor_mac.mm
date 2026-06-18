#include "platform/mouse_monitor/mouse_monitor.hpp"
#include <core/mouse_shake/mouse_shake_detector.hpp>

#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace dd {

namespace {

std::atomic<bool> g_running{false};
std::atomic<bool> g_shelf_visible{false};
CFMachPortRef g_event_tap = nullptr;
CFRunLoopSourceRef g_run_loop_source = nullptr;

CGEventRef event_callback(CGEventTapProxy /*proxy*/, CGEventType type,
                          CGEventRef event, void* user_info) {
    auto* detector = static_cast<MouseShakeDetector*>(user_info);
    if (!detector || !g_running.load(std::memory_order_relaxed)) {
        return event;
    }

    if (g_shelf_visible.load(std::memory_order_relaxed)) return event;

    switch (type) {
        case kCGEventMouseMoved:
        case kCGEventLeftMouseDragged:
        case kCGEventRightMouseDragged:
        case kCGEventOtherMouseDragged: {
            CGPoint point = CGEventGetLocation(event);
            detector->on_mouse_move(static_cast<int>(point.x),
                                    static_cast<int>(point.y));
            break;
        }
        case kCGEventLeftMouseDown:
        case kCGEventRightMouseDown:
        case kCGEventOtherMouseDown:
            detector->set_mouse_button_down(true);
            break;
        case kCGEventLeftMouseUp:
        case kCGEventRightMouseUp:
        case kCGEventOtherMouseUp:
            detector->set_mouse_button_down(false);
            break;
        default:
            break;
    }

    return event;
}

} // namespace

bool start_mouse_monitor(MouseShakeDetector& detector) {
    if (g_running.load(std::memory_order_acquire)) return true;

    CGEventMask mask = CGEventMaskBit(kCGEventMouseMoved) |
                       CGEventMaskBit(kCGEventLeftMouseDragged) |
                       CGEventMaskBit(kCGEventRightMouseDragged) |
                       CGEventMaskBit(kCGEventOtherMouseDragged) |
                       CGEventMaskBit(kCGEventLeftMouseDown) |
                       CGEventMaskBit(kCGEventLeftMouseUp) |
                       CGEventMaskBit(kCGEventRightMouseDown) |
                       CGEventMaskBit(kCGEventRightMouseUp);

    g_event_tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        mask,
        event_callback,
        &detector
    );

    if (!g_event_tap) {
        return false;
    }

    g_run_loop_source = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, g_event_tap, 0);

    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       g_run_loop_source,
                       kCFRunLoopCommonModes);

    CGEventTapEnable(g_event_tap, true);
    g_running.store(true, std::memory_order_release);

    return true;
}

void stop_mouse_monitor() {
    g_running.store(false, std::memory_order_release);

    if (g_event_tap) {
        CGEventTapEnable(g_event_tap, false);
        CFMachPortInvalidate(g_event_tap);
        CFRelease(g_event_tap);
        g_event_tap = nullptr;
    }

    if (g_run_loop_source) {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
                              g_run_loop_source,
                              kCFRunLoopCommonModes);
        CFRelease(g_run_loop_source);
        g_run_loop_source = nullptr;
    }
}

bool is_mouse_monitor_running() {
    return g_running.load(std::memory_order_acquire);
}

void set_shelf_visible(bool visible) {
    g_shelf_visible.store(visible, std::memory_order_release);
}

} // namespace dd
