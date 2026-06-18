// mouse_shake_detector.cpp — Shake gesture algorithm.
//
// Algorithm summary:
//   On every on_mouse_move() call (while a mouse button is held):
//   1. Compute the displacement vector (dx, dy) from the previous position.
//   2. Ignore movements smaller than dead_zone_px (eliminates cursor jitter).
//   3. Determine the dominant direction: positive (right/down) or negative
//      (left/up) based on whichever axis has the larger absolute delta.
//   4. If the direction reversed since the last sample, increment direction_changes_.
//      If the reversal happened outside the time window, reset to 1.
//   5. When direction_changes_ reaches config_.direction_changes AND we are
//      within the time window, fire the callback and reset state.
//   6. An 800ms cooldown after each trigger prevents accidental double-fires.
//
// The detector is platform-agnostic and purely algorithmic — no OS calls.
// The CGEventTap in mouse_monitor_mac.mm drives it with raw screen coordinates.

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
    reset_state();  // accumulated state may be invalid under new parameters
}

void MouseShakeDetector::set_enabled(bool enabled) {
    config_.enabled = enabled;
    if (!enabled) reset_state();
}

void MouseShakeDetector::on_mouse_move(int x, int y) {
    // Only detect shakes when enabled, a callback is registered, and the user
    // is actively holding a mouse button (i.e. dragging something).
    if (!config_.enabled || !callback_ || !mouse_button_down_) return;

    // Bootstrap: record the first position and wait for the next sample.
    if (!has_last_position_) {
        last_x_ = x;
        last_y_ = y;
        has_last_position_ = true;
        return;
    }

    const int dx = x - last_x_;
    const int dy = y - last_y_;
    last_x_ = x;
    last_y_ = y;

    // Determine dominant direction: use whichever axis moved more.
    // is_positive = true → moving right (|dx|≥|dy|) or down (|dy|>|dx|).
    const bool is_positive = (std::abs(dx) >= std::abs(dy)) ? (dx > 0) : (dy > 0);
    const int  abs_delta   = std::max(std::abs(dx), std::abs(dy));

    // Ignore micro-movements — cursor noise, sub-pixel rounding, etc.
    if (abs_delta < config_.dead_zone_px) return;

    const auto now = std::chrono::steady_clock::now();

    // Cooldown: after a trigger, ignore further input for 800ms so one
    // vigorous shake doesn't fire the callback multiple times.
    if (now - last_trigger_time_ < std::chrono::milliseconds(800)) {
        return;
    }

    if (is_positive != last_direction_positive_) {
        // Direction reversed.  Count this reversal only if we are still within
        // the configured time window; otherwise start a fresh streak.
        if (now - last_change_time_ < config_.time_window) {
            ++direction_changes_;
            if (direction_changes_ >= config_.direction_changes) {
                // Threshold reached — shake detected.
                last_trigger_time_ = now;
                callback_();
                reset_state();
                return;
            }
        } else {
            // Time window expired — this reversal starts a new potential shake.
            direction_changes_ = 1;
        }
        last_direction_positive_ = is_positive;
        last_change_time_        = now;
    }
}

void MouseShakeDetector::set_mouse_button_down(bool down) {
    if (!down && mouse_button_down_) {
        // Button released — discard accumulated shake state so the next press
        // starts a fresh gesture with no carry-over.
        reset_state();
    }
    mouse_button_down_ = down;
}

void MouseShakeDetector::reset_state() {
    direction_changes_        = 0;
    last_direction_positive_  = true;
    // has_last_position_ intentionally not reset — the position itself is still
    // valid; only the gesture accumulator needs clearing.
}

} // namespace dd
