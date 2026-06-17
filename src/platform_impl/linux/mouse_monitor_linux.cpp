#include "platform/mouse_monitor/mouse_monitor.hpp"

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>

#include <atomic>
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
    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);

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

                switch (event.xcookie.evtype) {
                    case XI_RawMotion: {
                        auto* me = reinterpret_cast<XIRawEvent*>(event.xcookie.data);
                        int x = 0, y = 0;
                        double* vals = me->raw_values;
                        for (int i = 0; i < me->valuators.mask_len * 8; ++i) {
                            if (XIMaskIsSet(me->valuators.mask, i)) {
                                if (i == 0) x = static_cast<int>(*vals);
                                if (i == 1) y = static_cast<int>(*vals);
                                vals++;
                            }
                        }
                        g_detector->on_mouse_move(x, y);

                        Window root, child;
                        int rx, ry, wx, wy;
                        unsigned int mask;
                        if (XQueryPointer(g_display, g_root, &root, &child,
                                         &rx, &ry, &wx, &wy, &mask)) {
                            bool btn = (mask & (Button1Mask | Button2Mask |
                                                Button3Mask | Button4Mask |
                                                Button5Mask)) != 0;
                            g_detector->set_mouse_button_down(btn);
                        }
                        break;
                    }
                    case XI_ButtonPress:
                        g_detector->set_mouse_button_down(true);
                        break;
                    case XI_ButtonRelease: {
                        Window root, child;
                        int rx, ry, wx, wy;
                        unsigned int mask;
                        if (XQueryPointer(g_display, g_root, &root, &child,
                                         &rx, &ry, &wx, &wy, &mask)) {
                            bool any = (mask & (Button1Mask | Button2Mask |
                                                Button3Mask | Button4Mask |
                                                Button5Mask)) != 0;
                            g_detector->set_mouse_button_down(any);
                        }
                        break;
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

} // namespace dd
