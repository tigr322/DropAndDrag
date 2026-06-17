#include "mouse_shake_detector.hpp"

#include <algorithm>
#include <cstdlib>

namespace dd {

MouseShakeDetector::MouseShakeDetector(ShakeConfig config)
    : config_(config)
    , last_change_time_(std::chrono::steady_clock::now())
    , last_trigger_time_(std::chrono::steady_clock::now())
{
}

MouseShakeDetector::~MouseShakeDetector() = default;

void MouseShakeDetector::set_callback(ShakeCallback cb) {
    callback_ = std::move(cb);
}

void MouseShakeDetector::set_config(const ShakeConfig& config) {
    config_ = config;
    reset_state();
}

void MouseShakeDetector::set_enabled(bool enabled) {
    config_.enabled = enabled;
    if (!enabled) reset_state();
}

void MouseShakeDetector::on_mouse_move(int x, int y) {
    if (!config_.enabled || !callback_ || !mouse_button_down_) return;

    if (!has_last_position_) {
        last_x_ = x;
        last_y_ = y;
        has_last_position_ = true;
        return;
    }

    int dx = x - last_x_;
    int dy = y - last_y_;
    last_x_ = x;
    last_y_ = y;

    bool is_positive = (std::abs(dx) >= std::abs(dy)) ? (dx > 0) : (dy > 0);
    int abs_delta = std::max(std::abs(dx), std::abs(dy));

    if (abs_delta < config_.dead_zone_px) return;

    auto now = std::chrono::steady_clock::now();

    if (now - last_trigger_time_ < std::chrono::milliseconds(800)) {
        return;
    }

    if (is_positive != last_direction_positive_) {
        if (now - last_change_time_ < config_.time_window) {
            direction_changes_++;
            if (direction_changes_ >= config_.direction_changes) {
                last_trigger_time_ = now;
                callback_();
                reset_state();
                return;
            }
        } else {
            direction_changes_ = 1;
        }
        last_direction_positive_ = is_positive;
        last_change_time_ = now;
    }
}

void MouseShakeDetector::set_mouse_button_down(bool down) {
    if (!down && mouse_button_down_) {
        reset_state();
    }
    mouse_button_down_ = down;
}

void MouseShakeDetector::reset_state() {
    direction_changes_ = 0;
    last_direction_positive_ = true;
}

} // namespace dd
