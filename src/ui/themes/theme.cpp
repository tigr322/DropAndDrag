#include "theme.hpp"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>

#include <cstdlib>
#include <cstring>
#include <stdexcept>

#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #include <CoreFoundation/CoreFoundation.h>
    #endif
#elif defined(_WIN32)
    #include <windows.h>
#endif

namespace dd {

namespace {
    constexpr ThemePalette kLightPalette = {
        .background{250, 250, 250, 255},
        .surface{255, 255, 255, 255},
        .surface_hover{240, 240, 240, 255},
        .text_primary{20, 20, 20, 255},
        .text_secondary{120, 120, 120, 255},
        .accent{0, 122, 255, 255},
        .border{220, 220, 220, 255},
        .shadow{0, 0, 0, 40},
        .tag_colors{{
            {255, 59, 48, 200},
            {255, 149, 0, 200},
            {255, 204, 0, 200},
            {52, 199, 89, 200},
            {0, 122, 255, 200},
            {88, 86, 214, 200},
            {175, 82, 222, 200},
            {255, 45, 85, 200},
        }},
        .error{255, 59, 48, 255},
        .success{52, 199, 89, 255},
        .warning{255, 149, 0, 255},
        .glass_background{255, 255, 255, 180},
        .glass_border{255, 255, 255, 60},
        .glass_shadow{0, 0, 0, 30},
        .drop_indicator{0, 122, 255, 180},
    };

    constexpr ThemePalette kDarkPalette = {
        .background{28, 28, 30, 255},
        .surface{44, 44, 46, 255},
        .surface_hover{58, 58, 60, 255},
        .text_primary{242, 242, 247, 255},
        .text_secondary{152, 152, 157, 255},
        .accent{10, 132, 255, 255},
        .border{72, 72, 74, 255},
        .shadow{0, 0, 0, 80},
        .tag_colors{{
            {255, 69, 58, 200},
            {255, 159, 10, 200},
            {255, 214, 10, 200},
            {48, 209, 88, 200},
            {10, 132, 255, 200},
            {94, 92, 230, 200},
            {191, 90, 242, 200},
            {255, 55, 95, 200},
        }},
        .error{255, 69, 58, 255},
        .success{48, 209, 88, 255},
        .warning{255, 159, 10, 255},
        .glass_background{44, 44, 46, 160},
        .glass_border{255, 255, 255, 20},
        .glass_shadow{0, 0, 0, 50},
        .drop_indicator{10, 132, 255, 180},
    };

    static ThemeVariant g_current_variant = ThemeVariant::Dark;
    static Theme g_theme_instance;
} // anonymous namespace

Theme::Theme() {
    detectSystemTheme();
    applyPalette();
}

const Theme& Theme::current() {
    return g_theme_instance;
}

void Theme::setVariant(ThemeVariant variant) {
    if (variant != g_theme_instance.variant_) {
        g_theme_instance.variant_ = variant;
        g_theme_instance.applyPalette();
    }
}

ThemeVariant Theme::variant() noexcept {
    return g_theme_instance.variant_;
}

void Theme::detectSystemTheme() {
#if defined(__APPLE__) && TARGET_OS_MAC
    auto dict = CFPreferencesCopyAppValue(
        CFSTR("AppleInterfaceStyle"), kCFPreferencesCurrentApplication
    );
    if (dict) {
        CFStringRef value = static_cast<CFStringRef>(dict);
        if (CFStringCompare(value, CFSTR("Dark"), 0) == kCFCompareEqualTo) {
            variant_ = ThemeVariant::Dark;
        } else {
            variant_ = ThemeVariant::Light;
        }
        CFRelease(dict);
    } else {
        variant_ = ThemeVariant::Light;
    }
#elif defined(_WIN32)
    HKEY hkey{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hkey) == ERROR_SUCCESS) {
        DWORD value{};
        DWORD size = sizeof(value);
        if (RegQueryValueExW(hkey, L"AppsUseLightTheme", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
            variant_ = (value == 0) ? ThemeVariant::Dark : ThemeVariant::Light;
        }
        RegCloseKey(hkey);
    }
#else
    const char* gtk_theme = std::getenv("GTK_THEME");
    if (gtk_theme && std::strstr(gtk_theme, "dark") != nullptr) {
        variant_ = ThemeVariant::Dark;
    } else {
        variant_ = ThemeVariant::Light;
    }
#endif
}

void Theme::applyPalette() {
    palette_ = (variant_ == ThemeVariant::Dark) ? kDarkPalette : kLightPalette;
}

SkColor Theme::getColor(std::string_view key) const {
    if (key == "background") return static_cast<SkColor>(palette_.background);
    if (key == "surface") return static_cast<SkColor>(palette_.surface);
    if (key == "surface_hover") return static_cast<SkColor>(palette_.surface_hover);
    if (key == "text_primary") return static_cast<SkColor>(palette_.text_primary);
    if (key == "text_secondary") return static_cast<SkColor>(palette_.text_secondary);
    if (key == "accent") return static_cast<SkColor>(palette_.accent);
    if (key == "border") return static_cast<SkColor>(palette_.border);
    if (key == "shadow") return static_cast<SkColor>(palette_.shadow);
    if (key == "error") return static_cast<SkColor>(palette_.error);
    if (key == "success") return static_cast<SkColor>(palette_.success);
    if (key == "warning") return static_cast<SkColor>(palette_.warning);
    if (key == "glass_background") return static_cast<SkColor>(palette_.glass_background);
    if (key == "glass_border") return static_cast<SkColor>(palette_.glass_border);
    if (key == "glass_shadow") return static_cast<SkColor>(palette_.glass_shadow);
    if (key == "drop_indicator") return static_cast<SkColor>(palette_.drop_indicator);
    if (key.starts_with("tag_")) {
        size_t idx = std::stoul(std::string(key.substr(4)));
        if (idx < palette_.tag_colors.size()) {
            return static_cast<SkColor>(palette_.tag_colors[idx]);
        }
    }
    return SK_ColorBLACK;
}

void Theme::drawBackground(SkCanvas* canvas, const SkRect& bounds) const {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(static_cast<SkColor>(palette_.background));
    canvas->drawRect(bounds, paint);
}

} // namespace dd
