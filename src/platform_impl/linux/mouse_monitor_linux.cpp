#include "platform/mouse_monitor/mouse_monitor.hpp"
#include <core/mouse_shake/mouse_shake_detector.hpp>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#if defined(HAVE_XRECORD)
#  include <X11/Xlib.h>
#  include <X11/extensions/record.h>
#endif

#if defined(HAVE_WL_RELATIVE_POINTER)
#  include <wayland-client.h>
#  include <poll.h>
#  include <cstring>
#  include "relative-pointer-unstable-v1.h"
#endif

#if defined(HAVE_XRECORD) || defined(HAVE_WL_RELATIVE_POINTER)
#  include <thread>
#endif

namespace dd {
namespace {

MouseShakeDetector* g_detector = nullptr;
std::atomic<bool>   g_running{false};
int                 g_mouse_fd = -1;
int                 g_virt_x   = 0;
int                 g_virt_y   = 0;

#if defined(HAVE_XRECORD)
Display*          g_rec_ctrl = nullptr;
Display*          g_rec_data = nullptr;
XRecordContext    g_rec_ctx  = 0;
std::thread       g_rec_thread;

// Latest absolute root-window cursor position from the XRecord thread.
// Written by the record thread; read by tick_mouse_monitor on the main thread.
std::atomic<int32_t> g_rec_x{0};
std::atomic<int32_t> g_rec_y{0};
std::atomic<int32_t> g_rec_gen{0};  // increments with each MotionNotify; 0 = none yet

// Previous position used to compute delta in tick_mouse_monitor (main thread only).
int  g_rec_prev_x     = 0;
int  g_rec_prev_y     = 0;
int  g_rec_last_gen   = 0;
bool g_rec_initialized = false;

static void record_intercept(XPointer, XRecordInterceptData* data) {
    if (data->category != XRecordFromServer || data->data_len < 32) {
        XRecordFreeData(data);
        return;
    }
    const auto* d = reinterpret_cast<const uint8_t*>(data->data);
    if ((d[0] & 0x7Fu) == MotionNotify) {
        // X11 MotionNotify wire format (little-endian, offset 20-23 = root_x, root_y).
        auto rx = static_cast<int16_t>(static_cast<uint16_t>(d[20]) | (static_cast<uint16_t>(d[21]) << 8));
        auto ry = static_cast<int16_t>(static_cast<uint16_t>(d[22]) | (static_cast<uint16_t>(d[23]) << 8));
        g_rec_x.store(rx, std::memory_order_relaxed);
        g_rec_y.store(ry, std::memory_order_relaxed);
        g_rec_gen.fetch_add(1, std::memory_order_release);
    }
    XRecordFreeData(data);
}

static void record_thread_func() {
    // Blocks until XRecordDisableContext is called from the control connection.
    XRecordEnableContext(g_rec_data, g_rec_ctx, record_intercept, nullptr);
}
#endif

#if defined(HAVE_WL_RELATIVE_POINTER)
// Wayland relative pointer — zwp_relative_pointer_manager_v1 broadcasts relative
// motion to ALL registered clients regardless of which surface has pointer focus.
// This works when cursor is over Nautilus (GTK4/Wayland) where X11 cannot help.

struct WlRelPtrState {
    wl_display*  display = nullptr;
    wl_registry* registry = nullptr;
    wl_seat*     seat    = nullptr;
    wl_pointer*  pointer = nullptr;
    zwp_relative_pointer_manager_v1* mgr = nullptr;
    zwp_relative_pointer_v1*         rel = nullptr;
} g_wl;

std::thread          g_wl_thread;
std::atomic<int32_t> g_wl_dx{0};
std::atomic<int32_t> g_wl_dy{0};
std::atomic<int32_t> g_wl_gen{0};

static void on_rel_motion(void*, zwp_relative_pointer_v1*,
    uint32_t, uint32_t,
    wl_fixed_t /*dx*/, wl_fixed_t /*dy*/,
    wl_fixed_t dx_u, wl_fixed_t dy_u) {
    g_wl_dx.fetch_add(wl_fixed_to_int(dx_u), std::memory_order_relaxed);
    g_wl_dy.fetch_add(wl_fixed_to_int(dy_u), std::memory_order_relaxed);
    g_wl_gen.fetch_add(1, std::memory_order_release);
}

static const zwp_relative_pointer_v1_listener s_rel_listener = { on_rel_motion };

static void on_registry_global(void*, wl_registry* reg, uint32_t name,
    const char* iface, uint32_t) {
    if (std::strcmp(iface, wl_seat_interface.name) == 0 && !g_wl.seat)
        g_wl.seat = static_cast<wl_seat*>(
            wl_registry_bind(reg, name, &wl_seat_interface, 1));
    else if (std::strcmp(iface, zwp_relative_pointer_manager_v1_interface.name) == 0 && !g_wl.mgr)
        g_wl.mgr = static_cast<zwp_relative_pointer_manager_v1*>(
            wl_registry_bind(reg, name, &zwp_relative_pointer_manager_v1_interface, 1));
}
static void on_registry_remove(void*, wl_registry*, uint32_t) {}
static const wl_registry_listener s_reg_listener = { on_registry_global, on_registry_remove };
// Zero-init pointer listener: libwayland skips null function pointers safely.
static const wl_pointer_listener s_ptr_listener = {};

static void wl_thread_func() {
    g_wl.display = wl_display_connect(nullptr);
    if (!g_wl.display) {
        fprintf(stderr, "[shake] Wayland display connect failed\n");
        return;
    }
    g_wl.registry = wl_display_get_registry(g_wl.display);
    wl_registry_add_listener(g_wl.registry, &s_reg_listener, nullptr);
    wl_display_roundtrip(g_wl.display);

    if (!g_wl.seat || !g_wl.mgr) {
        fprintf(stderr, "[shake] Wayland: wl_seat or zwp_relative_pointer_manager_v1 not found\n");
        wl_display_disconnect(g_wl.display);
        g_wl.display = nullptr;
        return;
    }

    g_wl.pointer = wl_seat_get_pointer(g_wl.seat);
    wl_pointer_add_listener(g_wl.pointer, &s_ptr_listener, nullptr);
    g_wl.rel = zwp_relative_pointer_manager_v1_get_relative_pointer(g_wl.mgr, g_wl.pointer);
    zwp_relative_pointer_v1_add_listener(g_wl.rel, &s_rel_listener, nullptr);
    wl_display_roundtrip(g_wl.display);

    fprintf(stderr, "[shake] Wayland relative pointer active — global tracking enabled\n");

    struct pollfd pfd = { wl_display_get_fd(g_wl.display), POLLIN, 0 };
    while (g_running.load(std::memory_order_acquire)) {
        while (wl_display_prepare_read(g_wl.display) != 0)
            wl_display_dispatch_pending(g_wl.display);
        wl_display_flush(g_wl.display);
        poll(&pfd, 1, 50);
        if (pfd.revents & POLLIN) {
            wl_display_read_events(g_wl.display);
            wl_display_dispatch_pending(g_wl.display);
        } else {
            wl_display_cancel_read(g_wl.display);
        }
    }

    zwp_relative_pointer_v1_destroy(g_wl.rel);     g_wl.rel = nullptr;
    wl_pointer_destroy(g_wl.pointer);               g_wl.pointer = nullptr;
    zwp_relative_pointer_manager_v1_destroy(g_wl.mgr); g_wl.mgr = nullptr;
    wl_seat_destroy(g_wl.seat);                     g_wl.seat = nullptr;
    wl_registry_destroy(g_wl.registry);             g_wl.registry = nullptr;
    wl_display_disconnect(g_wl.display);            g_wl.display = nullptr;
}
#endif

} // namespace

bool start_mouse_monitor(MouseShakeDetector& detector) {
    g_detector = &detector;

#if defined(HAVE_XRECORD)
    g_rec_ctrl = XOpenDisplay(nullptr);
    g_rec_data = XOpenDisplay(nullptr);
    if (g_rec_ctrl && g_rec_data) {
        int maj = 0, min = 0;
        if (XRecordQueryVersion(g_rec_ctrl, &maj, &min)) {
            XRecordRange* rng = XRecordAllocRange();
            rng->device_events.first = MotionNotify;
            rng->device_events.last  = MotionNotify;
            XRecordClientSpec spec = XRecordAllClients;
            g_rec_ctx = XRecordCreateContext(g_rec_ctrl, 0, &spec, 1, &rng, 1);
            XFree(rng);
            if (g_rec_ctx) {
                XSync(g_rec_ctrl, False);
                g_rec_thread = std::thread(record_thread_func);
                fprintf(stderr, "[shake] XRecord v%d.%d active\n", maj, min);
            }
        }
    }
    if (!g_rec_ctx) {
        fprintf(stderr, "[shake] XRecord unavailable; falling back to XQueryPointer\n");
        if (g_rec_data) { XCloseDisplay(g_rec_data); g_rec_data = nullptr; }
        if (g_rec_ctrl) { XCloseDisplay(g_rec_ctrl); g_rec_ctrl = nullptr; }
    }
#endif

    // Set running before starting threads so their while-loop guard sees true.
    g_running.store(true, std::memory_order_release);

#if defined(HAVE_WL_RELATIVE_POINTER)
    g_wl_thread = std::thread(wl_thread_func);
#endif

    // PS/2 compatibility layer — works on non-Wayland setups or if the compositor
    // does not grab devices exclusively.  Open non-blocking; EAGAIN = no events.
    g_mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
    if (g_mouse_fd < 0)
        fprintf(stderr, "[shake] /dev/input/mice unavailable (errno=%d)\n", errno);

    return true;
}

void stop_mouse_monitor() {
    g_running.store(false, std::memory_order_release);

#if defined(HAVE_XRECORD)
    if (g_rec_ctx && g_rec_ctrl) {
        XRecordDisableContext(g_rec_ctrl, g_rec_ctx);
        XSync(g_rec_ctrl, False);
        if (g_rec_thread.joinable()) g_rec_thread.join();
        XRecordFreeContext(g_rec_ctrl, g_rec_ctx);
        g_rec_ctx = 0;
    }
    if (g_rec_data) { XCloseDisplay(g_rec_data); g_rec_data = nullptr; }
    if (g_rec_ctrl) { XCloseDisplay(g_rec_ctrl); g_rec_ctrl = nullptr; }
    g_rec_initialized = false;
    g_rec_last_gen    = 0;
    g_rec_prev_x      = 0;
    g_rec_prev_y      = 0;
#endif

#if defined(HAVE_WL_RELATIVE_POINTER)
    if (g_wl_thread.joinable()) g_wl_thread.join();
#endif

    if (g_mouse_fd >= 0) { close(g_mouse_fd); g_mouse_fd = -1; }
    g_detector = nullptr;
}

bool is_mouse_monitor_running() {
    return g_running.load(std::memory_order_acquire);
}

void set_shelf_visible(bool visible) {
    if (!visible && g_detector)
        g_detector->set_mouse_button_down(false);
}

void tick_mouse_monitor(int fallback_x, int fallback_y) {
    if (!g_detector || !g_running.load(std::memory_order_acquire)) return;

    // On Linux/XWayland, button state from XQueryPointer is unreliable when
    // the cursor is over Wayland-native windows (the compositor owns those events).
    // Always enable shake detection so the shelf can be shown with a shake gesture.
    g_detector->set_mouse_button_down(true);

    bool got_any = false;

#if defined(HAVE_WL_RELATIVE_POINTER)
    // Priority 1: Wayland relative motion — works even when cursor is over
    // Wayland-native windows like Nautilus.  Uses unaccelerated deltas.
    if (int gen = g_wl_gen.exchange(0, std::memory_order_acq_rel); gen > 0) {
        g_virt_x += g_wl_dx.exchange(0, std::memory_order_relaxed);
        g_virt_y += g_wl_dy.exchange(0, std::memory_order_relaxed);
        got_any = true;
    }
#endif

#if defined(HAVE_XRECORD)
    {
        int cur_gen = g_rec_gen.load(std::memory_order_acquire);
        if (cur_gen != g_rec_last_gen) {
            g_rec_last_gen = cur_gen;
            int rx = g_rec_x.load(std::memory_order_relaxed);
            int ry = g_rec_y.load(std::memory_order_relaxed);
            if (g_rec_initialized) {
                g_virt_x += rx - g_rec_prev_x;
                g_virt_y += ry - g_rec_prev_y;
            } else {
                g_virt_x = rx;
                g_virt_y = ry;
                g_rec_initialized = true;
            }
            g_rec_prev_x = rx;
            g_rec_prev_y = ry;
            got_any = true;
        }
    }
#endif

    if (!got_any && g_mouse_fd >= 0) {
        unsigned char pkt[3];
        while (true) {
            ssize_t n = read(g_mouse_fd, pkt, 3);
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            if (n != 3) break;
            g_virt_x += static_cast<signed char>(pkt[1]);
            g_virt_y -= static_cast<signed char>(pkt[2]);
            got_any = true;
        }
    }

    if (!got_any) {
        g_virt_x = fallback_x;
        g_virt_y = fallback_y;
    }

    g_detector->on_mouse_move(g_virt_x, g_virt_y);
}

} // namespace dd
