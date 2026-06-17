#include "theme.hpp"
#import <Foundation/Foundation.h>

namespace dd {

Theme& Theme::instance() {
    static Theme t;
    return t;
}

Theme::Theme() {
    initPalettes();
}

void Theme::initPalettes() {
    light_ = {
        .background = 0xFFF2F2F7,
        .surface = 0xFFFFFFFF,
        .surface_hover = 0xFFE5E5EA,
        .text_primary = 0xFF000000,
        .text_secondary = 0xFF8E8E93,
        .accent = 0xFF007AFF,
        .border = 0xFFD1D1D6,
        .shadow = 0x40000000,
        .error = 0xFFFF3B30,
        .success = 0xFF34C759,
        .warning = 0xFFFF9500,
        .glass_background = 0xCCF2F2F7,
        .glass_border = 0x40FFFFFF,
        .drop_indicator = 0xFF007AFF,
        .tag_colors = {0xFFFF3B30, 0xFFFF9500, 0xFFFFCC00, 0xFF34C759,
                       0xFF5AC8FA, 0xFF007AFF, 0xFF5856D6, 0xFFFF2D55},
    };
    dark_ = {
        .background = 0xFF000000,
        .surface = 0xFF1C1C1E,
        .surface_hover = 0xFF2C2C2E,
        .text_primary = 0xFFFFFFFF,
        .text_secondary = 0xFF98989D,
        .accent = 0xFF0A84FF,
        .border = 0xFF38383A,
        .shadow = 0x60000000,
        .error = 0xFFFF453A,
        .success = 0xFF32D74B,
        .warning = 0xFFFF9F0A,
        .glass_background = 0xCC1C1C1E,
        .glass_border = 0x40FFFFFF,
        .drop_indicator = 0xFF0A84FF,
        .tag_colors = {0xFFFF453A, 0xFFFF9F0A, 0xFFFFD60A, 0xFF32D74B,
                       0xFF64D2FF, 0xFF0A84FF, 0xFF5E5CE6, 0xFFFF2D55},
    };
}

void Theme::setVariant(ThemeVariant v) {
    variant_ = v;
    if (v == ThemeVariant::Dark) is_dark_ = true;
    else if (v == ThemeVariant::Light) is_dark_ = false;
    else detectSystemTheme();
}

void Theme::detectSystemTheme() {
    NSString* style = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
    is_dark_ = (style && [style isEqualToString:@"Dark"]);
}

} // namespace dd
