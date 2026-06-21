// mouse_monitor_linux.cpp — Linux mouse monitor stub (XInput2 implementation pending).

#include "platform/mouse_monitor/mouse_monitor.hpp"
#include <core/mouse_shake/mouse_shake_detector.hpp>

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

namespace dd {

namespace {

std::atomic<bool> g_running{false};
Display* g_display = nullptr;
Window g_root = 0;
MouseShakeDetector* g_detector = nullptr;
std::thread g_poll_thread;

int xi_opcode = 0;

bool init_xinput2() {
    g_display = XOpenDisplay(nullptr);
    if (!g_display) return false;

    int event_base, error_base;
    if (!XQueryExtension(g_display, "XInputExtension", &xi_opcode,
                         &event_base, &error_base)) {
        XCloseDisplay(g_display);
        g_display = nullptr;
        return false;
    }

    int major = 2, minor = 0;
    if (XIQueryVersion(g_display, &major, &minor) != Success) {
        XCloseDisplay(g_display);
        g_display = nullptr;
        return false;
    }

    g_root = DefaultRootWindow(g_display);

    XIEventMask mask{};
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = static_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_RawMotion);

    XISelectEvents(g_display, g_root, &mask, 1);
    free(mask.mask);
    XFlush(g_display);

    return true;
}

void poll_loop() {
    while (g_running.load(std::memory_order_acquire)) {
        while (XPending(g_display)) {
            XEvent event;
            XNextEvent(g_display, &event);

            if (event.xcookie.type == GenericEvent &&
                event.xcookie.extension == xi_opcode) {
                XGetEventData(g_display, &event.xcookie);

                if (event.xcookie.evtype == XI_RawMotion) {
                        static int s_ev_count = 0;
                        ++s_ev_count;
                        Window root_ret, child_ret;
                        int rx, ry, wx, wy;
                        unsigned int state;
                        if (XQueryPointer(g_display, g_root, &root_ret, &child_ret,
                                         &rx, &ry, &wx, &wy, &state)) {
                            if (s_ev_count <= 5 || s_ev_count % 200 == 0)
                                fprintf(stderr, "[shake] ev#%d pos=%d,%d\n",
                                        s_ev_count, rx, ry);
                            g_detector->on_mouse_move(rx, ry);
                        }
                    }

                XFreeEventData(g_display, &event.xcookie);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
}

} // namespace

bool start_mouse_monitor(MouseShakeDetector& detector) {
    if (g_running.load(std::memory_order_acquire)) return true;

    g_detector = &detector;
    // On Linux, XQueryPointer doesn't see button state from Wayland-native
    // drags (e.g. GTK4 Nautilus on GNOME/Wayland).  Treat button as always
    // held so shake detection fires on movement alone.
    g_detector->set_mouse_button_down(true);

    if (!init_xinput2()) return false;

    g_running.store(true, std::memory_order_release);
    g_poll_thread = std::thread(poll_loop);

    return true;
}

void stop_mouse_monitor() {
    g_running.store(false, std::memory_order_release);

    if (g_poll_thread.joinable()) {
        g_poll_thread.join();
    }

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
    // When the shelf becomes hidden, reset button-down state so a subsequent
    // shake while dragging is detected cleanly.
    if (!visible && g_detector) {
        g_detector->set_mouse_button_down(false);
    }
}

} // namespace dd
