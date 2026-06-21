#pragma once

// mouse_monitor.hpp — Platform-agnostic API for the global mouse hook.
//
// The implementation (mouse_monitor_mac.mm on macOS) installs a system-wide
// event tap that fires for every mouse-move and button event, regardless of
// which application is in front.  This is necessary because the user drags
// files from other apps onto the shelf.
//
// macOS requires Accessibility permission (Privacy & Security → Accessibility).
// start_mouse_monitor() returns false if permission is denied; the caller
// (Application::init_mouse_shake) treats this as a non-fatal warning.
//
// IMPORTANT — shelf visibility flag:
//   set_shelf_visible(true/false) must be called whenever the shelf window
//   is shown or hidden.  The event tap checks this flag and returns early
//   while the shelf is visible — preventing shake detection from firing when
//   the user drags the shelf window itself.  Failing to call this function
//   on hide will permanently disable shake detection.
//   See ARCH_AUDIT.md §3 for the full state machine.

namespace dd {

class MouseShakeDetector;

// Start the global mouse monitor and feed events into detector.
// Safe to call multiple times — returns true immediately if already running.
// macOS: creates a CGEventTap and adds it to the main run loop's CommonModes.
bool start_mouse_monitor(MouseShakeDetector& detector);

// Stop the monitor and release the event tap.  After this call, detector
// will receive no further events.
void stop_mouse_monitor();

// True if start_mouse_monitor() has been called and succeeded.
bool is_mouse_monitor_running();

// Update the shelf-visibility flag checked by the event tap callback.
// Must be called on the main thread (same thread as the event tap).
// Call with true when the shelf is shown, false when it is hidden.
void set_shelf_visible(bool visible);

#if defined(__linux__)
// Feed the current absolute screen cursor position into the shake detector.
// Called each main-loop tick from Application::run_linux_loop so the
// window's X connection (not a stale separate one) drives detection.
void tick_mouse_monitor(int x, int y);
#endif

} // namespace dd
