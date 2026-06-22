#if defined(__linux__)

#define STB_IMAGE_IMPLEMENTATION
#include <vendor/stb_image.h>

#include "renderer.hpp"
#include "theme.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <clocale>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>

namespace dd {

// ── Layout constants (mirror macOS renderer.mm) ───────────────────────────
static constexpr int kIconW  = 48;
static constexpr int kIconH  = 48;
static constexpr int kStep   = 64;
static constexpr int kTileH  = 62;
static constexpr int kRowGap = 10;
static constexpr int kDivY   = 27;

// Public for hit-test in window_linux.cpp
int hitTestItemIndex(int mx, int my, int winW, int winH, int itemCount) {
    if (my <= kDivY + 6 || itemCount <= 0) return -1;
    constexpr int margin = 8;
    int cols = (winW - 2 * margin) / kStep;
    if (cols < 1) cols = 1;
    int row = (my - (kDivY + 6)) / (kTileH + kRowGap);
    int idx = row * cols + (mx - ((winW - (std::min(cols, itemCount - row * cols) - 1) * kStep + kIconW) / 2)) / kStep;
    return (idx >= 0 && idx < itemCount) ? idx : -1;
}

// ── Tile-colour fallback palette ──────────────────────────────────────────
struct TilePalette { unsigned r, g, b; char letter; };
static TilePalette tile_color(ItemType t) {
    switch (t) {
        case ItemType::File:   return {0x54, 0x9E, 0xFF, 'F'};
        case ItemType::Folder: return {0x30, 0xA0, 0xA0, 'D'};
        case ItemType::Image:  return {0x40, 0xA0, 0x40, 'I'};
        case ItemType::URL:    return {0xE1, 0x8E, 0x2D, 'U'};
        default:               return {0xF3, 0x8B, 0xA8, 'T'};
    }
}

// ── X11 drawing state ─────────────────────────────────────────────────────
struct X11Draw {
    Display* dpy     = nullptr;
    ::Window win     = 0;
    XFontSet fontset = nullptr;
    GC       gcBg, gcFg, gcRed, gcBlue, gcWhite;
    int      screen  = 0;
    int      depth   = 24;
};
static X11Draw g_xd;

static unsigned long alloc_color(Display* dpy, int scr, unsigned r, unsigned g, unsigned b) {
    XColor c{};
    c.red = static_cast<unsigned short>(r << 8);
    c.green = static_cast<unsigned short>(g << 8);
    c.blue = static_cast<unsigned short>(b << 8);
    c.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, DefaultColormap(dpy, scr), &c);
    return c.pixel;
}

static GC make_gc(Display* dpy, ::Window win, unsigned r, unsigned g, unsigned b, int scr) {
    GC gc = XCreateGC(dpy, win, 0, nullptr);
    XSetForeground(dpy, gc, alloc_color(dpy, scr, r, g, b));
    return gc;
}

// ── Freedesktop icon-theme lookup ─────────────────────────────────────────

static std::string find_icon_named(const std::string& name) {
    static const char* kThemes[] = {
        "Adwaita", "hicolor", "gnome", "breeze", "Breeze",
        "Humanity", "elementary", "Papirus", "Yaru", "Mint-Y", nullptr
    };
    static const char* kSizes[] = {
        "48x48", "64x64", "32x32", "128x128", "256x256",
        "scalable", "symbolic", "48", "64", "32", nullptr
    };
    static const char* kCats[] = {
        "mimetypes", "apps", "places", "devices", "actions", "categories", nullptr
    };

        for (int ti = 0; kThemes[ti]; ++ti) {
            for (int si = 0; kSizes[si]; ++si) {
                for (int ci = 0; kCats[ci]; ++ci) {
                    std::string p = "/usr/share/icons/" + std::string(kThemes[ti]) +
                                    "/" + kSizes[si] + "/" + kCats[ci] + "/" + name + ".png";
                    if (access(p.c_str(), R_OK) == 0) return p;
                }
            }
        }
        // Also try without size prefix
        for (int ti = 0; kThemes[ti]; ++ti) {
            for (int ci = 0; kCats[ci]; ++ci) {
                std::string p = "/usr/share/icons/" + std::string(kThemes[ti]) +
                                "/" + kCats[ci] + "/" + name + ".png";
                if (access(p.c_str(), R_OK) == 0) return p;
            }
        }
    return {};
}

