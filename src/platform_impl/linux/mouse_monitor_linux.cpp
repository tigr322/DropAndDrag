#include "platform/mouse_monitor/mouse_monitor.hpp"
#include <core/mouse_shake/mouse_shake_detector.hpp>

#include <X11/Xlib.h>

#include <atomic>
#include <cstdio>
#include <thread>

namespace dd {

namespace {

std::atomic<bool> g_running{false};
Display*          g_display = nullptr;
Window            g_root    = 0;
MouseShakeDetector* g_detector = nullptr;
std::thread       g_poll_thread;

void poll_loop() {
    int last_x = -1, last_y = -1;
    int ev_count = 0;

    while (g_running.load(std::memory_order_acquire)) {
        Window root_ret, child_ret;
        int rx, ry, wx, wy;
        unsigned int state;

        if (XQueryPointer(g_display, g_root, &root_ret, &child_ret,
                          &rx, &ry, &wx, &wy, &state)) {
            if (rx != last_x || ry != last_y) {
                ++ev_count;
                last_x = rx;
                last_y = ry;
                if (ev_count <= 5 || ev_count % 200 == 0)
                    fprintf(stderr, "[shake] poll#%d pos=%d,%d\n",
                            ev_count, rx, ry);
                g_detector->on_mouse_move(rx, ry);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

} // namespace

bool start_mouse_monitor(MouseShakeDetector& detector) {
    if (g_running.load(std::memory_order_acquire)) return true;

    g_display = XOpenDisplay(nullptr);
    if (!g_display) return false;

    g_root     = DefaultRootWindow(g_display);
    g_detector = &detector;

    // Button state is unreliable on Wayland/XDnD — always treat as held so
    // the shake fires on movement alone, without requiring a drag.
    g_detector->set_mouse_button_down(true);

    g_running.store(true, std::memory_order_release);
    g_poll_thread = std::thread(poll_loop);

    return true;
}

void stop_mouse_monitor() {
    g_running.store(false, std::memory_order_release);

    if (g_poll_thread.joinable())
        g_poll_thread.join();

    if (g_display) {
        XCloseDisplay(g_display);
        g_display = nullptr;
    }

    g_detector = nullptr;
}

bool is_mouse_monitor_running() {
    return g_running.load(std::memory_order_acquire);
}

void set_shelf_visible(bool visible) {
    if (!visible && g_detector)
        g_detector->set_mouse_button_down(false);
}

} // namespace dd
