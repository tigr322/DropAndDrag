#include "search_bar.hpp"

#include "src/ui/animations/animation.hpp"
#include "src/ui/themes/theme.hpp"

#include <include/core/SkBlurMaskFilter.h>
#include <include/core/SkFont.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkRRect.h>

#include <algorithm>
#include <cmath>

namespace dd {

SearchBar::SearchBar()
    : expand_animation_(std::make_unique<FadeAnimation>(200.0f, Easing::EaseOut)) {}

SearchBar::~SearchBar() = default;

void SearchBar::render(SkCanvas* canvas, const Rect& bounds) {
    render(canvas, bounds, Theme::current());
}

void SearchBar::render(SkCanvas* canvas, const SkRect& bounds, const Theme& theme) {
    render(canvas, Rect::fromSkRect(bounds), theme);
}

void SearchBar::render(SkCanvas* canvas, const Rect& bounds, const Theme& theme) {
    const SkRect sk_bounds = bounds.toSkRect();
    const float bar_y = sk_bounds.centerY() - kBarHeight * 0.5f;

    const SkRect bar_area;
    if (active_) {
        expand_animation_->start(0.0f, 1.0f);
        bar_area = SkRect::MakeXYWH(
            sk_bounds.x() + 8.0f, bar_y,
            sk_bounds.width() - 16.0f, kBarHeight
        );
    } else {
        const float collapsed_width = kBarHeight;
        bar_area = SkRect::MakeXYWH(
            sk_bounds.centerX() - collapsed_width * 0.5f,
            bar_y, collapsed_width, kBarHeight
        );
    }

    canvas->save();

    drawBackground(canvas, bar_area, theme);

    drawSearchIcon(canvas, bar_area, theme);

    if (active_) {
        drawTextInput(canvas, bar_area, theme);
        drawCursor(canvas, bar_area, theme);
        if (!query_.empty()) {
            drawClearButton(canvas, bar_area, theme);
        }
        drawFilterButtons(canvas, bar_area, theme);
    }

    canvas->restore();
}

bool SearchBar::handleEvent(const MouseEvent& event) {
    const SkRect sk_bounds = bounds_.toSkRect();
    const float bar_y = sk_bounds.centerY() - kBarHeight * 0.5f;
    const SkRect bar_area = SkRect::MakeXYWH(
        sk_bounds.x() + 8.0f, bar_y,
        active_ ? sk_bounds.width() - 16.0f : kBarHeight,
        kBarHeight
    );

    if (event.type == MouseEvent::Type::Down) {
        if (bar_area.contains(event.x, event.y)) {
            if (!active_) {
                activate();
            }

            const float clear_x = bar_area.right() - kClearButtonSize - 12.0f;
            const float clear_y = bar_area.centerY() - kClearButtonSize * 0.5f;
            if (query_.empty() && !active_) return true;

            const float rb = bar_area.right() - kClearButtonSize - 8.0f;
            if (event.x >= rb - 4.0f && event.x <= bar_area.right() - 8.0f) {
                FilterType ft = filterAt(event.x, event.y);
                if (ft != active_filter_) {
                    setFilter(ft);
                }
                return true;
            }

            const SkRect clear_rect = SkRect::MakeXYWH(clear_x - 4.0f, clear_y - 4.0f,
                                                       kClearButtonSize + 8.0f, kClearButtonSize + 8.0f);
            if (query_.empty() && clear_rect.contains(event.x, event.y)) {
                clear();
                return true;
            }

            return true;
        }
    }

    if (event.type == MouseEvent::Type::Move) {
        if (bar_area.contains(event.x, event.y)) {
            clear_button_hovered_ = true;
        } else {
            clear_button_hovered_ = false;
            filter_hovered_ = -1;
        }
        return bar_area.contains(event.x, event.y);
    }

    return false;
}

bool SearchBar::handleKeyEvent(const KeyEvent& event) {
    if (!active_) return false;

    if (event.type == KeyEvent::Type::Down) {
        if (event.key_code == 0x1B) {
            deactivate();
            return true;
        }

        if (event.key_code == 0x08) {
            if (!query_.empty()) {
                query_.pop_back();
            }
            return true;
        }

        if (event.key_code == 0x0D) {
            return true;
        }
    }

    if (event.type == KeyEvent::Type::Char && event.character > 0) {
        if (event.character >= 0x20 && event.character != 0x7F) {
            query_ += static_cast<char>(event.character);
            return true;
        }
    }

    return false;
}

void SearchBar::handleTextInput(std::string_view text) {
    query_ = text;
}

void SearchBar::clear() {
    query_.clear();
    filter_hovered_ = -1;
}

void SearchBar::setFilter(FilterType filter) {
    active_filter_ = filter;
}

void SearchBar::activate() {
    active_ = true;
    expand_animation_->start(0.0f, 1.0f);
}

void SearchBar::deactivate() {
    active_ = false;
    query_.clear();
    expand_animation_->start(1.0f, 0.0f);
}

void SearchBar::layout(const Rect& bounds) {
    bounds_ = bounds;
}

Size SearchBar::getDesiredSize() const {
    return {
        200.0f,
        kBarHeight + 8.0f,
    };
}

void SearchBar::drawBackground(SkCanvas* canvas, const SkRect& bounds, const Theme& theme) {
    SkPaint bg_paint;
    bg_paint.setAntiAlias(true);
    bg_paint.setColor(theme.palette().glass_background);

    SkRRect bg_rrect = SkRRect::MakeRectXY(bounds, kCornerRadius, kCornerRadius);
    canvas->drawRRect(bg_rrect, bg_paint);

    SkPaint border_paint;
    border_paint.setAntiAlias(true);
    border_paint.setColor(active_
                              ? static_cast<SkColor>(theme.palette().accent)
                              : static_cast<SkColor>(theme.palette().border));
    border_paint.setStyle(SkPaint::kStroke_Style);
    border_paint.setStrokeWidth(1.0f);

    SkRRect border_rrect = SkRRect::MakeRectXY(
        bounds.makeInset(0.5f, 0.5f), kCornerRadius - 0.5f, kCornerRadius - 0.5f
    );
    canvas->drawRRect(border_rrect, border_paint);
}

void SearchBar::drawSearchIcon(SkCanvas* canvas, const SkRect& bounds, const Theme& theme) {
    const float cx = bounds.x() + kSearchIconSize * 0.5f + 10.0f;
    const float cy = bounds.centerY();

    SkPaint icon_paint;
    icon_paint.setAntiAlias(true);
    icon_paint.setColor(active_
                            ? static_cast<SkColor>(theme.palette().accent)
                            : static_cast<SkColor>(theme.palette().text_secondary));
    icon_paint.setStyle(SkPaint::kStroke_Style);
    icon_paint.setStrokeWidth(2.0f);
    icon_paint.setStrokeCap(SkPaint::kRound_Cap);

    const float radius = kSearchIconSize * 0.35f;
    canvas->drawCircle(cx - 2.0f, cy - 2.0f, radius, icon_paint);

    const float handle_angle = static_cast<float>(M_PI) / 4.0f;
    const float hx = cx + std::cos(handle_angle) * (radius + 1.0f);
    const float hy = cy + std::sin(handle_angle) * (radius + 1.0f);
    canvas->drawLine(cx + std::cos(handle_angle) * radius,
                     cy + std::sin(handle_angle) * radius,
                     hx + 4.0f, hy + 4.0f, icon_paint);
}

void SearchBar::drawTextInput(SkCanvas* canvas, const SkRect& bounds, const Theme& theme) {
    SkFont font;
    font.setSize(14.0f);
    font.setEdging(SkFont::Edging::kAntiAlias);

    SkPaint text_paint;
    text_paint.setAntiAlias(true);
    text_paint.setColor(theme.palette().text_primary);

    const float text_x = bounds.x() + kTextLeftPadding;
    const float text_y = bounds.centerY() + font.getSize() * 0.35f;

    const float max_text_width = bounds.width() - kTextLeftPadding - kTextRightPadding - 100.0f;

    std::string display_text = query_;
    float text_width = font.measureText(display_text.c_str(), display_text.length(), SkTextEncoding::kUTF8);

    while (text_width > max_text_width && !display_text.empty()) {
        display_text.erase(display_text.begin());
        text_width = font.measureText(display_text.c_str(), display_text.length(), SkTextEncoding::kUTF8);
    }

    if (!display_text.empty()) {
        canvas->drawString(display_text.c_str(), text_x, text_y, font, text_paint);
    } else if (!active_) {
        SkPaint placeholder_paint;
        placeholder_paint.setAntiAlias(true);
        placeholder_paint.setColor(theme.palette().text_secondary);
        canvas->drawString("Search...", text_x, text_y, font, placeholder_paint);
    }
}

void SearchBar::drawCursor(SkCanvas* canvas, const SkRect& bounds, const Theme& theme) {
    if (!cursor_visible_ || !active_) return;

    SkFont font;
    font.setSize(14.0f);
    font.setEdging(SkFont::Edging::kAntiAlias);

    const float text_width = font.measureText(query_.c_str(), query_.length(), SkTextEncoding::kUTF8);

    const float cursor_x = bounds.x() + kTextLeftPadding + text_width + 2.0f;
    const float cursor_y1 = bounds.centerY() - font.getSize() * 0.55f;
    const float cursor_y2 = bounds.centerY() + font.getSize() * 0.55f;

    SkPaint cursor_paint;
    cursor_paint.setAntiAlias(true);
    cursor_paint.setColor(theme.palette().accent);
    cursor_paint.setStrokeWidth(kCursorWidth);
    cursor_paint.setStrokeCap(SkPaint::kRound_Cap);

    canvas->drawLine(cursor_x, cursor_y1, cursor_x, cursor_y2, cursor_paint);
}

void SearchBar::drawClearButton(SkCanvas* canvas, const SkRect& bounds, const Theme& theme) {
    const float cx = bounds.right() - kClearButtonSize - kTextRightPadding;
    const float cy = bounds.centerY();

    SkPaint circle_paint;
    circle_paint.setAntiAlias(true);
    circle_paint.setColor(clear_button_hovered_
                              ? static_cast<SkColor>(theme.palette().surface_hover)
                              : SK_ColorTRANSPARENT);
    canvas->drawCircle(cx, cy, kClearButtonSize * 0.5f + 2.0f, circle_paint);

    SkPaint x_paint;
    x_paint.setAntiAlias(true);
    x_paint.setColor(theme.palette().text_secondary);
    x_paint.setStyle(SkPaint::kStroke_Style);
    x_paint.setStrokeWidth(1.5f);
    x_paint.setStrokeCap(SkPaint::kRound_Cap);

    const float offset = kClearButtonSize * 0.3f;
    canvas->drawLine(cx - offset, cy - offset, cx + offset, cy + offset, x_paint);
    canvas->drawLine(cx + offset, cy - offset, cx - offset, cy + offset, x_paint);
}

void SearchBar::drawFilterButtons(SkCanvas* canvas, const SkRect& bounds, const Theme& theme) {
    const float start_x = bounds.right() - 140.0f;

    constexpr FilterType filters[] = {
        FilterType::All,
        FilterType::Files,
        FilterType::Images,
        FilterType::Text,
        FilterType::URLs,
    };

    constexpr const char* labels[] = {"All", "F", "\u25A0", "T", "\u2197"};

    for (int i = 0; i < 5; ++i) {
        const float fx = start_x + i * (kFilterButtonSize + 4.0f);
        const float fy = bounds.centerY() - kFilterButtonSize * 0.5f;

        SkRect filter_rect = SkRect::MakeXYWH(fx, fy, kFilterButtonSize, kFilterButtonSize);
        drawFilterIcon(canvas, filter_rect, theme, filters[i], labels[i],
                       filters[i] == active_filter_ || filter_hovered_ == i);
    }
}

void SearchBar::drawFilterIcon(SkCanvas* canvas, const SkRect& bounds, const Theme& theme,
                                FilterType filter, const char* label, bool is_active) {
    SkPaint bg_paint;
    bg_paint.setAntiAlias(true);
    bg_paint.setColor(is_active ? theme.palette().accent : theme.palette().surface_hover);

    SkRRect rrect = SkRRect::MakeRectXY(bounds, 6.0f, 6.0f);
    canvas->drawRRect(rrect, bg_paint);

    SkFont font;
    font.setSize(10.0f);
    font.setEdging(SkFont::Edging::kAntiAlias);

    SkPaint text_paint;
    text_paint.setAntiAlias(true);
    text_paint.setColor(is_active ? SK_ColorWHITE : theme.palette().text_secondary);

    const float text_w = font.measureText(label, 1, SkTextEncoding::kUTF8);
    const float text_x = bounds.centerX() - text_w * 0.5f;
    const float text_y = bounds.centerY() + font.getSize() * 0.35f;

    canvas->drawString(label, text_x, text_y, font, text_paint);
}

FilterType SearchBar::filterAt(float x, float y) const {
    const float start_x = bounds_.x + bounds_.width - 140.0f;
    const float fy = bounds_.y + bounds_.height * 0.5f - kFilterButtonSize * 0.5f;

    for (int i = 0; i < 5; ++i) {
        const float fx = start_x + i * (kFilterButtonSize + 4.0f);
        SkRect filter_rect = SkRect::MakeXYWH(fx, fy, kFilterButtonSize, kFilterButtonSize);

        if (filter_rect.contains(x, y)) {
            return static_cast<FilterType>(i);
        }
    }

    return FilterType::All;
}

} // namespace dd