// ── MIME-type helpers ─────────────────────────────────────────────────────

static bool is_image_ext(const std::string& path) {
    static const char* kExts[] = {".png",".jpg",".jpeg",".gif",".bmp",".webp",".tiff",".tif",nullptr};
    for (int i = 0; kExts[i]; ++i) {
        size_t elen = strlen(kExts[i]);
        if (path.size() >= elen &&
            strcasecmp(path.c_str() + path.size() - elen, kExts[i]) == 0)
            return true;
    }
    return false;
}

static std::string ext_to_mime(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(dot);
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    static std::unordered_map<std::string, std::string> kMap = {
        {".txt","text/plain"}, {".md","text/markdown"}, {".html","text/html"},
        {".css","text/css"}, {".js","text/javascript"}, {".py","text/x-python"},
        {".pdf","application/pdf"}, {".zip","application/zip"}, {".tar","application/x-tar"},
        {".gz","application/gzip"}, {".7z","application/x-7z-compressed"},
        {".deb","application/vnd.debian.binary-package"}, {".rpm","application/x-rpm"},
        {".doc","application/msword"}, {".docx","application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {".xls","application/vnd.ms-excel"}, {".ppt","application/vnd.ms-powerpoint"},
        {".mp3","audio/mpeg"}, {".wav","audio/x-wav"}, {".ogg","audio/ogg"},
        {".mp4","video/mp4"}, {".mkv","video/x-matroska"}, {".avi","video/x-msvideo"},
        {".png","image/png"}, {".jpg","image/jpeg"}, {".jpeg","image/jpeg"},
        {".gif","image/gif"}, {".bmp","image/bmp"}, {".webp","image/webp"},
        {".svg","image/svg+xml"}, {".xml","text/xml"}, {".json","application/json"},
        {".cpp","text/x-c++src"}, {".c","text/x-csrc"}, {".h","text/x-chdr"},
        {".hpp","text/x-c++hdr"}, {".py","text/x-python"}, {".sh","text/x-shellscript"},
        {".rs","text/x-rust"}, {".go","text/x-go"},
    };
    auto it = kMap.find(ext);
    return (it != kMap.end()) ? it->second : "application/octet-stream";
}

// MIME type → freedesktop icon names (try in order: specific → generic)
static std::vector<std::string> mime_to_icon_names(const std::string& mime) {
    std::vector<std::string> names;

    // 1. Exact MIME icon name: text/plain → text-plain
    std::string exact = mime;
    for (auto& c : exact) if (c == '/') c = '-';
    names.push_back(exact);

    // 2. Category generic
    if (mime.find("image/") == 0)      names.push_back("image-x-generic");
    else if (mime.find("text/") == 0)  names.push_back("text-x-generic");
    else if (mime.find("audio/") == 0) names.push_back("audio-x-generic");
    else if (mime.find("video/") == 0) names.push_back("video-x-generic");
    else if (mime.find("application/pdf") == 0)
        names.push_back("application-pdf");
    else if (mime.find("application/zip") == 0
          || mime.find("application/gzip") == 0
          || mime.find("application/x-tar") == 0
          || mime.find("application/x-7z") == 0
          || mime.find("application/x-rpm") == 0)
        names.push_back("package-x-generic");
    else if (mime.find("application/") == 0)
        names.push_back("application-x-generic");
    else if (mime.find("inode/") == 0)
        names.push_back("folder");

    // 3. Ultimate fallbacks
    names.push_back("text-x-generic");
    names.push_back("application-x-generic");
    return names;
}

// ── Icon loading + pixmap cache ──────────────────────────────────────────

struct CachedPixmap {
    Pixmap pm;
    int w, h;
};

static std::unordered_map<std::string, CachedPixmap> g_pixmapCache;

// Pre-composite RGBA onto dark background (0x1E,0x1E,0x2E), return XRGB
static std::vector<uint8_t> composite_rgba(const uint8_t* rgba, int w, int h) {
    std::vector<uint8_t> out(w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        uint8_t r = rgba[i*4], g = rgba[i*4+1], b = rgba[i*4+2], a = rgba[i*4+3];
        out[i*4]   = static_cast<uint8_t>((r * a + 0x1E * (255 - a)) / 255);
        out[i*4+1] = static_cast<uint8_t>((g * a + 0x1E * (255 - a)) / 255);
        out[i*4+2] = static_cast<uint8_t>((b * a + 0x2E * (255 - a)) / 255);
        out[i*4+3] = 255;
    }
    return out;
}

// Simple box-filter downscale to fit within max_w×max_h.
// Returns {data, dw, dh} — caller owns the vector.
struct ScaledRGBA {
    std::vector<uint8_t> data;
    int w, h;
};

static ScaledRGBA downscale_rgba(const uint8_t* src, int sw, int sh, int max_w, int max_h) {
    float scale = std::min((float)max_w / sw, (float)max_h / sh);
    if (scale >= 1.0f) {
        return {{src, src + sw * sh * 4}, sw, sh};
    }
    int dw = std::max(1, (int)(sw * scale));
    int dh = std::max(1, (int)(sh * scale));
    std::vector<uint8_t> out(dw * dh * 4);
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            int sx = (int)(x / scale), sy = (int)(y / scale);
            int ex = std::min((int)((x+1) / scale), sw);
            int ey = std::min((int)((y+1) / scale), sh);
            int r=0, g=0, b=0, a=0, cnt=0;
            for (int py = sy; py < ey; ++py)
                for (int px = sx; px < ex; ++px) {
                    int idx = (py * sw + px) * 4;
                    r += src[idx]; g += src[idx+1]; b += src[idx+2]; a += src[idx+3];
                    ++cnt;
                }
            if (cnt) { r/=cnt; g/=cnt; b/=cnt; a/=cnt; }
            int oi = (y * dw + x) * 4;
            out[oi]=r; out[oi+1]=g; out[oi+2]=b; out[oi+3]=a;
        }
    }
    return {std::move(out), dw, dh};
}

