#include "animation.hpp"

#include <cmath>
#include <ranges>

namespace dd {

Animation::Animation(float duration_ms, Easing easing)
    : duration_ms_(duration_ms)
    , easing_(easing) {}

void Animation::start() {
    is_running_ = true;
    is_completed_ = false;
    elapsed_ms_ = 0.0f;
    progress_ = 0.0f;
}

void Animation::stop() {
    is_running_ = false;
}

void Animation::reset() {
    is_running_ = false;
    is_completed_ = false;
    elapsed_ms_ = 0.0f;
    progress_ = 0.0f;
}

void Animation::update(float delta_ms) {
    if (!is_running_ || is_completed_) return;

    elapsed_ms_ += delta_ms;

    if (elapsed_ms_ >= duration_ms_) {
        elapsed_ms_ = duration_ms_;
        progress_ = 1.0f;
        is_completed_ = true;
        is_running_ = false;
    } else {
        progress_ = elapsed_ms_ / duration_ms_;
    }
}

float Animation::getValue() const {
    return easedProgress();
}

float Animation::easedProgress() const noexcept {
    return easing::apply(easing_, progress_);
}

FadeAnimation::FadeAnimation(float duration_ms, Easing easing)
    : Animation(duration_ms, easing) {}

void FadeAnimation::start(float from, float to) {
    from_ = from;
    to_ = to;
    Animation::start();
}

float FadeAnimation::getValue() const {
    const float t = easedProgress();
    return from_ + (to_ - from_) * t;
}

SlideAnimation::SlideAnimation(float duration_ms, Easing easing)
    : Animation(duration_ms, easing) {}

void SlideAnimation::start(float from_x, float from_y, float to_x, float to_y) {
    from_x_ = from_x;
    from_y_ = from_y;
    to_x_ = to_x;
    to_y_ = to_y;
    Animation::start();
}

float SlideAnimation::getValueX() const {
    const float t = easedProgress();
    return from_x_ + (to_x_ - from_x_) * t;
}

float SlideAnimation::getValueY() const {
    const float t = easedProgress();
    return from_y_ + (to_y_ - from_y_) * t;
}

ScaleAnimation::ScaleAnimation(float duration_ms, Easing easing)
    : Animation(duration_ms, easing) {}

void ScaleAnimation::start(float from_scale, float to_scale) {
    from_ = from_scale;
    to_ = to_scale;
    Animation::start();
}

float ScaleAnimation::getValue() const {
    const float t = easedProgress();
    return from_ + (to_ - from_) * t;
}

BounceAnimation::BounceAnimation(float duration_ms, Easing easing)
    : Animation(duration_ms, easing) {}

void BounceAnimation::start(float from, float to) {
    from_ = from;
    to_ = to;
    Animation::start();
}

float BounceAnimation::getValue() const {
    const float t = easedProgress();
    const float bounce = std::abs(std::sin(t * static_cast<float>(M_PI) * 3.0f)) * (1.0f - t);
    return from_ + (to_ - from_) * (t + bounce * 0.15f);
}

void AnimationManager::add(std::unique_ptr<Animation> animation) {
    if (animation) {
        animations_.push_back(std::move(animation));
    }
}

void AnimationManager::updateAll(float delta_ms) {
    for (auto& anim : animations_) {
        if (anim && anim->isRunning()) {
            anim->update(delta_ms);
        }
    }
}

void AnimationManager::removeCompleted() {
    std::erase_if(animations_, [](const auto& anim) {
        return !anim || (!anim->isRunning() && anim->isCompleted());
    });
}

void AnimationManager::clear() {
    animations_.clear();
}

} // namespace dd
