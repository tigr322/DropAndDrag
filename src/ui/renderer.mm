#import <Cocoa/Cocoa.h>
#include "renderer.hpp"
#include <core/items/item.hpp>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <include/core/SkFont.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRRect.h>
#include <include/core/SkPath.h>
#include <include/effects/SkImageFilters.h>

namespace dd {

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(void* view, int w, int h) {
    width_ = w;
    height_ = h;

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) return false;

    GrMtlBackendContext ctx;
    ctx.fDevice.retain((__bridge void*)device);
    ctx.fQueue.retain((__bridge void*)[device newCommandQueue]);
    gpu_ = GrDirectContexts::MakeMetal(ctx);
    if (!gpu_) return false;

    if (view) {
        NSView* nsview = (__bridge NSView*)view;
        nsview.wantsLayer = YES;
        CAMetalLayer* ml = [CAMetalLayer layer];
        ml.device = device;
        ml.pixelFormat = MTLPixelFormatBGRA8Unorm;
        ml.framebufferOnly = NO;
        nsview.layer = ml;
        metal_layer_ = (__bridge void*)ml;
    }

    resize(w, h);
    ok_ = true;
    return true;
}

void Renderer::resize(int w, int h) {
    width_ = w;
    height_ = h;
    surface_ = nullptr;

    if (!metal_layer_) return;

    CAMetalLayer* ml = (__bridge CAMetalLayer*)metal_layer_;
    ml.drawableSize = CGSizeMake(w, h);

    id<CAMetalDrawable> drawable = [ml nextDrawable];
    if (!drawable) return;

    GrMtlTextureInfo info;
    info.fTexture.retain((__bridge GrMTLHandle)drawable.texture);

    auto rt = GrBackendRenderTargets::MakeMtl(w, h, info);
    surface_ = SkSurfaces::WrapBackendRenderTarget(
        gpu_.get(), rt, kTopLeft_GrSurfaceOrigin,
        kBGRA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
}

void Renderer::shutdown() {
    surface_ = nullptr;
    gpu_ = nullptr;
    metal_layer_ = nullptr;
    ok_ = false;
}

static void draw_shelf(SkCanvas* c, const SkRect& bounds, const ItemList& items,
                       const ThemePalette& p, float time) {
    SkRRect r = SkRRect::MakeRectXY(bounds, 14, 14);

    SkPaint shadow;
    shadow.setAntiAlias(true);
    shadow.setImageFilter(SkImageFilters::Blur(20, 20, nullptr));
    shadow.setColor(SkColorSetA(p.shadow, 0x60));
    c->drawRRect(r.makeOffset(0, 4), shadow);

    SkPaint bg;
    bg.setAntiAlias(true);
    bg.setColor(SkColorSetA(p.glass_background, 0xCC));
    c->drawRRect(r, bg);

    SkPaint border;
    border.setAntiAlias(true);
    border.setStyle(SkPaint::kStroke_Style);
    border.setStrokeWidth(0.5f);
    border.setColor(p.glass_border);
    c->drawRRect(r, border);

    float x = 12, y = 12;
    SkFont font(nullptr, 11);
    SkPaint text;
    text.setColor(p.text_primary);
    text.setAntiAlias(true);

    for (size_t i = 0; i < items.size() && i < 20; ++i) {
        const auto& item = items[i];

        SkRect icon = SkRect::MakeXYWH(x, y, 48, 48);
        SkRRect icon_r = SkRRect::MakeRectXY(icon, 8, 8);

        SkPaint icon_bg;
        icon_bg.setAntiAlias(true);
        icon_bg.setColor((item.data.type == ItemType::Image) ? p.tag_colors[2]
                       : (item.data.type == ItemType::URL) ? p.tag_colors[3]
                       : (item.data.type == ItemType::Text) ? p.tag_colors[0]
                       : p.tag_colors[5]);
        c->drawRRect(icon_r, icon_bg);

        SkPaint icon_text;
        icon_text.setAntiAlias(true);
        icon_text.setColor(SK_ColorWHITE);
        const char* label = "F";
        if (item.data.type == ItemType::Image) label = "I";
        else if (item.data.type == ItemType::URL) label = "U";
        else if (item.data.type == ItemType::Text) label = "T";
        else if (item.data.type == ItemType::Folder) label = "D";

        SkFont icon_font(nullptr, 18);
        SkRect lb;
        icon_font.measureText(label, 1, SkTextEncoding::kUTF8, &lb);
        c->drawString(label, icon.centerX() - lb.centerX(), icon.centerY() + 4, icon_font, icon_text);

        std::string name = item.data.file_name.value_or(
            item.data.title.value_or(
            item.data.text_content.value_or("")));
        if (name.size() > 16) { name = name.substr(0, 14); name += "..."; }

        SkRect nb;
        font.measureText(name.c_str(), name.size(), SkTextEncoding::kUTF8, &nb);
        float nx = icon.x() + (48 - nb.width()) * 0.5f;
        c->drawString(name.c_str(), nx, y + 62, font, text);

        x += 64;
    }
}

void Renderer::render(float dt) {
    if (!ok_ || !surface_) return;
    time_ += dt;

    if (!metal_layer_) return;
    CAMetalLayer* ml = (__bridge CAMetalLayer*)metal_layer_;
    id<CAMetalDrawable> drawable = [ml nextDrawable];
    if (!drawable) return;

    GrMtlTextureInfo info;
    info.fTexture.retain((__bridge GrMTLHandle)drawable.texture);
    auto rt = GrBackendRenderTargets::MakeMtl(width_, height_, info);
    surface_ = SkSurfaces::WrapBackendRenderTarget(
        gpu_.get(), rt, kTopLeft_GrSurfaceOrigin,
        kBGRA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);

    SkCanvas* c = surface_->getCanvas();
    if (!c) return;

    auto& p = Theme::instance().palette();
    c->clear(p.background);

    SkRect bounds = SkRect::MakeXYWH(4, 4, width_ - 8, height_ - 8);
    draw_shelf(c, bounds, items_, p, time_);

    gpu_->flush();
    gpu_->submit();
}

} // namespace dd
