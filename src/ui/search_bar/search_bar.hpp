#pragma once

#include "src/ui/components/component.hpp"

#include <include/core/SkCanvas.h>
#include <include/core/SkRRect.h>

#include <memory>
#include <string>

namespace dd {

class Theme;
class FadeAnimation;

enum class FilterType : uint8_t {
    All,
    Files,
    Images,
    Text,
    URLs,
};

class SearchBar : public Component {
public:
    SearchBar();
    ~SearchBar() override;

    void render(SkCanvas* canvas, const Rect& bounds) override;
    void render(SkCanvas* canvas, const SkRect& bounds, const Theme& theme);
    void render(SkCanvas* canvas, const Rect& bounds, const Theme& theme);

    bool handleEvent(const MouseEvent& event) override;
    bool handleKeyEvent(const KeyEvent& event) override;

    void handleTextInput(std::string_view text);
    [[nodiscard]] const std::string& query() const noexcept { return query_; }
    void clear();

    void setFilter(FilterType filter);
    [[nodiscard]] FilterType activeFilter() const noexcept { return active_filter_; }

    void activate();
    void deactivate();

    [[nodiscard]] bool isActive() const noexcept { return active_; }

    void layout(const Rect& bounds) override;
    [[nodiscard]] Size getDesiredSize() const override;

private:
    void drawBackground(SkCanvas* canvas, const SkRect& bounds, const Theme& theme);
    void drawSearchIcon(SkCanvas* canvas, const SkRect& bounds, const Theme& theme);
    void drawTextInput(SkCanvas* canvas, const SkRect& bounds, const Theme& theme);
    void drawCursor(SkCanvas* canvas, const SkRect& bounds, const Theme& theme);
    void drawClearButton(SkCanvas* canvas, const SkRect& bounds, const Theme& theme);
    void drawFilterButtons(SkCanvas* canvas, const SkRect& bounds, const Theme& theme);
    void drawFilterIcon(SkCanvas* canvas, const SkRect& bounds, const Theme& theme,
                        FilterType filter, const char* label, bool is_active);

    [[nodiscard]] FilterType filterAt(float x, float y) const;

    std::string query_;
    FilterType active_filter_ = FilterType::All;

    bool active_ = false;
    bool cursor_visible_ = true;
    float cursor_blink_timer_ = 0.0f;
    bool clear_button_hovered_ = false;
    bool clear_button_pressed_ = false;

    int filter_hovered_ = -1;

    std::unique_ptr<FadeAnimation> expand_animation_;

    static constexpr float kBarHeight = 36.0f;
    static constexpr float kCornerRadius = 18.0f;
    static constexpr float kSearchIconSize = 16.0f;
    static constexpr float kClearButtonSize = 16.0f;
    static constexpr float kFilterButtonSize = 24.0f;
    static constexpr float kTextLeftPadding = 36.0f;
    static constexpr float kTextRightPadding = 36.0f;
    static constexpr float kCursorWidth = 1.5f;
    static constexpr float kCursorBlinkInterval = 530.0f;
};

} // namespace dd
