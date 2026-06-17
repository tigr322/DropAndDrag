#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <string_view>
#include <vector>

namespace dd {

enum class Easing : uint8_t {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Spring,
};

namespace easing {
    [[nodiscard]] inline float apply(Easing type, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (type) {
            case Easing::Linear:
                return t;
            case Easing::EaseIn:
                return t * t;
            case Easing::EaseOut:
                return t * (2.0f - t);
            case Easing::EaseInOut:
                return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
            case Easing::Spring:
                return 1.0f - std::exp(-6.0f * t) * std::cos(12.0f * t);
        }
        return t;
    }
} // namespace easing

class Animation {
public:
    explicit Animation(float duration_ms = 300.0f, Easing easing = Easing::EaseOut);
    virtual ~Animation() = default;

    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;
    Animation(Animation&&) noexcept = default;
    Animation& operator=(Animation&&) noexcept = default;

    void start();
    void stop();
    void reset();
    void update(float delta_ms);

    [[nodiscard]] virtual float getValue() const;
    [[nodiscard]] bool isRunning() const noexcept { return is_running_; }
    [[nodiscard]] bool isCompleted() const noexcept { return is_completed_; }

    void setDuration(float duration_ms) noexcept { duration_ms_ = duration_ms; }
    void setEasing(Easing easing) noexcept { easing_ = easing; }

protected:
    [[nodiscard]] float progress() const noexcept { return progress_; }
    [[nodiscard]] float easedProgress() const noexcept;

    float duration_ms_;
    float elapsed_ms_ = 0.0f;
    Easing easing_;
    bool is_running_ = false;
    bool is_completed_ = false;

private:
    float progress_ = 0.0f;
};

class FadeAnimation : public Animation {
public:
    explicit FadeAnimation(float duration_ms = 300.0f, Easing easing = Easing::EaseOut);
    void start(float from, float to);

    [[nodiscard]] float getValue() const override;

private:
    float from_ = 0.0f;
    float to_ = 1.0f;
};

class SlideAnimation : public Animation {
public:
    explicit SlideAnimation(float duration_ms = 300.0f, Easing easing = Easing::EaseOut);
    void start(float from_x, float from_y, float to_x, float to_y);

    [[nodiscard]] float getValueX() const;
    [[nodiscard]] float getValueY() const;
    [[nodiscard]] float getValue() const override { return getValueX(); }

private:
    float from_x_ = 0.0f;
    float from_y_ = 0.0f;
    float to_x_ = 0.0f;
    float to_y_ = 0.0f;
};

class ScaleAnimation : public Animation {
public:
    explicit ScaleAnimation(float duration_ms = 300.0f, Easing easing = Easing::EaseOut);
    void start(float from_scale, float to_scale);

    [[nodiscard]] float getValue() const override;

private:
    float from_ = 1.0f;
    float to_ = 1.0f;
};

class BounceAnimation : public Animation {
public:
    explicit BounceAnimation(float duration_ms = 500.0f, Easing easing = Easing::EaseOut);
    void start(float from, float to);

    [[nodiscard]] float getValue() const override;

private:
    float from_ = 0.0f;
    float to_ = 1.0f;
};

class AnimationManager {
public:
    AnimationManager() = default;
    ~AnimationManager() = default;

    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;
    AnimationManager(AnimationManager&&) = delete;
    AnimationManager& operator=(AnimationManager&&) = delete;

    void add(std::unique_ptr<Animation> animation);
    void updateAll(float delta_ms);
    void removeCompleted();

    [[nodiscard]] size_t activeCount() const noexcept { return animations_.size(); }
    void clear();

private:
    std::vector<std::unique_ptr<Animation>> animations_;
};

} // namespace dd
