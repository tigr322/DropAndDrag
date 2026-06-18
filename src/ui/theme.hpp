#pragma once

// theme.hpp — Theme: color palettes for light/dark/auto modes.
// Auto resolves to Dark or Light at render time by querying the OS appearance.

#include <include/core/SkColor.h>

namespace dd {

enum class ThemeVariant : uint8_t { Light, Dark, Auto };

struct ThemePalette {
    SkColor background;
    SkColor surface;
    SkColor surface_hover;
    SkColor text_primary;
    SkColor text_secondary;
    SkColor accent;
    SkColor border;
    SkColor shadow;
    SkColor error;
    SkColor success;
    SkColor warning;
    SkColor glass_background;
    SkColor glass_border;
    SkColor drop_indicator;
    SkColor tag_colors[8];
};

class Theme {
public:
    static Theme& instance();
    void setVariant(ThemeVariant v);
    ThemeVariant variant() const { return variant_; }
    const ThemePalette& palette() const { return is_dark_ ? dark_ : light_; }
    bool isDark() const { return is_dark_; }
    void detectSystemTheme();
private:
    Theme();
    ThemeVariant variant_{ThemeVariant::Auto};
    bool is_dark_{false};
    ThemePalette light_;
    ThemePalette dark_;
    void initPalettes();
};

} // namespace dd
