#include "shelf_view.hpp"

#include "src/ui/item_view/item_view.hpp"
#include "src/ui/layout/layout.hpp"
#include "src/ui/renderer/skia_context.hpp"
#include "src/ui/themes/theme.hpp"

#include <include/core/SkBlurMaskFilter.h>
#include <include/core/SkFont.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkRRect.h>
#include <include/core/SkTextBlob.h>

#include <algorithm>

namespace dd {

ShelfView::ShelfView() {
    layout_engine_ = std::make_unique<LayoutEngine>();
}

ShelfView::~ShelfView() = default;

void ShelfView::init(SkiaContext* skia_context) {
    skia_context_ = skia_context;
}

void ShelfView::render(SkCanvas* canvas, const Rect& bounds) {
    if (!visible_ || !skia_context_) return;

    layout({bounds.x, bounds.y, bounds.width, std::max(bounds.height, kMinShelfHeight)});

    SkRect sk_bounds = SkRect::MakeXYWH(
        bounds_.x, bounds_.y, bounds_.width, bounds_.height
    );

    const auto& theme = Theme::current();

    canvas->save();
    canvas->clipRRect(SkRRect::MakeRectXY(sk_bounds, kCornerRadius, kCornerRadius), true);

    drawGlassBackground(canvas, sk_bounds);

    if (dragging_over_) {
        drawDropIndicator(canvas);
    }

    canvas->save();
    canvas->translate(0.0f, -scroll_offset_);

    for (size_t i = 0; i < item_views_.size() && i < item_layouts_.size(); ++i) {
        if (item_layouts_[i].y + item_layouts_[i].height < scroll_offset_ ||
            item_layouts_[i].y > scroll_offset_ + sk_bounds.height()) {
            continue;
        }

        const auto& layout = item_layouts_[i];
        const Rect item_bounds = {layout.x, layout.y, layout.width, layout.height};

        item_views_[i]->render(canvas, item_bounds, items_[i]);
    }

    canvas->restore();

    drawResizeHandle(canvas, sk_bounds);

    canvas->restore();

    drawShadow(canvas, sk_bounds);
}

bool ShelfView::handleEvent(const MouseEvent& event) {
    if (!visible_) return false;

    if (event.type == MouseEvent::Type::Scroll) {
        target_scroll_offset_ = std::clamp(
            target_scroll_offset_ + event.delta_y * 20.0f,
            0.0f,
            maxScrollOffset()
        );
        scroll_offset_ = std::lerp(scroll_offset_, target_scroll_offset_, 0.3f);
        updateLayout();
        return true;
    }

    if (event.type == MouseEvent::Type::Move) {
        hovered_index_ = static_cast<int>(itemIndexAt(event.x, event.y + scroll_offset_));
    }

    if (event.type == MouseEvent::Type::Down) {
        selected_index_ = hovered_index_;
    }

    routeMouseEventToItems(event);
    return bounds_.contains(event.x, event.y);
}

bool ShelfView::handleKeyEvent(const KeyEvent& event) {
    if (!visible_) return false;
    return propagateKeyEvent(event);
}

bool ShelfView::handleDragEvent(const DragEvent& event) {
    if (event.type == DragEvent::Type::Enter || event.type == DragEvent::Type::Over) {
        dragging_over_ = true;
        drag_x_ = event.x;
        drag_y_ = event.y;
        return true;
    }

    if (event.type == DragEvent::Type::Exit || event.type == DragEvent::Type::Drop) {
        dragging_over_ = false;
        return true;
    }

    return false;
}

void ShelfView::layout(const Rect& bounds) {
    bounds_ = bounds;
    updateLayout();
}

void ShelfView::setItems(const std::vector<Item>& items) {
    items_ = items;
    item_views_.clear();
    item_views_.reserve(items_.size());

    for (const auto& item : items_) {
        auto view = std::make_unique<ItemView>();
        view->init(skia_context_);
        item_views_.push_back(std::move(view));
    }

    updateLayout();
}

void ShelfView::addItem(const Item& item) {
    items_.push_back(item);

    auto view = std::make_unique<ItemView>();
    view->init(skia_context_);
    view->startAddAnimation();
    item_views_.push_back(std::move(view));

    updateLayout();
}

void ShelfView::removeItem(std::string_view uuid) {
    auto it = std::ranges::find_if(items_, [uuid](const Item& item) {
        return item.data.uuid == uuid;
    });

    if (it != items_.end()) {
        const auto index = static_cast<size_t>(std::distance(items_.begin(), it));
        items_.erase(it);

        if (index < item_views_.size()) {
            item_views_[index]->startRemoveAnimation();
            item_views_.erase(item_views_.begin() + static_cast<ptrdiff_t>(index));
        }

        updateLayout();
    }
}

void ShelfView::clearItems() {
    items_.clear();
    item_views_.clear();
    updateLayout();
}

void ShelfView::setScrollOffset(float offset) {
    target_scroll_offset_ = std::clamp(offset, 0.0f, maxScrollOffset());
}

float ShelfView::maxScrollOffset() const noexcept {
    return std::max(0.0f, content_height_ - bounds_.height);
}

void ShelfView::show() {
    visible_ = true;
    show_animation_running_ = true;
    anim_progress_ = 0.0f;
}

void ShelfView::hide() {
    visible_ = false;
}

void ShelfView::setSearchText(std::string_view text) {
    // Filtering logic — delegates to search controller externally
}

void ShelfView::drawGlassBackground(SkCanvas* canvas, const SkRect& bounds) {
    const auto& theme = Theme::current();
    const SkColor glass_color = theme.palette().glass_background;

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(glass_color);
    paint.setStyle(SkPaint::kFill_Style);

    SkRRect rrect = SkRRect::MakeRectXY(bounds, kCornerRadius, kCornerRadius);
    canvas->drawRRect(rrect, paint);
}

void ShelfView::drawDropIndicator(SkCanvas* canvas) {
    const auto& theme = Theme::current();
    const SkColor indicator_color = theme.palette().drop_indicator;

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(indicator_color);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2.0f);

