#pragma once

#include <include/core/SkCanvas.h>
#include <include/core/SkRect.h>

#include <chrono>
#include <memory>

namespace dd {

class SkiaContext;
class Theme;
class AnimationManager;
class ShelfView;
class ContextMenu;
class SearchBar;

class Renderer {
public:
    static Renderer& instance();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    [[nodiscard]] bool init(void* native_window);
    void shutdown();

    void render(float delta_time_ms);
    void invalidate();

    void set_size(int width, int height);
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] float fps() const noexcept { return fps_; }
    [[nodiscard]] float dpi_scale() const noexcept;

    [[nodiscard]] SkiaContext* skia_context() noexcept { return skia_context_.get(); }
    [[nodiscard]] Theme* theme() noexcept { return theme_.get(); }
    [[nodiscard]] AnimationManager* animation_manager() noexcept { return animation_manager_.get(); }
    [[nodiscard]] ShelfView* shelf_view() noexcept { return shelf_view_.get(); }
    [[nodiscard]] ContextMenu* context_menu() noexcept { return context_menu_.get(); }
    [[nodiscard]] SearchBar* search_bar() noexcept { return search_bar_.get(); }

private:
    Renderer() = default;
    ~Renderer();

    void update_frame_timing(float delta_time_ms);

    std::unique_ptr<SkiaContext> skia_context_;
    std::unique_ptr<Theme> theme_;
    std::unique_ptr<AnimationManager> animation_manager_;
    std::unique_ptr<ShelfView> shelf_view_;
    std::unique_ptr<ContextMenu> context_menu_;
    std::unique_ptr<SearchBar> search_bar_;

    int width_ = 0;
    int height_ = 0;
    bool dirty_ = true;
    bool initialized_ = false;

    float fps_ = 0.0f;
    float frame_time_accum_ = 0.0f;
    int frame_count_ = 0;
    std::chrono::steady_clock::time_point last_fps_update_;
};

} // namespace dd