// Load an image (icon or thumbnail) and return a composited pixmap
static CachedPixmap load_icon_pixmap(const std::string& img_path) {
    auto it = g_pixmapCache.find(img_path);
    if (it != g_pixmapCache.end()) return it->second;

    int w=0, h=0;
    unsigned char* raw = stbi_load(img_path.c_str(), &w, &h, nullptr, 4);
    if (!raw) return {0,0,0};

    auto scaled = downscale_rgba(raw, w, h, kIconW, kIconH);
    stbi_image_free(raw);

    auto comp = composite_rgba(scaled.data.data(), scaled.w, scaled.h);

    int bpl = scaled.w * 4;
    char* buf = static_cast<char*>(malloc(bpl * scaled.h));
    memcpy(buf, comp.data(), comp.size());
    XImage* xi = XCreateImage(g_xd.dpy, DefaultVisual(g_xd.dpy, g_xd.screen),
                               g_xd.depth, ZPixmap, 0, buf, scaled.w, scaled.h, 32, bpl);
    if (!xi) { free(buf); return {0,0,0}; }

    Pixmap pm = XCreatePixmap(g_xd.dpy, g_xd.win, scaled.w, scaled.h, g_xd.depth);
    GC pm_gc = XCreateGC(g_xd.dpy, pm, 0, nullptr);
    XPutImage(g_xd.dpy, pm, pm_gc, xi, 0, 0, 0, 0, scaled.w, scaled.h);
    XFreeGC(g_xd.dpy, pm_gc);
    XDestroyImage(xi);

    CachedPixmap cp{pm, scaled.w, scaled.h};
    g_pixmapCache[img_path] = cp;
    return cp;
}

