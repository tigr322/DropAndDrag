#pragma once

// mouse_shake_detector.hpp — Algorithm that recognises a "shake" gesture.
//
// A shake is defined as N rapid direction reversals on either axis within a
// time window, while a mouse button is held down (i.e. while the user is
// dragging a file).  When detected, a registered callback fires once; a
// cooldown prevents re-firing until the user lifts and re-presses the mouse.
//
// The algorithm runs on the CGEventTap callback thread (macOS), which is the
// main run-loop thread — all state is accessed single-threadedly.
//
// Tuning (ShakeConfig defaults):
//   direction_changes = 4   — 4 reversals required (left→right→left→right)
//   dead_zone_px      = 8   — ignore tiny jitter below 8px per sample
//   time_window       = 500ms — all reversals must happen within half a second
//   sample_interval   = 8ms  — how often CGEventTap fires (platform-driven)

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace dd {

using ShakeCallback = std::function<void()>;

// Algorithm tuning parameters.  Can be overridden via set_config() at runtime.
struct ShakeConfig {
    int                          direction_changes{4};   // reversals before trigger
    int                          dead_zone_px{8};        // minimum movement per sample
    std::chrono::milliseconds    time_window{500};       // window for all reversals
    std::chrono::milliseconds    sample_interval{8};     // expected sample rate
    bool                         enabled{true};          // master on/off switch
};

class MouseShakeDetector {
public:
    explicit MouseShakeDetector(ShakeConfig config = {});
    ~MouseShakeDetector();

    MouseShakeDetector(const MouseShakeDetector&)            = delete;
    MouseShakeDetector& operator=(const MouseShakeDetector&) = delete;
    MouseShakeDetector(MouseShakeDetector&&)                 = delete;
    MouseShakeDetector& operator=(MouseShakeDetector&&)      = delete;

    // Register the callback that fires when a shake is detected.
    // Replaces any previously registered callback.
    void set_callback(ShakeCallback cb);

    // Update algorithm parameters and reset accumulated state.
    void set_config(const ShakeConfig& config);

    // Enable or disable without changing other config.  Disabling resets state.
    void set_enabled(bool enabled);

    // Called by the platform mouse monitor on every mouse-move event.
    // x, y are screen coordinates (pixels, top-left origin on macOS).
    void on_mouse_move(int x, int y);

    // Called by the platform mouse monitor on button press/release events.
    // Shake detection only runs while a button is held (user is dragging).
    // Releasing the button resets accumulated state.
    void set_mouse_button_down(bool down);

    [[nodiscard]] bool is_enabled() const noexcept { return config_.enabled; }

private:
    // Reset direction change count and directional state.
    // Called after a successful trigger or after the button is released.
    void reset_state();

    ShakeConfig   config_;
    ShakeCallback callback_;

    // Running state — last known position and directional streak.
    int  last_x_{0};
    int  last_y_{0};
    int  direction_changes_{0};
    bool last_direction_positive_{true};  // true = right/down, false = left/up
    bool has_last_position_{false};       // false until first on_mouse_move call

    // Timestamps for the time-window check and the trigger cooldown.
    std::chrono::steady_clock::time_point last_change_time_;
    std::chrono::steady_clock::time_point last_trigger_time_;  // 800ms cooldown

    bool mouse_button_down_{false};   // shake only fires while a button is held
};

} // namespace dd
