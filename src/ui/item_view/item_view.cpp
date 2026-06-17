#include "item_view.hpp"

#include "src/core/items/item.hpp"
#include "src/ui/animations/animation.hpp"
#include "src/ui/renderer/skia_context.hpp"
#include "src/ui/themes/theme.hpp"

#include <include/core/SkBlurMaskFilter.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkFont.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkRRect.h>
#include <include/core/SkString.h>
#include <include/core/SkTextBlob.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <iomanip>
#include <sstream>

namespace dd {

ItemView::ItemView()
    : fade_animation_(std::make_unique<FadeAnimation>(200.0f))
    , scale_animation_(std::make_unique<ScaleAnimation>(200.0f)) {}

ItemView::~ItemView() = default;

void ItemView::init(SkiaContext* skia_context) {
    skia_context_ = skia_context;
}

void ItemView::render(SkCanvas* canvas, const Rect& bounds) {
    render(canvas, bounds, Item{});
}

void ItemView::render(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    canvas->save();

    if (state_ == State::Dragging) {
        canvas->alpha_(0.5f);
    } else {
        canvas->alpha_(anim_opacity_);
    }

    canvas->translate(bounds.x, bounds.y);

    if (scale_animation_->isRunning()) {
        const float s = anim_scale_;
        canvas->translate(bounds.width * 0.5f, bounds.height * 0.5f);
        canvas->scale(s, s);
        canvas->translate(-bounds.width * 0.5f, -bounds.height * 0.5f);
    }

    const Rect local_bounds{0.0f, 0.0f, bounds.width, bounds.height};

    drawBackground(canvas, local_bounds);

    switch (item.data.type) {
        case ItemType::Image:
            drawThumbnail(canvas, local_bounds, item);
            break;
        case ItemType::Text:
            drawTextPreview(canvas, local_bounds, item);
            break;
        case ItemType::URL:
            drawURLPreview(canvas, local_bounds, item);
            break;
        default:
            drawFileIcon(canvas, local_bounds, item);
            break;
    }

    drawFileName(canvas, local_bounds, item);

    if (item.data.file_size.has_value()) {
        drawFileSize(canvas, local_bounds, item);
    }

    drawTypeBadge(canvas, local_bounds, item);

    if (item.metadata.is_favorite) {
        drawFavoriteStar(canvas, local_bounds, item);
    }

    if (!item.metadata.tags.empty()) {
        drawTags(canvas, local_bounds, item);
    }

    if (state_ == State::Selected) {
        drawSelectionHighlight(canvas, local_bounds);
    }

    canvas->restore();
}

bool ItemView::handleEvent(const MouseEvent& event) {
    if (event.type == MouseEvent::Type::Enter) {
        setState(State::Hovered);
        return true;
    }

    if (event.type == MouseEvent::Type::Exit) {
        setState(State::Normal);
        return true;
    }

    return false;
}

void ItemView::layout(const Rect& bounds) {
    bounds_ = bounds;
}

void ItemView::startAddAnimation() {
    anim_scale_ = 0.0f;
    anim_opacity_ = 0.0f;
    scale_animation_->start(0.0f, 1.0f);
    fade_animation_->start(0.0f, 1.0f);
}

void ItemView::startRemoveAnimation() {
    scale_animation_->start(1.0f, 0.0f);
    fade_animation_->start(1.0f, 0.0f);
}

void ItemView::drawBackground(SkCanvas* canvas, const Rect& bounds) {
    const auto& theme = Theme::current();
    SkColor bg_color;

    switch (state_) {
        case State::Hovered:
            bg_color = theme.palette().surface_hover;
            break;
        case State::Selected:
            bg_color = theme.palette().accent;
            break;
        default:
            bg_color = theme.palette().surface;
            break;
    }

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(bg_color);

    SkRRect rrect = SkRRect::MakeRectXY(bounds.toSkRect(), kCornerRadius, kCornerRadius);
    canvas->drawRRect(rrect, paint);
}

void ItemView::drawFileIcon(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    const auto& theme = Theme::current();
    const SkColor fg_color = theme.palette().text_primary;

    const float icon_size = bounds.width * 0.45f;
    const float cx = bounds.x + bounds.width * 0.5f;
    const float cy = bounds.y + bounds.height * 0.35f;

    SkRect icon_rect = SkRect::MakeXYWH(
        cx - icon_size * 0.5f,
        cy - icon_size * 0.5f,
        icon_size,
        icon_size
    );

    SkPaint rect_paint;
    rect_paint.setAntiAlias(true);
    rect_paint.setColor(fg_color);
    rect_paint.setStyle(SkPaint::kStroke_Style);
    rect_paint.setStrokeWidth(2.0f);

    const float corner = icon_size * 0.15f;
    SkRRect file_rrect = SkRRect::MakeRectXY(icon_rect, corner, corner);
    canvas->drawRRect(file_rrect, rect_paint);

    const float fold_w = icon_size * 0.3f;
    const float fold_h = icon_size * 0.25f;
    SkPath fold_path;
    fold_path.moveTo(icon_rect.right() - fold_w, icon_rect.top());
    fold_path.lineTo(icon_rect.right(), icon_rect.top() + fold_h);
    fold_path.lineTo(icon_rect.right() - fold_w, icon_rect.top() + fold_h);
    fold_path.close();

    SkPaint fold_paint;
    fold_paint.setAntiAlias(true);
    fold_paint.setColor(theme.palette().accent);
    canvas->drawPath(fold_path, fold_paint);

    SkPaint stroke_paint;
    stroke_paint.setAntiAlias(true);
    stroke_paint.setColor(fg_color);
    stroke_paint.setStyle(SkPaint::kStroke_Style);
    stroke_paint.setStrokeWidth(2.0f);
    stroke_paint.setStrokeJoin(SkPaint::kRound_Join);

    SkPath outline;
    outline.moveTo(icon_rect.right() - fold_w, icon_rect.top());
    outline.lineTo(icon_rect.right(), icon_rect.top() + fold_h);
    canvas->drawPath(outline, stroke_paint);
}

void ItemView::drawThumbnail(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    const auto& theme = Theme::current();

    const float thumb_size = bounds.width - kIconPadding * 4.0f;
    const float tx = bounds.x + (bounds.width - thumb_size) * 0.5f;
    const float ty = bounds.y + kIconPadding;

    SkRect thumb_rect = SkRect::MakeXYWH(tx, ty, thumb_size, thumb_size);

    if (cached_thumbnail_ && cached_item_uuid_ == item.data.uuid) {
        canvas->drawImageRect(cached_thumbnail_, thumb_rect, SkSamplingOptions());
    } else {
        loadThumbnailAsync(item);

        SkPaint placeholder_paint;
        placeholder_paint.setAntiAlias(true);
        placeholder_paint.setColor(theme.palette().surface_hover);

        SkRRect rrect = SkRRect::MakeRectXY(thumb_rect, kCornerRadius * 0.5f, kCornerRadius * 0.5f);
        canvas->drawRRect(rrect, placeholder_paint);
    }

    SkPaint border_paint;
    border_paint.setAntiAlias(true);
    border_paint.setColor(theme.palette().border);
    border_paint.setStyle(SkPaint::kStroke_Style);
    border_paint.setStrokeWidth(1.0f);

    SkRRect border_rrect = SkRRect::MakeRectXY(thumb_rect, kCornerRadius * 0.5f, kCornerRadius * 0.5f);
    canvas->drawRRect(border_rrect, border_paint);
}

void ItemView::drawFileName(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    const auto& theme = Theme::current();

    SkFont font;
    font.setSize(11.0f);
    font.setEdging(SkFont::Edging::kAntiAlias);

    std::string name = item.data.file_name.value_or(
        item.data.title.value_or("Unknown")
    );

    const float text_max_width = bounds.width - kIconPadding * 2.0f;
    name = ellipsizeText(name, text_max_width, font);

    const float text_x = bounds.x + kIconPadding;
    const float text_y = bounds.y + bounds.height - kIconPadding * 3.0f;

    for (int line = 0; line < kMaxNameLines; ++line) {
        std::string line_text;

        if (line == 0) {
            line_text = name;
        } else {
            size_t mid = name.length() / 2;
            auto space_pos = name.find(' ', mid);
            if (space_pos != std::string::npos && space_pos < name.length() - 1) {
                line_text = name.substr(0, space_pos);
                line_text = ellipsizeText(line_text, text_max_width, font);
            } else {
                break;
            }
            name = name.substr(line_text.length() + 1);
            if (name.empty()) break;
        }

        SkPaint text_paint;
        text_paint.setAntiAlias(true);
        text_paint.setColor(theme.palette().text_primary);

        const float line_y = text_y + line * (font.getSize() + 2.0f);
        canvas->drawString(line_text.c_str(), text_x, line_y, font, text_paint);
    }
}

void ItemView::drawFileSize(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    if (!item.data.file_size.has_value()) return;

    const auto& theme = Theme::current();
    const std::string size_str = formatFileSize(*item.data.file_size);

    SkFont font;
    font.setSize(10.0f);
    font.setEdging(SkFont::Edging::kAntiAlias);

    const float text_width = font.measureText(size_str.c_str(), size_str.length(), SkTextEncoding::kUTF8);
    const float text_x = bounds.x + bounds.width - text_width - kIconPadding;
    const float text_y = bounds.y + bounds.height - kIconPadding;

    SkPaint text_paint;
    text_paint.setAntiAlias(true);
    text_paint.setColor(theme.palette().text_secondary);

    canvas->drawString(size_str.c_str(), text_x, text_y, font, text_paint);
}

void ItemView::drawTypeBadge(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    const auto& theme = Theme::current();
    SkColor badge_color;

    switch (item.data.type) {
        case ItemType::Image:
            badge_color = theme.palette().tag_colors[3];
            break;
        case ItemType::Text:
            badge_color = theme.palette().tag_colors[4];
            break;
        case ItemType::URL:
            badge_color = theme.palette().tag_colors[6];
            break;
        case ItemType::Folder:
            badge_color = theme.palette().tag_colors[2];
            break;
        default:
            badge_color = theme.palette().text_secondary;
            break;
    }

    const float bx = bounds.x + kIconPadding;
    const float by = bounds.y + kIconPadding;

    SkPaint badge_paint;
    badge_paint.setAntiAlias(true);
    badge_paint.setColor(badge_color);

    SkRRect badge_rrect = SkRRect::MakeRectXY(
        SkRect::MakeXYWH(bx, by, kBadgeSize, kBadgeSize),
        kBadgeSize * 0.5f,
        kBadgeSize * 0.5f
    );
    canvas->drawRRect(badge_rrect, badge_paint);

    const char* type_char = "?";
    switch (item.data.type) {
        case ItemType::Image: type_char = "\u25A0"; break;
        case ItemType::Text:  type_char = "T"; break;
        case ItemType::URL:   type_char = "\u2197"; break;
        case ItemType::Folder:type_char = "\u25B6"; break;
        default:              type_char = "F"; break;
    }

    SkFont badge_font;
    badge_font.setSize(10.0f);
    badge_font.setEdging(SkFont::Edging::kAntiAlias);

    SkPaint text_paint;
    text_paint.setAntiAlias(true);
    text_paint.setColor(SK_ColorWHITE);

    const float text_w = badge_font.measureText(type_char, 1, SkTextEncoding::kUTF8);
    const float text_x = bx + (kBadgeSize - text_w) * 0.45f;
    const float text_y = by + kBadgeSize * 0.7f;

    canvas->drawString(type_char, text_x, text_y, badge_font, text_paint);
}

void ItemView::drawFavoriteStar(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    if (!item.metadata.is_favorite) return;

    const auto& theme = Theme::current();

    const float sx = bounds.x + bounds.width - kStarSize - kIconPadding;
    const float sy = bounds.y + kIconPadding;

    SkPaint star_paint;
    star_paint.setAntiAlias(true);
    star_paint.setColor(SkColorSetRGB(255, 204, 0));

    const float cx = sx + kStarSize * 0.5f;
    const float cy = sy + kStarSize * 0.5f;
    const float outer_r = kStarSize * 0.5f;
    const float inner_r = kStarSize * 0.2f;

    SkPath star_path;
    constexpr int kNumPoints = 5;
    for (int i = 0; i < kNumPoints * 2; ++i) {
        const float angle = static_cast<float>(i) * static_cast<float>(M_PI) / kNumPoints - static_cast<float>(M_PI) / 2.0f;
        const float r = (i % 2 == 0) ? outer_r : inner_r;
        const float px = cx + std::cos(angle) * r;
        const float py = cy + std::sin(angle) * r;

        if (i == 0) {
            star_path.moveTo(px, py);
        } else {
            star_path.lineTo(px, py);
        }
    }
    star_path.close();

    canvas->drawPath(star_path, star_paint);
}

void ItemView::drawSelectionHighlight(SkCanvas* canvas, const Rect& bounds) {
    const auto& theme = Theme::current();

    SkPaint highlight_paint;
    highlight_paint.setAntiAlias(true);
    highlight_paint.setColor(theme.palette().accent);
    highlight_paint.setStyle(SkPaint::kStroke_Style);
    highlight_paint.setStrokeWidth(2.0f);

    SkRRect rrect = SkRRect::MakeRectXY(
        bounds.toSkRect().makeInset(1.0f, 1.0f),
        kCornerRadius,
        kCornerRadius
    );
    canvas->drawRRect(rrect, highlight_paint);
}

void ItemView::drawTags(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    const auto& theme = Theme::current();

    const float start_x = bounds.x + kIconPadding;
    const float start_y = bounds.y + bounds.height - kTagDotRadius * 2.0f - 2.0f;

    size_t max_tags = std::min(item.metadata.tags.size(), size_t{5});

    for (size_t i = 0; i < max_tags; ++i) {
        const float cx = start_x + i * (kTagDotRadius * 2.5f);
        const size_t color_idx = std::hash<std::string>{}(item.metadata.tags[i]) % theme.palette().tag_colors.size();

        SkPaint dot_paint;
        dot_paint.setAntiAlias(true);
        dot_paint.setColor(theme.palette().tag_colors[color_idx]);

        canvas->drawCircle(cx + kTagDotRadius, start_y + kTagDotRadius, kTagDotRadius, dot_paint);
    }
}

void ItemView::drawTextPreview(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    const auto& theme = Theme::current();

    std::string text = item.data.text_content.value_or("");
    if (text.length() > static_cast<size_t>(kMaxTextChars)) {
        text = text.substr(0, kMaxTextChars);
        text += "...";
    }

    SkFont font;
    font.setSize(9.0f);
    font.setEdging(SkFont::Edging::kAntiAlias);

    SkPaint text_paint;
    text_paint.setAntiAlias(true);
    text_paint.setColor(theme.palette().text_secondary);

    const float text_x = bounds.x + kIconPadding * 2.0f;
    const float text_y = bounds.y + kIconPadding * 2.0f + font.getSize();

    const float max_width = bounds.width - kIconPadding * 4.0f;
    const float line_height = font.getSize() + 2.0f;
    const size_t chars_per_line = static_cast<size_t>(max_width / (font.getSize() * 0.6f));

    for (int line = 0; line < 4; ++line) {
        const size_t start = line * chars_per_line;
        if (start >= text.length()) break;

        std::string line_str = text.substr(start, chars_per_line);
        canvas->drawString(line_str.c_str(), text_x, text_y + line * line_height, font, text_paint);
    }
}

void ItemView::drawURLPreview(SkCanvas* canvas, const Rect& bounds, const Item& item) {
    const auto& theme = Theme::current();

    if (cached_favicon_ && cached_item_uuid_ == item.data.uuid) {
        const float icon_size = 32.0f;
        const float ix = bounds.x + (bounds.width - icon_size) * 0.5f;
        const float iy = bounds.y + kIconPadding * 2.0f;
        SkRect icon_rect = SkRect::MakeXYWH(ix, iy, icon_size, icon_size);
        canvas->drawImageRect(cached_favicon_, icon_rect, SkSamplingOptions());
    }

    SkFont title_font;
    title_font.setSize(10.0f);
    title_font.setEdging(SkFont::Edging::kAntiAlias);

    std::string title = item.data.title.value_or(item.data.url.value_or("Unknown"));

    SkPaint title_paint;
    title_paint.setAntiAlias(true);
    title_paint.setColor(theme.palette().text_primary);

    const float title_x = bounds.x + kIconPadding;
    const float title_y = bounds.y + bounds.height * 0.5f;
    canvas->drawString(title.c_str(), title_x, title_y, title_font, title_paint);

    if (item.data.url.has_value()) {
        SkFont url_font;
        url_font.setSize(8.0f);
        url_font.setEdging(SkFont::Edging::kAntiAlias);

        SkPaint url_paint;
        url_paint.setAntiAlias(true);
        url_paint.setColor(theme.palette().text_secondary);

        const std::string short_url = item.data.url->substr(0, 30) + "...";
        canvas->drawString(short_url.c_str(), title_x, title_y + 12.0f, url_font, url_paint);
    }
}

void ItemView::loadThumbnailAsync(const Item& item) {
    if (thumbnail_loading_ || !item.data.thumbnail_data.has_value()) return;

    thumbnail_loading_ = true;
    cached_item_uuid_ = item.data.uuid;

    const auto& thumb_data = *item.data.thumbnail_data;
    auto data = SkData::MakeWithCopy(thumb_data.data(), thumb_data.size());
    cached_thumbnail_ = SkImages::DeferredFromEncodedData(data);
    thumbnail_loading_ = false;
}

std::string ItemView::formatFileSize(uint64_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        ++unit_idx;
    }

    std::ostringstream oss;
    if (unit_idx == 0) {
        oss << static_cast<uint64_t>(size) << " " << units[unit_idx];
    } else {
        oss << std::fixed << std::setprecision(1) << size << " " << units[unit_idx];
    }
    return oss.str();
}

std::string ItemView::ellipsizeText(const std::string& text, float max_width, const SkFont& font) const {
    if (text.empty()) return text;

    float text_width = font.measureText(text.c_str(), text.length(), SkTextEncoding::kUTF8);

    if (text_width <= max_width) return text;

    const float ellipsis_width = font.measureText("...", 3, SkTextEncoding::kUTF8);
    const float target_width = max_width - ellipsis_width;

    size_t lo = 0;
    size_t hi = text.length();

    while (lo < hi) {
        const size_t mid = lo + (hi - lo + 1) / 2;
        float w = font.measureText(text.c_str(), mid, SkTextEncoding::kUTF8);
        if (w <= target_width) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    return text.substr(0, lo) + "...";
}

} // namespace dd
