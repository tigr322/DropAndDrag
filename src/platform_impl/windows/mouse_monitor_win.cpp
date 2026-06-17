#include "platform/mouse_monitor/mouse_monitor.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>

namespace dd {

namespace {

std::atomic<bool> g_running{false};
HHOOK g_mouse_hook = nullptr;
MouseShakeDetector* g_detector = nullptr;

LRESULT CALLBACK low_level_mouse_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || !g_detector || !g_running.load(std::memory_order_relaxed)) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    auto* hs = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

    switch (wParam) {
        case WM_MOUSEMOVE: {
            g_detector->on_mouse_move(hs->pt.x, hs->pt.y);

            bool left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            bool right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            bool middle = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
            g_detector->set_mouse_button_down(left || right || middle);
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            g_detector->set_mouse_button_down(true);
            break;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            g_detector->set_mouse_button_down(false);
            break;
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

} // namespace

bool start_mouse_monitor(MouseShakeDetector& detector) {
    if (g_running.load(std::memory_order_acquire)) return true;

    g_detector = &detector;

    g_mouse_hook = SetWindowsHookExW(
        WH_MOUSE_LL,
        low_level_mouse_proc,
        GetModuleHandleW(nullptr),
        0
    );

    if (!g_mouse_hook) {
        g_detector = nullptr;
        return false;
    }

    g_running.store(true, std::memory_order_release);
    return true;
}

void stop_mouse_monitor() {
    g_running.store(false, std::memory_order_release);

    if (g_mouse_hook) {
        UnhookWindowsHookEx(g_mouse_hook);
        g_mouse_hook = nullptr;
    }

    g_detector = nullptr;
}

bool is_mouse_monitor_running() {
    return g_running.load(std::memory_order_acquire);
}

} // namespace dd
