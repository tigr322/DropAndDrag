#if defined(__linux__)

#include "renderer.hpp"
#include "theme.hpp"

#include <X11/Xlib.h>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace dd {

// ── Linux renderer — Xlib drawing ────────────────────────────────────────────
// Opens a dedicated Display connection for rendering (separate from the event
// connection in window_linux.cpp).  XID is server-global, so the same window
// handle is valid from both connections.

struct X11Draw {
    Display*  dpy     = nullptr;
    ::Window   win     = 0;
    XFontSet  fontset = nullptr;
    GC        gcBg    = nullptr;
    GC        gcFg    = nullptr;
    GC        gcRed   = nullptr;
    GC        gcBlue  = nullptr;
};

static X11Draw g_xd;

static unsigned long xcolor(Display* dpy, int scr, unsigned r, unsigned g, unsigned b) {
    XColor c{};
    c.red   = static_cast<unsigned short>(r << 8);
    c.green = static_cast<unsigned short>(g << 8);
    c.blue  = static_cast<unsigned short>(b << 8);
    c.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, DefaultColormap(dpy, scr), &c);
    return c.pixel;
}

static GC mkgc(Display* dpy, ::Window win, unsigned r, unsigned g, unsigned b, int scr) {
    GC gc = XCreateGC(dpy, win, 0, nullptr);
    XSetForeground(dpy, gc, xcolor(dpy, scr, r, g, b));
    return gc;
}

Renderer::~Renderer() {
    if (g_xd.fontset) { XFreeFontSet(g_xd.dpy, g_xd.fontset); g_xd.fontset = nullptr; }
    if (g_xd.gcBg)    { XFreeGC(g_xd.dpy, g_xd.gcBg);         g_xd.gcBg    = nullptr; }
    if (g_xd.gcFg)    { XFreeGC(g_xd.dpy, g_xd.gcFg);         g_xd.gcFg    = nullptr; }
    if (g_xd.gcRed)   { XFreeGC(g_xd.dpy, g_xd.gcRed);        g_xd.gcRed   = nullptr; }
    if (g_xd.gcBlue)  { XFreeGC(g_xd.dpy, g_xd.gcBlue);       g_xd.gcBlue  = nullptr; }
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

    g_xd.dpy    = dpy;
    g_xd.win    = xid;
    g_xd.gcBg   = mkgc(dpy, xid, 0x1E, 0x1E, 0x2E, scr);
    g_xd.gcFg   = mkgc(dpy, xid, 0xCD, 0xD6, 0xF4, scr);
    g_xd.gcRed  = mkgc(dpy, xid, 0xF3, 0x8B, 0xA8, scr);
    g_xd.gcBlue = mkgc(dpy, xid, 0x89, 0xB4, 0xFA, scr);

    // Font set for UTF-8 text rendering (Xutf8DrawString).
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

    XSetWindowBackground(dpy, xid, xcolor(dpy, scr, 0x1E, 0x1E, 0x2E));
    XClearWindow(dpy, xid);
    XFlush(dpy);

    return true;
}

void Renderer::shutdown() { ok_ = false; }

void Renderer::render(float) {
    if (!ok_ || !g_xd.dpy) return;
    Display* dpy = g_xd.dpy;
    ::Window  win = g_xd.win;

    XFillRectangle(dpy, win, g_xd.gcBg, 0, 0, width_, height_);

    // Buttons: red = hide (x 8-22), blue = clear (x 28-42)
    XFillArc(dpy, win, g_xd.gcRed,  8, 7, 14, 14, 0, 360 * 64);
    XFillArc(dpy, win, g_xd.gcBlue, 28, 7, 14, 14, 0, 360 * 64);

    // Helper: draw UTF-8 text (falls back to XDrawString if no font set)
    auto utf8 = [&](GC gc, int x, int y, const char* s) {
        int n = static_cast<int>(strlen(s));
        if (g_xd.fontset)
            Xutf8DrawString(dpy, win, g_xd.fontset, gc, x, y, s, n);
        else
            XDrawString(dpy, win, gc, x, y, s, n);
    };

    utf8(g_xd.gcFg, 50, 19, "DropAndDrag");

    // Divider
    XDrawLine(dpy, win, g_xd.gcFg, 0, 27, width_, 27);

    const auto& items = *shared_items_;
    if (items.empty()) {
        utf8(g_xd.gcFg, 8, 47, "Drop files here - shake to toggle");
    } else {
        int y = 44;
        for (const auto& item : items) {
            const std::string nm = item.data.file_name.value_or(item.data.path.value_or("?"));
            utf8(g_xd.gcFg, 8, y, nm.c_str());
            y += 18;
            if (y > height_ - 6) break;
        }
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
