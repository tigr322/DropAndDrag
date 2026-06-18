// window_manager.cpp — WindowManager: tracks open NativeWindow instances,
// dispatches show/hide/focus, and handles z-order policy (always-on-top).

#include "window_manager.hpp"

#include <algorithm>
#include <thread>

namespace dd {

struct WindowManager::AutoHideTimerImpl {
    std::chrono::steady_clock::time_point last_activity;
    bool running = false;

    void reset() { last_activity = std::chrono::steady_clock::now(); }

    bool elapsed(std::chrono::milliseconds delay) const {
        return std::chrono::steady_clock::now() - last_activity >= delay;
    }
};

WindowManager& WindowManager::instance() {
    static WindowManager mgr;
    return mgr;
}

WindowManager::WindowManager()
    : auto_hide_timer_(std::make_unique<AutoHideTimerImpl>()) {
    window_ = NativeWindow::create(WindowStyle::Transparent);
}

WindowManager::~WindowManager() {
    stopAutoHideTimer();
}

void WindowManager::showShelf() {
    std::lock_guard lock(mutex_);
    if (!window_) return;

    animation_state_ = ShelfAnimationState::Showing;
    window_->setVisible(true);
    window_->show();
    shelf_visible_ = true;
    animation_state_ = ShelfAnimationState::Idle;

    if (auto_hide_enabled_) {
        startAutoHideTimer();
    }
}

void WindowManager::hideShelf() {
    std::lock_guard lock(mutex_);
    if (!window_) return;

    animation_state_ = ShelfAnimationState::Hiding;
    stopAutoHideTimer();
    window_->hide();
    shelf_visible_ = false;
    animation_state_ = ShelfAnimationState::Idle;
}

void WindowManager::toggleShelf() {
    if (isShelfVisible()) {
        hideShelf();
    } else {
        showShelf();
    }
}

bool WindowManager::isShelfVisible() const {
    std::lock_guard lock(mutex_);
    return shelf_visible_;
}

void WindowManager::setShelfPosition(const Rect& rect) {
    std::lock_guard lock(mutex_);
    if (!window_) return;
    window_->setBounds(rect.x, rect.y, rect.width, rect.height);
}

Rect WindowManager::getShelfRect() const {
    std::lock_guard lock(mutex_);
    if (!window_) return {};
    return window_->getBounds();
}

void WindowManager::setScreenEdge(ScreenEdge edge) {
    std::lock_guard lock(mutex_);
    screen_edge_ = edge;
    positionAtScreenEdge(edge);
}

ScreenEdge WindowManager::getScreenEdge() const {
    std::lock_guard lock(mutex_);
    return screen_edge_;
}

void WindowManager::setAutoHideEnabled(bool enabled) {
    std::lock_guard lock(mutex_);
    auto_hide_enabled_ = enabled;
    if (!enabled) {
        stopAutoHideTimer();
    } else if (shelf_visible_) {
        startAutoHideTimer();
    }
}

bool WindowManager::isAutoHideEnabled() const {
    std::lock_guard lock(mutex_);
    return auto_hide_enabled_;
}

void WindowManager::setAutoHideDelay(std::chrono::milliseconds delay) {
    std::lock_guard lock(mutex_);
    auto_hide_delay_ = delay;
}

std::vector<MonitorInfo> WindowManager::enumerateMonitors() const {
    return {};
}

MonitorInfo WindowManager::getCurrentMonitor() const {
    return {};
}

void WindowManager::positionAtScreenEdge(ScreenEdge /*edge*/) {
    auto monitor = getCurrentMonitor();
    if (monitor.work_area.width == 0 || monitor.work_area.height == 0) return;

    Rect shelf_rect = window_ ? window_->getBounds() : Rect{};

    switch (screen_edge_) {
    case ScreenEdge::Bottom:
        shelf_rect.x = monitor.work_area.x;
        shelf_rect.y = monitor.work_area.y + monitor.work_area.height - shelf_rect.height;
        shelf_rect.width = monitor.work_area.width;
        break;
    case ScreenEdge::Top:
        shelf_rect.x = monitor.work_area.x;
        shelf_rect.y = monitor.work_area.y;
        shelf_rect.width = monitor.work_area.width;
        break;
    case ScreenEdge::Left:
        shelf_rect.x = monitor.work_area.x;
        shelf_rect.y = monitor.work_area.y;
        shelf_rect.height = monitor.work_area.height;
        break;
    case ScreenEdge::Right:
        shelf_rect.x = monitor.work_area.x + monitor.work_area.width - shelf_rect.width;
        shelf_rect.y = monitor.work_area.y;
        shelf_rect.height = monitor.work_area.height;
        break;
    }

    if (window_) {
        window_->setBounds(shelf_rect.x, shelf_rect.y, shelf_rect.width, shelf_rect.height);
    }
}

ShelfAnimationState WindowManager::getAnimationState() const {
    std::lock_guard lock(mutex_);
    return animation_state_;
}

void WindowManager::setShelfOpacity(float opacity) {
    std::lock_guard lock(mutex_);
    opacity = std::clamp(opacity, 0.0f, 1.0f);
    shelf_opacity_ = opacity;
    if (window_) {
        window_->setTransparency(opacity);
    }
}

float WindowManager::getShelfOpacity() const {
    std::lock_guard lock(mutex_);
    return shelf_opacity_;
}

void WindowManager::startAutoHideTimer() {
    if (!auto_hide_timer_) return;
    auto_hide_timer_->running = true;
    auto_hide_timer_->reset();
}

void WindowManager::stopAutoHideTimer() {
    if (!auto_hide_timer_) return;
    auto_hide_timer_->running = false;
}

void WindowManager::onAutoHideTimerTick() {
    if (!auto_hide_timer_ || !auto_hide_timer_->running) return;
    if (!auto_hide_timer_->elapsed(auto_hide_delay_)) return;

    std::lock_guard lock(mutex_);
    Rect shelf_rect = window_ ? window_->getBounds() : Rect{};

    (void)shelf_rect;
}

} // namespace dd