    SkRect indicator = SkRect::MakeXYWH(
        bounds_.x + 2.0f, bounds_.y + 2.0f,
        bounds_.width - 4.0f, bounds_.height - 4.0f
    );

    SkRRect rrect = SkRRect::MakeRectXY(indicator, kCornerRadius - 1.0f, kCornerRadius - 1.0f);
    canvas->drawRRect(rrect, paint);
}

void ShelfView::drawShadow(SkCanvas* canvas, const SkRect& bounds) {
    const auto& theme = Theme::current();
    const SkColor shadow_color = theme.palette().shadow;

    SkPaint shadow_paint;
    shadow_paint.setAntiAlias(true);
    shadow_paint.setColor(shadow_color);

    auto blur = SkImageFilters::Blur(kShadowSigma, kShadowSigma, nullptr, nullptr);
    shadow_paint.setImageFilter(blur);

    SkRect shadow_bounds = bounds.makeOffset(0.0f, 4.0f);
    shadow_bounds.inset(-4.0f, -4.0f);

    canvas->drawRoundRect(shadow_bounds, kCornerRadius + 4.0f, kCornerRadius + 4.0f, shadow_paint);
}

void ShelfView::drawResizeHandle(SkCanvas* canvas, const SkRect& bounds) {
    const auto& theme = Theme::current();
    const SkColor handle_color = theme.palette().text_secondary;

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(handle_color);
    paint.setStyle(SkPaint::kFill_Style);

    const float hx = bounds.right() - kResizeHandleSize;
    const float hy = bounds.bottom() - kResizeHandleSize;

    constexpr float kDotRadius = 1.5f;
    constexpr float kDotSpacing = 5.0f;

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3 - row; ++col) {
            const float cx = hx + 4.0f + col * kDotSpacing + row * kDotSpacing * 0.5f;
            const float cy = hy + 4.0f + row * kDotSpacing;
            canvas->drawCircle(cx, cy, kDotRadius, paint);
        }
    }
}

void ShelfView::updateLayout() {
    if (!layout_engine_ || items_.empty()) {
        item_layouts_.clear();
        content_height_ = 0.0f;
        return;
    }

    item_layouts_ = layout_engine_->calculateGridLayout(
        bounds_.width,
        std::numeric_limits<float>::max(),
        items_.size(),
        LayoutConstants::kItemSize + 40.0f,
        LayoutConstants::kSpacing,
        LayoutConstants::kPadding
    );

    content_height_ = layout_engine_->getTotalContentHeight(
        items_.size(),
        LayoutConstants::kItemSize + 40.0f,
        LayoutConstants::kSpacing,
        LayoutConstants::kPadding,
        bounds_.width
    );
}

void ShelfView::routeMouseEventToItems(const MouseEvent& event) {
    if (hovered_index_ < 0 || static_cast<size_t>(hovered_index_) >= item_views_.size()) return;

    const auto& layout = item_layouts_[hovered_index_];

    MouseEvent local_event = event;
    local_event.x -= layout.x;
    local_event.y -= (layout.y - scroll_offset_);

    item_views_[hovered_index_]->handleEvent(local_event);
}

size_t ShelfView::itemIndexAt(float x, float y) const {
    for (size_t i = 0; i < item_layouts_.size(); ++i) {
        const auto& layout = item_layouts_[i];
        if (x >= layout.x && x <= layout.x + layout.width &&
            y >= layout.y && y <= layout.y + layout.height) {
            return i;
        }
    }
    return static_cast<size_t>(-1);
}

} // namespace dd
