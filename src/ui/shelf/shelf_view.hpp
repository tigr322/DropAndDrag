#pragma once

#include "src/ui/components/component.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace dd {

struct Item;
class SkiaContext;
class ItemView;
class SearchBar;
class LayoutEngine;

class ShelfView : public Component {
public:
    ShelfView();
    ~ShelfView() override;

    void init(SkiaContext* skia_context);

    void render(SkCanvas* canvas, const Rect& bounds) override;
    bool handleEvent(const MouseEvent& event) override;
    bool handleKeyEvent(const KeyEvent& event) override;
    bool handleDragEvent(const DragEvent& event) override;

    void layout(const Rect& bounds) override;

    void setItems(const std::vector<Item>& items);
    void addItem(const Item& item);
    void removeItem(std::string_view uuid);
    void clearItems();

    [[nodiscard]] const std::vector<Item>& items() const noexcept { return items_; }

    void setScrollOffset(float offset);
    [[nodiscard]] float scrollOffset() const noexcept { return scroll_offset_; }
    [[nodiscard]] float maxScrollOffset() const noexcept;

    void show();
    void hide();
    [[nodiscard]] bool isVisible() const noexcept { return visible_; }

    void setSearchText(std::string_view text);

private:
    void drawGlassBackground(SkCanvas* canvas, const SkRect& bounds);
    void drawDropIndicator(SkCanvas* canvas);
    void drawShadow(SkCanvas* canvas, const SkRect& bounds);
    void drawResizeHandle(SkCanvas* canvas, const SkRect& bounds);
    void updateLayout();
    void routeMouseEventToItems(const MouseEvent& event);
    [[nodiscard]] size_t itemIndexAt(float x, float y) const;

    SkiaContext* skia_context_ = nullptr;
    std::unique_ptr<LayoutEngine> layout_engine_;

    std::vector<Item> items_;
    std::vector<std::unique_ptr<ItemView>> item_views_;
    std::vector<ItemLayout> item_layouts_;

    float scroll_offset_ = 0.0f;
    float target_scroll_offset_ = 0.0f;
    float content_height_ = 0.0f;

    bool visible_ = true;
    bool dragging_over_ = false;
    float drag_x_ = 0.0f;
    float drag_y_ = 0.0f;

    int hovered_index_ = -1;
    int selected_index_ = -1;

    bool show_animation_running_ = false;
    float anim_progress_ = 1.0f;

    static constexpr float kCornerRadius = 16.0f;
    static constexpr float kShadowSigma = 20.0f;
    static constexpr float kGlassAlpha = 0.7f;
    static constexpr float kResizeHandleSize = 16.0f;
    static constexpr float kMinShelfHeight = 120.0f;
};

} // namespace dd
