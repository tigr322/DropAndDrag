#pragma once

#include "src/ui/components/component.hpp"

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkRRect.h>

#include <memory>
#include <string>

namespace dd {

struct Item;
class SkiaContext;
class FadeAnimation;
class ScaleAnimation;

class ItemView : public Component {
public:
    ItemView();
    ~ItemView() override;

    void init(SkiaContext* skia_context);

    void render(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void render(SkCanvas* canvas, const Rect& bounds) override;
    bool handleEvent(const MouseEvent& event) override;

    void layout(const Rect& bounds) override;

    [[nodiscard]] bool hitTest(int x, int y) const noexcept {
        return bounds_.contains(static_cast<float>(x), static_cast<float>(y));
    }

    void startAddAnimation();
    void startRemoveAnimation();

    enum class State : uint8_t {
        Normal,
        Hovered,
        Selected,
        Dragging,
    };

    void setState(State state) noexcept { state_ = state; }
    [[nodiscard]] State state() const noexcept { return state_; }

private:
    void drawFileIcon(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void drawThumbnail(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void drawFileName(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void drawFileSize(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void drawTypeBadge(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void drawFavoriteStar(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void drawSelectionHighlight(SkCanvas* canvas, const Rect& bounds);
    void drawTags(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void drawTextPreview(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void drawURLPreview(SkCanvas* canvas, const Rect& bounds, const Item& item);
    void drawBackground(SkCanvas* canvas, const Rect& bounds);

    void loadThumbnailAsync(const Item& item);
    [[nodiscard]] std::string formatFileSize(uint64_t bytes) const;
    [[nodiscard]] std::string ellipsizeText(const std::string& text, float max_width, const SkFont& font) const;

    SkiaContext* skia_context_ = nullptr;
    State state_ = State::Normal;

    sk_sp<SkImage> cached_thumbnail_;
    sk_sp<SkImage> cached_icon_;
    sk_sp<SkImage> cached_favicon_;
    std::string cached_item_uuid_;
    bool thumbnail_loading_ = false;

    std::unique_ptr<FadeAnimation> fade_animation_;
    std::unique_ptr<ScaleAnimation> scale_animation_;
    float anim_scale_ = 1.0f;
    float anim_opacity_ = 1.0f;

    static constexpr float kCornerRadius = 8.0f;
    static constexpr float kIconPadding = 4.0f;
    static constexpr float kBadgeSize = 20.0f;
    static constexpr float kTagDotRadius = 4.0f;
    static constexpr float kStarSize = 16.0f;
    static constexpr int kMaxNameLines = 2;
    static constexpr int kMaxTextChars = 100;
};

} // namespace dd