// ── Icon resolution for a shelf item ──────────────────────────────────────

static std::string icon_for_item(const Item& item) {
    std::string path = item.data.path.value_or("");

    if (!path.empty()) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            return find_icon_named("folder");
        if (access(path.c_str(), R_OK) == 0 && is_image_ext(path))
            return path;
    }

    std::string mime = ext_to_mime(path);
    static std::string last_missing_mime;
    for (auto& name : mime_to_icon_names(mime)) {
        std::string found = find_icon_named(name);
        if (!found.empty()) return found;
    }
    if (mime != last_missing_mime) {
        fprintf(stderr, "[icon] no theme icon for %s (mime=%s), using fallback tile\n",
                path.empty() ? "?" : path.c_str(), mime.c_str());
        last_missing_mime = mime;
    }
    return {};
}

// ── Renderer lifecycle ───────────────────────────────────────────────────

Renderer::~Renderer() {
    for (auto& [k, v] : g_pixmapCache)
        if (v.pm) XFreePixmap(g_xd.dpy, v.pm);
    g_pixmapCache.clear();
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

    g_xd.dpy    = dpy;
    g_xd.win    = xid;
    g_xd.screen = scr;
    g_xd.depth  = DefaultDepth(dpy, scr);
    g_xd.gcBg   = make_gc(dpy, xid, 0x1E, 0x1E, 0x2E, scr);
    g_xd.gcFg   = make_gc(dpy, xid, 0xCD, 0xD6, 0xF4, scr);
    g_xd.gcRed  = make_gc(dpy, xid, 0xF3, 0x8B, 0xA8, scr);
    g_xd.gcBlue = make_gc(dpy, xid, 0x89, 0xB4, 0xFA, scr);
    g_xd.gcWhite= make_gc(dpy, xid, 0xFF, 0xFF, 0xFF, scr);

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

        std::string icon_path = icon_for_item(item);

        if (!icon_path.empty()) {
            auto cp = load_icon_pixmap(icon_path);
            if (cp.pm) {
                int ix = x + (kIconW - cp.w) / 2;
                int iy = y + (kIconH - cp.h) / 2;
                XCopyArea(dpy, cp.pm, win, g_xd.gcBg, 0, 0, cp.w, cp.h, ix, iy);
            } else {
                goto fallback_tile;
            }
        } else {
fallback_tile:
        {
            auto pal = tile_color(item.data.type);
            GC tileGc = XCreateGC(dpy, win, 0, nullptr);
            XSetForeground(dpy, tileGc, alloc_color(dpy, g_xd.screen, pal.r, pal.g, pal.b));
            XFillRectangle(dpy, win, tileGc, x, y, kIconW, kIconH);
            char letter[2]{pal.letter, 0};
            int lx = x + kIconW / 2 - 5;
            int ly = y + kIconH / 2 + 5;
            drawText(g_xd.gcWhite, lx, ly, letter);
            XFreeGC(dpy, tileGc);
        }
        }

        std::string label = item.data.file_name.value_or(item.data.path.value_or("?"));
        if (label.size() > 10) { label = label.substr(0, 8); label += ".."; }
        int labelW = static_cast<int>(label.size()) * 7;
        int lblX = x + (kIconW - labelW) / 2;
        if (lblX < 0) lblX = 0;
        drawText(g_xd.gcFg, lblX, y + kIconH + 14, label.c_str());
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

// ── Theme ──────────────────────────────────────────────────────────────────

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
