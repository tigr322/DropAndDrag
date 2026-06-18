// mouse_monitor_mac.mm — macOS implementation of the global mouse monitor.
//
// Uses a CGEventTap at kCGSessionEventTap (session-level) so we see mouse
// events from ALL applications — required because the user drags files from
// Finder, browsers, etc. while the shelf is hidden.
//
// The event tap is attached to the MAIN run-loop (CFRunLoopGetCurrent()).
// It fires in CommonModes, which includes the default NSRunLoop mode used by
// native_loop_step(), so no extra thread is needed.
//
// Accessibility permission:
//   CGEventTapCreate returns NULL if the process doesn't have the Accessibility
//   entitlement (Privacy & Security → Accessibility).  start_mouse_monitor()
//   returns false in that case; Application logs a warning but continues —
//   the shelf is still usable via the tray icon.
//
// g_shelf_visible guard:
//   While the shelf is visible, the callback returns immediately without
//   feeding events to the detector.  This prevents the user's rapid shelf-drag
//   movements from triggering another shake.  CRITICAL: set_shelf_visible()
//   must be called whenever the shelf is shown or hidden; if it gets stuck at
//   true the detector will never fire again.  See ARCH_AUDIT.md §3.

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

// Whether the tap's run-loop source is active.
std::atomic<bool> g_running{false};

// True while the shelf window is visible.  Read in the event callback (main
// thread); written by set_shelf_visible() also on the main thread.
// relaxed ordering is safe — both accesses are on the same thread.
std::atomic<bool> g_shelf_visible{false};

CFMachPortRef      g_event_tap       = nullptr;
CFRunLoopSourceRef g_run_loop_source = nullptr;

// CGEventTap callback — fires on every mouse event matching the mask below.
// Runs synchronously on the thread whose run loop hosts the tap source
// (the main thread).  Must be fast; never blocks.
CGEventRef event_callback(CGEventTapProxy /*proxy*/, CGEventType type,
                          CGEventRef event, void* user_info) {
    auto* detector = static_cast<MouseShakeDetector*>(user_info);
    if (!detector || !g_running.load(std::memory_order_relaxed)) {
        return event;  // tap disabled or detector gone — pass event through
    }

    // Skip all detection while the shelf is visible.
    // Rapid movement = the user is dragging the shelf, not shaking to open it.
    if (g_shelf_visible.load(std::memory_order_relaxed)) return event;

    switch (type) {
        case kCGEventMouseMoved:
        case kCGEventLeftMouseDragged:
        case kCGEventRightMouseDragged:
        case kCGEventOtherMouseDragged: {
            // Feed screen coordinates (top-left origin, fractional pixels rounded)
            // into the shake algorithm.
            CGPoint point = CGEventGetLocation(event);
            detector->on_mouse_move(static_cast<int>(point.x),
                                    static_cast<int>(point.y));
            break;
        }
        case kCGEventLeftMouseDown:
        case kCGEventRightMouseDown:
        case kCGEventOtherMouseDown:
            // Shake detection only runs while a button is held (user dragging).
            detector->set_mouse_button_down(true);
            break;
        case kCGEventLeftMouseUp:
        case kCGEventRightMouseUp:
        case kCGEventOtherMouseUp:
            // Releasing the button resets accumulated shake state in the detector.
            detector->set_mouse_button_down(false);
            break;
        default:
            break;
    }

    return event;   // always pass the event through; we are passive observers
}

} // anonymous namespace

bool start_mouse_monitor(MouseShakeDetector& detector) {
    // Idempotent — safe to call if already running.
    if (g_running.load(std::memory_order_acquire)) return true;

    // Event mask covers moves (all drag flavours) and button state changes.
    // Scroll, key, and other events are excluded to minimise tap overhead.
    CGEventMask mask = CGEventMaskBit(kCGEventMouseMoved)        |
                       CGEventMaskBit(kCGEventLeftMouseDragged)  |
                       CGEventMaskBit(kCGEventRightMouseDragged) |
                       CGEventMaskBit(kCGEventOtherMouseDragged) |
                       CGEventMaskBit(kCGEventLeftMouseDown)     |
                       CGEventMaskBit(kCGEventLeftMouseUp)       |
                       CGEventMaskBit(kCGEventRightMouseDown)    |
                       CGEventMaskBit(kCGEventRightMouseUp);

    // kCGSessionEventTap — sees events from all processes in the login session.
    // kCGHeadInsertEventTap — placed before the event reaches the target app.
    // kCGEventTapOptionDefault — passive listener; does not modify events.
    g_event_tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        mask,
        event_callback,
        &detector   // passed as user_info to every callback invocation
    );

    if (!g_event_tap) {
        // NULL = Accessibility permission denied.
        return false;
    }

    // Wrap the Mach port in a CFRunLoopSource so the main run loop services it.
    g_run_loop_source = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, g_event_tap, 0);

    // Add to CommonModes — fires even during modal loops and drag tracking.
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

// Update the shelf-visibility guard.  Called from the main thread whenever
// the shelf is shown (true) or hidden (false).
void set_shelf_visible(bool visible) {
    g_shelf_visible.store(visible, std::memory_order_release);
}

} // namespace dd
