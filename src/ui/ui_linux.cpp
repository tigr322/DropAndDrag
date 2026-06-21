#if defined(__linux__)

#include "renderer.hpp"
#include "theme.hpp"

#include <cstdlib>
#include <string_view>

namespace dd {

// ── Renderer ─────────────────────────────────────────────────────────────────

Renderer::~Renderer() {}

bool Renderer::init(void* /*view*/, int w, int h) {
    width_  = w;
    height_ = h;
    ok_     = true;
    return true;
}

void Renderer::shutdown() { ok_ = false; }

void Renderer::render(float /*dt*/) {}

void Renderer::setItems(const ItemList& items) {
    *shared_items_ = items;
}

ItemList Renderer::items() const {
    return *shared_items_;
}

void Renderer::setClearCallback(std::function<void()> cb) {
    *clearCallback_ = std::move(cb);
}

void Renderer::setHideCallback(std::function<void()> cb) {
    *hideCallback_ = std::move(cb);
}

// ── Theme ─────────────────────────────────────────────────────────────────────

Theme& Theme::instance() {
    static Theme t;
    return t;
}

Theme::Theme() {
    initPalettes();
    detectSystemTheme();
}

void Theme::setVariant(ThemeVariant v) {
    variant_ = v;
    if (v == ThemeVariant::Dark)  is_dark_ = true;
    if (v == ThemeVariant::Light) is_dark_ = false;
}

void Theme::detectSystemTheme() {
    const char* env = std::getenv("DARKMODE");
    is_dark_ = env && std::string_view(env) == "1";
}

void Theme::initPalettes() {
    // Dark palette
    dark_.background        = 0xFF1E1E2E;
    dark_.surface           = 0xFF2A2A3E;
    dark_.surface_hover     = 0xFF3A3A50;
    dark_.text_primary      = 0xFFCDD6F4;
    dark_.text_secondary    = 0xFF9399B2;
    dark_.accent            = 0xFF89B4FA;
    dark_.border            = 0xFF45475A;
    dark_.shadow            = 0xCC000000;
    dark_.error             = 0xFFF38BA8;
    dark_.success           = 0xFFA6E3A1;
    dark_.warning           = 0xFFF9E2AF;
    dark_.glass_background  = 0xCC1E1E2E;
    dark_.glass_border      = 0x8845475A;
    dark_.drop_indicator    = 0xFF89B4FA;
    for (int i = 0; i < 8; ++i)
        dark_.tag_colors[i] = 0xFF89B4FA;

    // Light palette
    light_.background       = 0xFFEFF1F5;
    light_.surface          = 0xFFFFFFFF;
    light_.surface_hover    = 0xFFE6E9EF;
    light_.text_primary     = 0xFF4C4F69;
    light_.text_secondary   = 0xFF6C6F85;
    light_.accent           = 0xFF1E66F5;
    light_.border           = 0xFFCCD0DA;
    light_.shadow           = 0x33000000;
    light_.error            = 0xFFD20F39;
    light_.success          = 0xFF40A02B;
    light_.warning          = 0xFFDF8E1D;
    light_.glass_background = 0xCCEFF1F5;
    light_.glass_border     = 0x88CCD0DA;
    light_.drop_indicator   = 0xFF1E66F5;
    for (int i = 0; i < 8; ++i)
        light_.tag_colors[i] = 0xFF1E66F5;
}

} // namespace dd

#endif // __linux__
