#include "context_menu.hpp"

#include "src/core/items/item.hpp"
#include "src/ui/themes/theme.hpp"

#include <include/core/SkBlurMaskFilter.h>
#include <include/core/SkFont.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkRRect.h>

#include <algorithm>

namespace dd {

void ContextMenu::show(int x, int y, const Item& context) {
    context_item_ = context;
    buildMenu(context);
    calculateLayout(3840, 2160);

    const float new_x = std::max(4.0f, std::min(static_cast<float>(x), 3840.0f - menu_bounds_.width() - 4.0f));
    const float new_y = std::max(4.0f, std::min(static_cast<float>(y), 2160.0f - menu_bounds_.height() - 4.0f));

    menu_bounds_ = SkRect::MakeXYWH(new_x, new_y, menu_bounds_.width(), menu_bounds_.height());
    hovered_index_ = -1;
    selected_index_ = -1;
    submenu_open_index_ = -1;
    visible_ = true;
}

void ContextMenu::hide() {
    visible_ = false;
    submenu_open_index_ = -1;
}

void ContextMenu::render(SkCanvas* canvas, const Theme& theme) {
    if (!visible_) return;

    SkPaint shadow_paint;
    shadow_paint.setAntiAlias(true);
    shadow_paint.setColor(theme.palette().shadow);
    shadow_paint.setImageFilter(SkImageFilters::Blur(kShadowSigma, kShadowSigma, nullptr, nullptr));

    SkRect shadow_bounds = menu_bounds_;
    shadow_bounds.offset(2.0f, 4.0f);
    canvas->drawRoundRect(shadow_bounds, kCornerRadius + 2.0f, kCornerRadius + 2.0f, shadow_paint);

    SkPaint bg_paint;
    bg_paint.setAntiAlias(true);
    bg_paint.setColor(theme.palette().surface);

    SkRRect bg_rrect = SkRRect::MakeRectXY(menu_bounds_, kCornerRadius, kCornerRadius);
    canvas->drawRRect(bg_rrect, bg_paint);

    SkPaint border_paint;
    border_paint.setAntiAlias(true);
    border_paint.setColor(theme.palette().border);
    border_paint.setStyle(SkPaint::kStroke_Style);
    border_paint.setStrokeWidth(0.5f);

    SkRRect border_rrect = SkRRect::MakeRectXY(
        menu_bounds_.makeInset(0.5f, 0.5f),
        kCornerRadius,
        kCornerRadius
    );
    canvas->drawRRect(border_rrect, border_paint);

    SkFont font;
    font.setSize(13.0f);
    font.setEdging(SkFont::Edging::kAntiAlias);

    canvas->save();
    canvas->clipRRect(bg_rrect, true);

    for (size_t i = 0; i < entries_.size(); ++i) {
        renderEntry(canvas, theme, i, entries_[i], font);
    }

    canvas->restore();

    if (submenu_open_index_ >= 0 && static_cast<size_t>(submenu_open_index_) < entries_.size()) {
        const auto& entry = entries_[submenu_open_index_];
        if (!entry.children.empty()) {
            const auto& parent_layout = layouts_[submenu_open_index_];
            const float sx = menu_bounds_.right();
            const float sy = parent_layout.bounds.top() - kPaddingY;

            SkRect sub_bounds = SkRect::MakeXYWH(sx, sy, kSubmenuWidth,
                                                  entry.children.size() * kEntryHeight + kPaddingY * 2.0f);

            SkPaint sub_bg;
            sub_bg.setAntiAlias(true);
            sub_bg.setColor(theme.palette().surface);

            SkRRect sub_rrect = SkRRect::MakeRectXY(sub_bounds, kCornerRadius, kCornerRadius);
            canvas->drawRRect(sub_rrect, sub_bg);

            SkPaint sub_border;
            sub_border.setAntiAlias(true);
            sub_border.setColor(theme.palette().border);
            sub_border.setStyle(SkPaint::kStroke_Style);
            sub_border.setStrokeWidth(0.5f);
            canvas->drawRRect(SkRRect::MakeRectXY(sub_bounds.makeInset(0.5f, 0.5f), kCornerRadius, kCornerRadius),
                              sub_border);

            for (size_t j = 0; j < entry.children.size(); ++j) {
                const SkRect child_bounds = SkRect::MakeXYWH(
                    sx + kPaddingX,
                    sy + kPaddingY + j * kEntryHeight,
                    kSubmenuWidth - kPaddingX * 2.0f,
                    kEntryHeight
                );

                if (static_cast<int>(j) == hovered_index_) {
                    SkPaint hover_paint;
                    hover_paint.setAntiAlias(true);
                    hover_paint.setColor(theme.palette().surface_hover);

                    SkRRect hover_rrect = SkRRect::MakeRectXY(child_bounds, kCornerRadius * 0.5f, kCornerRadius * 0.5f);
                    canvas->drawRRect(hover_rrect, hover_paint);
                }

                SkPaint text_paint;
                text_paint.setAntiAlias(true);
                text_paint.setColor(entry.children[j].enabled
                                        ? theme.palette().text_primary
                                        : theme.palette().text_secondary);

                canvas->drawString(entry.children[j].label.c_str(),
                                   child_bounds.x(), child_bounds.y() + kEntryHeight * 0.65f,
                                   font, text_paint);
            }
        }
    }
}

void ContextMenu::handleMouseMove(float x, float y) {
    if (!visible_) return;

    hovered_index_ = entryAt(x, y);
}

void ContextMenu::handleMouseDown(float x, float y) {
    if (!visible_) return;

    const int idx = entryAt(x, y);
    if (idx < 0) {
        hide();
        return;
    }

    if (static_cast<size_t>(idx) < entries_.size()) {
        const auto& entry = entries_[idx];

        if (entry.separator) return;

        if (!entry.children.empty()) {
            submenu_open_index_ = (submenu_open_index_ == idx) ? -1 : idx;
            return;
        }

        if (entry.enabled && callback_) {
            callback_(entry.action, context_item_);
        }

        hide();
    }
}

void ContextMenu::handleKey(int key_code) {
    if (!visible_) return;

    if (key_code == 0x28) {
        selected_index_ = std::min(selected_index_ + 1, static_cast<int>(entries_.size()) - 1);
    } else if (key_code == 0x26) {
        selected_index_ = std::max(selected_index_ - 1, 0);
    } else if (key_code == 0x0D || key_code == 0x20) {
        if (selected_index_ >= 0 && static_cast<size_t>(selected_index_) < entries_.size()) {
            const auto& entry = entries_[selected_index_];
            if (entry.enabled && callback_) {
                callback_(entry.action, context_item_);
            }
            hide();
        }
    } else if (key_code == 0x1B) {
        hide();
    }
}

bool ContextMenu::handleCharInput(uint32_t /*character*/) {
    return false;
}

void ContextMenu::buildMenu(const Item& context) {
    entries_.clear();

    entries_.push_back({MenuAction::Open, "Open"});
    entries_.push_back({MenuAction::RevealInFolder, "Reveal in Folder", context.data.path.has_value()});
    entries_.push_back({MenuAction::CopyPath, "Copy Path", context.data.path.has_value()});
    entries_.push_back({MenuAction::CopyContent, "Copy Content", context.data.text_content.has_value()});
    entries_.push_back({.separator = true, .label = ""});
    entries_.push_back({MenuAction::Rename, "Rename"});

    MenuEntry add_tag;
    add_tag.action = MenuAction::AddTag;
    add_tag.label = "Add Tag";
    add_tag.children.push_back({MenuAction::AddTag, "Work"});
    add_tag.children.push_back({MenuAction::AddTag, "Personal"});
    add_tag.children.push_back({MenuAction::AddTag, "Important"});
    add_tag.children.push_back({MenuAction::AddTag, "Archive"});
    entries_.push_back(std::move(add_tag));

    if (!context.metadata.tags.empty()) {
        MenuEntry remove_tag;
        remove_tag.action = MenuAction::RemoveTag;
        remove_tag.label = "Remove Tag";
        for (const auto& tag : context.metadata.tags) {
            remove_tag.children.push_back({MenuAction::RemoveTag, tag});
        }
        entries_.push_back(std::move(remove_tag));
    }

    entries_.push_back({.separator = true, .label = ""});

    if (context.metadata.is_favorite) {
        entries_.push_back({MenuAction::RemoveFromFavorites, "Remove from Favorites"});
    } else {
        entries_.push_back({MenuAction::AddToFavorites, "Add to Favorites"});
    }

    entries_.push_back({.separator = true, .label = ""});
    entries_.push_back({MenuAction::Delete, "Delete"});
    entries_.push_back({.separator = true, .label = ""});
    entries_.push_back({MenuAction::ClearShelf, "Clear Shelf"});
}

void ContextMenu::calculateLayout(int /*screen_width*/, int /*screen_height*/) {
    layouts_.clear();
    layouts_.reserve(entries_.size());

    float total_height = kPaddingY * 2.0f;

    for (const auto& entry : entries_) {
        if (entry.separator) {
            total_height += kSeparatorHeight + kPaddingY * 0.5f;
        } else {
            total_height += kEntryHeight;
        }
    }

    menu_bounds_ = SkRect::MakeXYWH(0, 0, kMenuWidth, total_height);

    float y = kPaddingY;
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& entry = entries_[i];

        if (entry.separator) {
            layouts_.push_back({SkRect::MakeXYWH(0, y, kMenuWidth, kSeparatorHeight), 0, -1});
            y += kSeparatorHeight + kPaddingY * 0.5f;
        } else {
            layouts_.push_back({
                SkRect::MakeXYWH(kPaddingX, y, kMenuWidth - kPaddingX * 2.0f, kEntryHeight),
                0, -1
            });
            y += kEntryHeight;
        }
    }
}

