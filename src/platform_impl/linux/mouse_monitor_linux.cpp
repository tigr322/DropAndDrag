#include "platform/mouse_monitor/mouse_monitor.hpp"
#include <core/mouse_shake/mouse_shake_detector.hpp>

#include <atomic>

namespace dd {

namespace {
MouseShakeDetector* g_detector = nullptr;
std::atomic<bool>   g_running{false};
} // namespace

bool start_mouse_monitor(MouseShakeDetector& detector) {
    g_detector = &detector;
    // Button state detection is unreliable on Wayland/XDnD.
    // Shake fires on movement alone; the main loop feeds positions.
    g_detector->set_mouse_button_down(true);
    g_running.store(true, std::memory_order_release);
    return true;
}

void stop_mouse_monitor() {
    g_running.store(false, std::memory_order_release);
    g_detector = nullptr;
}

bool is_mouse_monitor_running() {
    return g_running.load(std::memory_order_acquire);
}

void set_shelf_visible(bool visible) {
    if (!visible && g_detector)
        g_detector->set_mouse_button_down(false);
}

// Called each tick from Application::run_linux_loop with the screen
// pointer position queried via the window's own X connection.
void tick_mouse_monitor(int x, int y) {
    if (g_detector && g_running.load(std::memory_order_acquire))
        g_detector->on_mouse_move(x, y);
}

} // namespace dd
