#pragma once

#include "native_window.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace dd {

enum class ScreenEdge : uint8_t {
    Top,
    Bottom,
    Left,
    Right,
};

struct MonitorInfo {
    Rect bounds;
    Rect work_area;
    bool is_primary = false;
    float dpi_scale = 1.0f;
};

enum class ShelfAnimationState : uint8_t {
    Idle,
    Showing,
    Hiding,
};

class WindowManager {
public:
    static WindowManager& instance();

    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;
    WindowManager(WindowManager&&) = delete;
    WindowManager& operator=(WindowManager&&) = delete;

    void showShelf();
    void hideShelf();
    void toggleShelf();
    bool isShelfVisible() const;

    void setShelfPosition(const Rect& rect);
    Rect getShelfRect() const;

    void setScreenEdge(ScreenEdge edge);
    ScreenEdge getScreenEdge() const;

    void setAutoHideEnabled(bool enabled);
    bool isAutoHideEnabled() const;
    void setAutoHideDelay(std::chrono::milliseconds delay);

    std::vector<MonitorInfo> enumerateMonitors() const;
    MonitorInfo getCurrentMonitor() const;
    void positionAtScreenEdge(ScreenEdge edge);

    ShelfAnimationState getAnimationState() const;
    void setShelfOpacity(float opacity);

    float getShelfOpacity() const;

private:
    WindowManager();
    ~WindowManager();

    void startAutoHideTimer();
    void stopAutoHideTimer();
    void onAutoHideTimerTick();

    std::unique_ptr<NativeWindow> window_;
    ScreenEdge screen_edge_{ScreenEdge::Bottom};
    bool auto_hide_enabled_{true};
    std::chrono::milliseconds auto_hide_delay_{500};
    mutable std::mutex mutex_;

    struct AutoHideTimerImpl;
    std::unique_ptr<AutoHideTimerImpl> auto_hide_timer_;

    ShelfAnimationState animation_state_{ShelfAnimationState::Idle};
    float shelf_opacity_{1.0f};
    bool shelf_visible_{false};
};

} // namespace dd
