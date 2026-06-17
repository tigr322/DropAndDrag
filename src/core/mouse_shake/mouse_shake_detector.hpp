#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace dd {

using ShakeCallback = std::function<void()>;

struct ShakeConfig {
    int direction_changes = 4;
    int dead_zone_px = 8;
    std::chrono::milliseconds time_window{500};
    std::chrono::milliseconds sample_interval{8};
    bool enabled = true;
};

class MouseShakeDetector {
public:
    explicit MouseShakeDetector(ShakeConfig config = {});
    ~MouseShakeDetector();

    MouseShakeDetector(const MouseShakeDetector&) = delete;
    MouseShakeDetector& operator=(const MouseShakeDetector&) = delete;
    MouseShakeDetector(MouseShakeDetector&&) = delete;
    MouseShakeDetector& operator=(MouseShakeDetector&&) = delete;

    void set_callback(ShakeCallback cb);
    void set_config(const ShakeConfig& config);
    void set_enabled(bool enabled);

    void on_mouse_move(int x, int y);
    void set_mouse_button_down(bool down);

    [[nodiscard]] bool is_enabled() const noexcept { return config_.enabled; }

private:
    void reset_state();

    ShakeConfig config_;
    ShakeCallback callback_;

    int last_x_ = 0;
    int last_y_ = 0;
    int direction_changes_ = 0;
    bool last_direction_positive_ = true;
    bool has_last_position_ = false;
    std::chrono::steady_clock::time_point last_change_time_;
    std::chrono::steady_clock::time_point last_trigger_time_;
    bool mouse_button_down_ = false;
};

} // namespace dd
