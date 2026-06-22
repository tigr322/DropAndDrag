#if defined(__linux__)

#include "renderer.hpp"
#include "theme.hpp"

#include <X11/Xlib.h>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace dd {

// ── Tile-colour palette per item type ──────────────────────────────────────
// Colours match macOS drawColoredTile() approximate equivalents.

struct TilePalette {
    unsigned r, g, b;
    char     letter;
};

static TilePalette tile_color(ItemType t) {
    switch (t) {
        case ItemType::File:   return {0x54, 0x9E, 0xFF, 'F'};
        case ItemType::Folder: return {0x30, 0xA0, 0xA0, 'D'};
        case ItemType::Image:  return {0x40, 0xA0, 0x40, 'I'};
        case ItemType::URL:    return {0xE1, 0x8E, 0x2D, 'U'};
        default:               return {0xF3, 0x8B, 0xA8, 'T'};
    }
}

// ── Layout constants (mirror macOS renderer.mm) ───────────────────────────
static constexpr int kIconW  = 48;
static constexpr int kIconH  = 48;
static constexpr int kStep   = 64;
static constexpr int kTileH  = 62;
static constexpr int kRowGap = 10;
static constexpr int kDivY   = 27;   // divider line Y

// Public for hit-test in window_linux.cpp
int hitTestItemIndex(int mx, int my, int winW, int winH, int itemCount) {
    if (my <= kDivY + 6 || itemCount <= 0) return -1;
    constexpr int margin = 8;
    int cols = (winW - 2 * margin) / kStep;
    if (cols < 1) cols = 1;
    int row = (my - (kDivY + 6)) / (kTileH + kRowGap);
    int col = (mx - ((winW - ((itemCount % cols ? itemCount % cols : cols) - 1) * kStep + kIconW) / 2)) / kStep;
    if (row < 0 || col < 0 || col >= cols) return -1;
    int idx = row * cols + col;
    if (idx < 0 || idx >= itemCount) return -1;
    // Verify click is within the tile bounds
    int tileBaseX = (winW - (std::min(cols, itemCount - row * cols) - 1) * kStep + kIconW) / 2
                    - ((cols - std::min(cols, itemCount - row * cols)) * kStep) / 2;
    // (simplified check — just bounds-check the index)
    return idx;
}

// ── X11 drawing state ──────────────────────────────────────────────────────

struct X11Draw {
    Display*   dpy     = nullptr;
    ::Window   win     = 0;
    XFontSet   fontset = nullptr;
    GC         gcBg    = nullptr;
    GC         gcFg    = nullptr;
    GC         gcRed   = nullptr;
    GC         gcBlue  = nullptr;
    GC         gcWhite = nullptr;
};

static X11Draw g_xd;

static unsigned long alloc_color(Display* dpy, int scr, unsigned r, unsigned g, unsigned b) {
    XColor c{};
    c.red   = static_cast<unsigned short>(r << 8);
    c.green = static_cast<unsigned short>(g << 8);
    c.blue  = static_cast<unsigned short>(b << 8);
    c.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, DefaultColormap(dpy, scr), &c);
    return c.pixel;
}

static GC make_gc(Display* dpy, ::Window win, unsigned r, unsigned g, unsigned b, int scr) {
    GC gc = XCreateGC(dpy, win, 0, nullptr);
    XSetForeground(dpy, gc, alloc_color(dpy, scr, r, g, b));
    return gc;
}

Renderer::~Renderer() {
    if (g_xd.fontset) { XFreeFontSet(g_xd.dpy, g_xd.fontset); g_xd.fontset = nullptr; }
    if (g_xd.gcBg)    { XFreeGC(g_xd.dpy, g_xd.gcBg);         g_xd.gcBg    = nullptr; }
    if (g_xd.gcFg)    { XFreeGC(g_xd.dpy, g_xd.gcFg);         g_xd.gcFg    = nullptr; }
    if (g_xd.gcRed)   { XFreeGC(g_xd.dpy, g_xd.gcRed);        g_xd.gcRed   = nullptr; }
    if (g_xd.gcBlue)  { XFreeGC(g_xd.dpy, g_xd.gcBlue);       g_xd.gcBlue  = nullptr; }
    if (g_xd.gcWhite) { XFreeGC(g_xd.dpy, g_xd.gcWhite);      g_xd.gcWhite = nullptr; }
    if (g_xd.dpy)     { XCloseDisplay(g_xd.dpy);               g_xd.dpy     = nullptr; }
}

