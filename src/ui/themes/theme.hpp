#pragma once

#include <include/core/SkColor.h>

#include <array>
#include <string>
#include <string_view>

namespace dd {

enum class ThemeVariant : uint8_t { Light, Dark };

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    [[nodiscard]] constexpr explicit operator SkColor() const noexcept {
        return SkColorSetARGB(a, r, g, b);
    }

    [[nodiscard]] constexpr Color withAlpha(uint8_t alpha) const noexcept {
        return {r, g, b, alpha};
    }
};

struct ThemePalette {
    Color background;
    Color surface;
    Color surface_hover;
    Color text_primary;
    Color text_secondary;
    Color accent;
    Color border;
    Color shadow;
    std::array<Color, 8> tag_colors;
    Color error;
    Color success;
    Color warning;

    Color glass_background;
    Color glass_border;
    Color glass_shadow;
    Color drop_indicator;
};

class Theme {
public:
    Theme();
    ~Theme() = default;

    Theme(const Theme&) = delete;
    Theme& operator=(const Theme&) = delete;
    Theme(Theme&&) = delete;
    Theme& operator=(Theme&&) = delete;

    [[nodiscard]] static const Theme& current();
    static void setVariant(ThemeVariant variant);
    [[nodiscard]] static ThemeVariant variant() noexcept;

    [[nodiscard]] SkColor getColor(std::string_view key) const;
    [[nodiscard]] bool isDark() const noexcept { return variant_ == ThemeVariant::Dark; }

    void drawBackground(SkCanvas* canvas, const SkRect& bounds) const;

    [[nodiscard]] const ThemePalette& palette() const noexcept { return palette_; }

private:
    void detectSystemTheme();
    void applyPalette();

    ThemeVariant variant_ = ThemeVariant::Dark;
    ThemePalette palette_;
};

} // namespace dd
