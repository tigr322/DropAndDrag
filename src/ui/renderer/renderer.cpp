#include "renderer.hpp"

#include "skia_context.hpp"
#include "src/ui/animations/animation.hpp"
#include "src/ui/context_menu/context_menu.hpp"
#include "src/ui/search_bar/search_bar.hpp"
#include "src/ui/shelf/shelf_view.hpp"
#include "src/ui/themes/theme.hpp"

#include <include/core/SkColor.h>

namespace dd {

Renderer& Renderer::instance() {
    static Renderer instance;
    return instance;
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(void* native_window) {
    if (initialized_) return true;

    skia_context_ = std::make_unique<SkiaContext>();
    if (!skia_context_->init(native_window, width_, height_, dpi_scale())) {
        return false;
    }

    animation_manager_ = std::make_unique<AnimationManager>();
    theme_ = std::make_unique<Theme>();
    shelf_view_ = std::make_unique<ShelfView>();
    context_menu_ = std::make_unique<ContextMenu>();
    search_bar_ = std::make_unique<SearchBar>();

    shelf_view_->init(skia_context_.get());

    initialized_ = true;
    dirty_ = true;
    last_fps_update_ = std::chrono::steady_clock::now();

    return true;
}

void Renderer::shutdown() {
    if (!initialized_) return;

    search_bar_.reset();
    context_menu_.reset();
    shelf_view_.reset();
    theme_.reset();
    animation_manager_.reset();

    if (skia_context_) {
        skia_context_->shutdown();
        skia_context_.reset();
    }

    initialized_ = false;
}

void Renderer::render(float delta_time_ms) {
    if (!initialized_ || !skia_context_) return;

    animation_manager_->updateAll(delta_time_ms);
    update_frame_timing(delta_time_ms);

    auto* canvas = skia_context_->beginFrame();
    if (!canvas) return;

    const SkRect bounds = SkRect::MakeWH(static_cast<float>(width_), static_cast<float>(height_));

    canvas->clear(SK_ColorTRANSPARENT);

    canvas->save();
    canvas->scale(dpi_scale(), dpi_scale());

    const float logical_w = static_cast<float>(width_) / dpi_scale();
    const float logical_h = static_cast<float>(height_) / dpi_scale();
    const SkRect logical_bounds = SkRect::MakeWH(logical_w, logical_h);

    theme_->drawBackground(canvas, logical_bounds);

    shelf_view_->render(canvas, logical_bounds);

    if (search_bar_) {
        constexpr float kSearchBarHeight = 44.0f;
        const SkRect search_bounds = SkRect::MakeXYWH(0.0f, 0.0f, logical_w, kSearchBarHeight);
        search_bar_->render(canvas, search_bounds, *theme_);
    }

    if (context_menu_ && context_menu_->isVisible()) {
        context_menu_->render(canvas, *theme_);
    }

    canvas->restore();

    skia_context_->endFrame();
    dirty_ = false;
}

void Renderer::invalidate() {
    dirty_ = true;
}

void Renderer::set_size(int width, int height) {
    if (width == width_ && height == height_) return;

    width_ = width;
    height_ = height;

    if (skia_context_ && initialized_) {
        skia_context_->resize(width, height);
        shelf_view_->layout();
    }

    invalidate();
}

float Renderer::dpi_scale() const noexcept {
    if (skia_context_) {
        return skia_context_->dpi_scale();
    }
    return 1.0f;
}

void Renderer::update_frame_timing(float delta_time_ms) {
    ++frame_count_;
    frame_time_accum_ += delta_time_ms;

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_update_).count();

    if (elapsed >= 1000) {
        fps_ = static_cast<float>(frame_count_) / (static_cast<float>(elapsed) / 1000.0f);
        frame_count_ = 0;
        frame_time_accum_ = 0.0f;
        last_fps_update_ = now;
    }
}

} // namespace dd