int ContextMenu::entryAt(float x, float y) const {
    if (!menu_bounds_.contains(x, y)) return -1;

    const float local_y = y - menu_bounds_.top();

    for (size_t i = 0; i < layouts_.size(); ++i) {
        const auto& layout = layouts_[i];
        if (local_y >= layout.bounds.top() && local_y <= layout.bounds.bottom()) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void ContextMenu::renderEntry(SkCanvas* canvas, const Theme& theme, size_t index,
                               const MenuEntry& entry, const SkFont& font) {
    const auto& layout = layouts_[index];
    SkRect bounds = layout.bounds;
    bounds.offset(0.0f, menu_bounds_.top());
    bounds.set(SkRect::MakeXYWH(menu_bounds_.x(), bounds.y(),
                                 menu_bounds_.width(), bounds.height()));

    if (entry.separator) {
        const float sep_y = bounds.centerY();
        SkPaint sep_paint;
        sep_paint.setAntiAlias(true);
        sep_paint.setColor(theme.palette().border);
        sep_paint.setStyle(SkPaint::kStroke_Style);
        sep_paint.setStrokeWidth(0.5f);
        canvas->drawLine(menu_bounds_.x() + kPaddingX, sep_y,
                         menu_bounds_.right() - kPaddingX, sep_y, sep_paint);
        return;
    }

    if (static_cast<int>(index) == selected_index_ || static_cast<int>(index) == hovered_index_) {
        SkRect hover_bounds = SkRect::MakeXYWH(
            menu_bounds_.x() + 4.0f, bounds.y() + 1.0f,
            menu_bounds_.width() - 8.0f, bounds.height() - 2.0f
        );

        SkPaint hover_paint;
        hover_paint.setAntiAlias(true);
        hover_paint.setColor(theme.palette().surface_hover);

        SkRRect hover_rrect = SkRRect::MakeRectXY(hover_bounds, kCornerRadius * 0.5f, kCornerRadius * 0.5f);
        canvas->drawRRect(hover_rrect, hover_paint);
    }

    SkPaint text_paint;
    text_paint.setAntiAlias(true);
    text_paint.setColor(entry.enabled ? theme.palette().text_primary : theme.palette().text_secondary);

    const float text_x = menu_bounds_.x() + kPaddingX + kActionIndent;
    const float text_y = bounds.y() + kEntryHeight * 0.65f;

    canvas->drawString(entry.label.c_str(), text_x, text_y, font, text_paint);

    if (!entry.children.empty()) {
        SkPaint arrow_paint;
        arrow_paint.setAntiAlias(true);
        arrow_paint.setColor(theme.palette().text_secondary);
        arrow_paint.setStyle(SkPaint::kStroke_Style);
        arrow_paint.setStrokeWidth(1.5f);
        arrow_paint.setStrokeCap(SkPaint::kRound_Cap);

        const float arrow_x = menu_bounds_.right() - kPaddingX - 10.0f;
        const float arrow_cy = bounds.centerY();

        SkPath arrow;
        arrow.moveTo(arrow_x, arrow_cy - 4.0f);
        arrow.lineTo(arrow_x + 5.0f, arrow_cy);
        arrow.lineTo(arrow_x, arrow_cy + 4.0f);
        canvas->drawPath(arrow, arrow_paint);
    }

    if (entry.action == MenuAction::Delete) {
        SkPaint delete_paint;
        delete_paint.setAntiAlias(true);
        delete_paint.setColor(theme.palette().error);

        canvas->drawString(entry.label.c_str(), text_x, text_y, font, delete_paint);
    }
}

} // namespace dd