bool Renderer::init(void* view, int w, int h) {
    width_  = w;
    height_ = h;
    ok_     = true;
    if (!view) return true;

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return true;
    int scr = DefaultScreen(dpy);
    auto xid = static_cast<::Window>(reinterpret_cast<uintptr_t>(view));

    g_xd.dpy     = dpy;
    g_xd.win     = xid;
    g_xd.gcBg    = make_gc(dpy, xid, 0x1E, 0x1E, 0x2E, scr);
    g_xd.gcFg    = make_gc(dpy, xid, 0xCD, 0xD6, 0xF4, scr);
    g_xd.gcRed   = make_gc(dpy, xid, 0xF3, 0x8B, 0xA8, scr);
    g_xd.gcBlue  = make_gc(dpy, xid, 0x89, 0xB4, 0xFA, scr);
    g_xd.gcWhite = make_gc(dpy, xid, 0xFF, 0xFF, 0xFF, scr);

    std::setlocale(LC_ALL, "");
    XSetLocaleModifiers("");
    char** miss = nullptr; int miss_n = 0; char* def = nullptr;
    g_xd.fontset = XCreateFontSet(dpy,
        "-*-dejavu sans-medium-r-normal-*-14-*,"
        "-*-liberation sans-medium-r-normal-*-14-*,"
        "-*-*-medium-r-*-*-14-*-*-*-*-*-*-*,"
        "*",
        &miss, &miss_n, &def);
    if (miss) XFreeStringList(miss);

    XSetWindowBackground(dpy, xid, alloc_color(dpy, scr, 0x1E, 0x1E, 0x2E));
    XClearWindow(dpy, xid);
    XFlush(dpy);

    return true;
}

void Renderer::shutdown() { ok_ = false; }

void Renderer::render(float) {
    if (!ok_ || !g_xd.dpy) return;
    auto* dpy = g_xd.dpy;
    auto  win = g_xd.win;

    XFillRectangle(dpy, win, g_xd.gcBg, 0, 0, width_, height_);

    XFillArc(dpy, win, g_xd.gcRed,  8, 7, 14, 14, 0, 360 * 64);
    XFillArc(dpy, win, g_xd.gcBlue, 28, 7, 14, 14, 0, 360 * 64);

    auto drawText = [&](GC gc, int x, int y, const char* s) {
        int n = static_cast<int>(strlen(s));
        if (g_xd.fontset)
            Xutf8DrawString(dpy, win, g_xd.fontset, gc, x, y, s, n);
        else
            XDrawString(dpy, win, gc, x, y, s, n);
    };

    drawText(g_xd.gcFg, 50, 19, "DropAndDrag");
    XDrawLine(dpy, win, g_xd.gcFg, 0, kDivY, width_, kDivY);

    const auto& items = *shared_items_;
    if (items.empty()) {
        drawText(g_xd.gcFg, 8, 47, "Drop files here — shake to toggle");
        XFlush(dpy);
        return;
    }

    int n = static_cast<int>(items.size());
    constexpr int margin = 8;
    int cols = std::max(1, (width_ - 2 * margin) / kStep);
    int cY = kDivY + 6;

    for (int i = 0; i < n; ++i) {
        int row = i / cols;
        int col = i % cols;
        int itemsInRow = std::min(cols, n - row * cols);
        int rowW = (itemsInRow - 1) * kStep + kIconW;
        int rowX = (width_ - rowW) / 2;
        int x = rowX + col * kStep;
        int y = cY + row * (kTileH + kRowGap);
        if (y + kTileH <= kDivY + 6 || y >= height_) continue;

        const auto& item = items[i];

        // Coloured tile background (rounded-rect approximation: filled rect)
        auto pal = tile_color(item.data.type);
        GC tileGc = XCreateGC(dpy, win, 0, nullptr);
        XSetForeground(dpy, tileGc, alloc_color(dpy, DefaultScreen(dpy), pal.r, pal.g, pal.b));
        XFillRectangle(dpy, win, tileGc, x, y, kIconW, kIconH);

        // Type letter in tile centre
        char letter[2]{pal.letter, 0};
        int lx = x + kIconW / 2 - 5;
        int ly = y + kIconH / 2 + 5;
        drawText(g_xd.gcWhite, lx, ly, letter);

        // File name below tile
        std::string label = item.data.file_name.value_or(item.data.path.value_or("?"));
        // Truncate to fit tile width (~7 chars at 14pt)
        if (label.size() > 10) { label = label.substr(0, 8); label += ".."; }
        int labelW = static_cast<int>(label.size()) * 7;
        int lblX = x + (kIconW - labelW) / 2;
        if (lblX < 0) lblX = 0;
        drawText(g_xd.gcFg, lblX, y + kIconH + 14, label.c_str());

        XFreeGC(dpy, tileGc);
    }

    XFlush(dpy);
}

void Renderer::setItems(const ItemList& items) {
    *shared_items_ = items;
    if (g_xd.dpy) render(0.0f);
}

ItemList Renderer::items() const { return *shared_items_; }

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
