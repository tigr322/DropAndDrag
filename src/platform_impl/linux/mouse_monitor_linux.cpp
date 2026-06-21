#include "platform/mouse_monitor/mouse_monitor.hpp"
#include <core/mouse_shake/mouse_shake_detector.hpp>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace dd {

namespace {

MouseShakeDetector* g_detector  = nullptr;
std::atomic<bool>   g_running{false};
int                 g_mouse_fd  = -1;
int                 g_virt_x    = 0;
int                 g_virt_y    = 0;

} // namespace

bool start_mouse_monitor(MouseShakeDetector& detector) {
    g_detector = &detector;
    g_detector->set_mouse_button_down(true);

    // /dev/input/mice aggregates all pointing devices and works on both
    // native X11 and Wayland sessions (XQueryPointer is stale on XWayland).
    g_mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
    if (g_mouse_fd < 0) {
        fprintf(stderr,
            "[shake] Cannot open /dev/input/mice (errno=%d).\n"
            "  Fix: sudo usermod -aG input $USER  then log out and back in.\n",
            errno);
        // Return true anyway — tick will use XQueryPointer fallback.
    }

    g_running.store(true, std::memory_order_release);
    return true;
}

void stop_mouse_monitor() {
    g_running.store(false, std::memory_order_release);
    if (g_mouse_fd >= 0) {
        close(g_mouse_fd);
        g_mouse_fd = -1;
    }
    g_detector = nullptr;
}

bool is_mouse_monitor_running() {
    return g_running.load(std::memory_order_acquire);
}

void set_shelf_visible(bool visible) {
    if (!visible && g_detector) {
        // Reset accumulated shake state so the next gesture starts clean,
        // then re-enable immediately (button always stays "held" on Linux).
        g_detector->set_mouse_button_down(false);
        g_detector->set_mouse_button_down(true);
    }
}

void tick_mouse_monitor(int fallback_x, int fallback_y) {
    if (!g_detector || !g_running.load(std::memory_order_acquire)) return;

    if (g_mouse_fd >= 0) {
        // PS/2 packet: [buttons, rel_x, rel_y] — 3 bytes, signed rel deltas.
        unsigned char pkt[3];
        bool got_any = false;
        while (true) {
            ssize_t n = read(g_mouse_fd, pkt, 3);
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            if (n != 3) break;
            g_virt_x += (signed char)pkt[1];
            g_virt_y -= (signed char)pkt[2]; // Y inverted in PS/2
            got_any = true;
            g_detector->on_mouse_move(g_virt_x, g_virt_y);
        }
        (void)got_any;
    } else {
        // Fallback to XQueryPointer (works on native X11, stale on XWayland).
        g_detector->on_mouse_move(fallback_x, fallback_y);
    }
}

} // namespace dd
