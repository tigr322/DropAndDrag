// renderer.mm — Objective-C++ implementation of the Renderer C++ class.
//
// This file is the entire UI rendering layer for macOS.  It is intentionally
// a single file: all globals, helpers, and callbacks live here to keep the
// ObjC/C++ interop boundary in one place.
//
// Rendering model:
//   • Reactive, not polling — drawRect: fires only when setNeedsDisplay:YES is
//     called explicitly.  No CADisplayLink, no 60Hz timer.
//   • drawShelf() is a pure function (reads globals, no side-effects) that is
//     called from DDDragView::drawRect: via _ddDrawBlock.
//
// Global state (main-thread only, no locking needed):
//   g_view         — the content NSView (DDDragView)
//   g_w / g_h      — current logical dimensions
//   g_tileRects    — item bounding rects updated every draw; used for hit-test
//   g_*BtnRect     — button rects updated every draw; used for hit-test
//   g_selectedIndices — current selection set
//   g_scrollOffsetY   — current scroll position in points
//
// Caches (main-thread only, never locked):
//   g_iconCache    — filesystem path → NSImage (system icon via NSWorkspace)
//   g_thumbCache   — item uuid → NSImage (QuickLook 96px @2x thumbnail)
//   g_faviconCache — domain string → NSImage (Google favicon service)
//   g_labelCache   — item uuid → {truncated NSString, pre-measured width}
//   g_bgPath       — CGPathRef for the background rounded-rect (rebuilt on resize)
//
// All cache keys are std::string to avoid per-frame NSString allocation.
// See ARCH_AUDIT.md §6 for the full rendering pipeline.

#import <Cocoa/Cocoa.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import "../platform_impl/macos/dd_renderable.h"
#include "renderer.hpp"
#include <core/items/item.hpp>
#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

// ─── File-scope globals ───────────────────────────────────────────────────────

static NSView* g_view = nil;
static int     g_w    = 400;
static int     g_h    = 120;

// Updated by drawShelf every frame; read by all interaction blocks.
// Main-thread only — no locking needed.
static std::vector<NSRect> g_tileRects;
static NSRect              g_clearBtnRect = {};
static NSRect              g_hideBtnRect  = {};
static std::set<size_t>    g_selectedIndices;

// Scroll state — updated by drawShelf, mutated by ddScrollBlock.
static CGFloat g_scrollOffsetY   = 0.0;
static CGFloat g_maxScrollOffset = 0.0;
static CGFloat g_contentClipY0   = 32.0;

// ─── Image caches — C++ maps to avoid per-frame NSString key allocations ──────
// All accessed on the main thread only (completion handlers dispatch_async back).

static std::unordered_map<std::string, NSImage*> g_iconCache;    // path   → system icon
static std::unordered_map<std::string, NSImage*> g_thumbCache;   // uuid   → QL thumbnail
static std::unordered_map<std::string, NSImage*> g_faviconCache; // domain → favicon
static std::unordered_set<std::string>           g_pendingThumbs;
static std::unordered_set<std::string>           g_pendingFavicons;
// At most 4 simultaneous QL thumbnail requests; extras queue behind the semaphore.
static dispatch_semaphore_t g_thumbSemaphore;

// ─── Label cache — invalidated on every setItems() ───────────────────────────
// Stores the truncated display string and its pre-measured pixel width so
// sizeWithAttributes: (a full text-layout pass) runs at most once per item
// rather than once per tile per frame.

struct LabelEntry { NSString* text; CGFloat width; };
static std::unordered_map<std::string, LabelEntry> g_labelCache;

// ─── Background path cache — rebuilt only when the window is resized ──────────

static CGPathRef g_bgPath   = nullptr;
static CGRect    g_bgBounds = {};

// ─── systemIconForPath ────────────────────────────────────────────────────────

static NSImage* systemIconForPath(NSString* path) {
    std::string key = (path.length > 0) ? path.UTF8String : "__generic__";
    auto it = g_iconCache.find(key);
    if (it != g_iconCache.end()) return it->second;
    NSImage* icon = (path.length > 0)
        ? [[NSWorkspace sharedWorkspace] iconForFile:path]
        : [[NSWorkspace sharedWorkspace] iconForFileType:@"public.data"];
    if (!icon) icon = [NSImage imageNamed:NSImageNameMultipleDocuments];
    if (icon) g_iconCache[key] = icon;
    return icon;
}

// ─── requestThumbnail ─────────────────────────────────────────────────────────

static void requestThumbnail(NSString* filePath, NSString* uuid) {
    std::string uuidKey = uuid.UTF8String;
    if (g_pendingThumbs.count(uuidKey)) return;
    g_pendingThumbs.insert(uuidKey);

    if (@available(macOS 10.15, *)) {
        NSURL* url = [NSURL fileURLWithPath:filePath];
        QLThumbnailGenerationRequest* req =
            [[QLThumbnailGenerationRequest alloc]
                initWithFileAtURL:url
                             size:CGSizeMake(96, 96)
                            scale:2.0
                representationTypes:QLThumbnailGenerationRequestRepresentationTypeThumbnail];
        // Wait behind the semaphore on a background thread so the main thread
        // is never blocked. At most 4 QL requests run at once.
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
            dispatch_semaphore_wait(g_thumbSemaphore, DISPATCH_TIME_FOREVER);
            [[QLThumbnailGenerator sharedGenerator]
                generateBestRepresentationForRequest:req
                completionHandler:^(QLThumbnailRepresentation* rep, NSError* err) {
                    dispatch_semaphore_signal(g_thumbSemaphore);
                    NSImage* img = (!err && rep) ? rep.NSImage : nil;
                    dispatch_async(dispatch_get_main_queue(), ^{
                        if (img) {
                            [img setSize:NSMakeSize(48, 48)];
                            g_thumbCache[uuidKey] = img;
                        } else {
                            g_pendingThumbs.erase(uuidKey);
                        }
                        if (g_view) [g_view setNeedsDisplay:YES];
                    });
                }];
        });
    } else {
        g_pendingThumbs.erase(uuidKey);
    }
}

// ─── requestFavicon ───────────────────────────────────────────────────────────

static void requestFavicon(NSString* domain) {
    std::string domainKey = domain.UTF8String;
    if (g_pendingFavicons.count(domainKey)) return;
    g_pendingFavicons.insert(domainKey);

    NSURL* url = [NSURL URLWithString:
        [NSString stringWithFormat:
            @"https://www.google.com/s2/favicons?domain=%@&sz=64", domain]];
    [[[NSURLSession sharedSession]
        dataTaskWithURL:url
        completionHandler:^(NSData* data, NSURLResponse*, NSError* err) {
            NSImage* img = (!err && data.length > 0)
                ? [[NSImage alloc] initWithData:data] : nil;
            dispatch_async(dispatch_get_main_queue(), ^{
                if (img) g_faviconCache[domainKey] = img;
                else     g_pendingFavicons.erase(domainKey);
                if (g_view) [g_view setNeedsDisplay:YES];
            });
        }] resume];
}

// ─── iconForItem ──────────────────────────────────────────────────────────────

static NSImage* iconForItem(const dd::Item& item) {
    switch (item.data.type) {
        case dd::ItemType::File:
        case dd::ItemType::Folder: {
            NSString* path = item.data.path.has_value()
                ? [NSString stringWithUTF8String:item.data.path->c_str()]
                : @"";
            return systemIconForPath(path);
        }
        case dd::ItemType::Image: {
            auto it = g_thumbCache.find(item.data.uuid);
            if (it != g_thumbCache.end()) return it->second;
            if (item.data.path.has_value())
                requestThumbnail(
                    [NSString stringWithUTF8String:item.data.path->c_str()],
                    [NSString stringWithUTF8String:item.data.uuid.c_str()]);
            NSString* path = item.data.path.has_value()
                ? [NSString stringWithUTF8String:item.data.path->c_str()]
                : @"";
            return systemIconForPath(path);
        }
        case dd::ItemType::URL: {
            NSString* urlStr = nil;
            if (item.data.path.has_value())
                urlStr = [NSString stringWithUTF8String:item.data.path->c_str()];
            else if (item.data.text_content.has_value())
                urlStr = [NSString stringWithUTF8String:item.data.text_content->c_str()];
            if (urlStr.length > 0) {
                NSString* hostNS = [NSURL URLWithString:urlStr].host;
                if (hostNS.length > 0) {
                    std::string domain = hostNS.UTF8String;
                    auto it = g_faviconCache.find(domain);
                    if (it != g_faviconCache.end()) return it->second;
                    requestFavicon(hostNS);
                }
            }
            return [NSImage imageNamed:NSImageNameNetwork];
        }
        case dd::ItemType::Text:
        default:
            return nil;
    }
}

// ─── drawColoredTile ─────────────────────────────────────────────────────────

static void drawColoredTile(NSRect r, const dd::Item& item) {
    const char* type = "T";
    NSColor* col = [NSColor systemRedColor];
    switch (item.data.type) {
        case dd::ItemType::File:   type = "F"; col = [NSColor systemBlueColor];   break;
        case dd::ItemType::Folder: type = "D"; col = [NSColor systemTealColor];   break;
        case dd::ItemType::Image:  type = "I"; col = [NSColor systemGreenColor];  break;
        case dd::ItemType::URL:    type = "U"; col = [NSColor systemOrangeColor]; break;
        default: break;
    }
    NSBezierPath* bg = [NSBezierPath bezierPathWithRoundedRect:r xRadius:8 yRadius:8];
    [col setFill]; [bg fill];
    // Static — allocated once, not on every frame while thumbnails are loading.
    static NSDictionary* a;
    if (!a) a = @{ NSFontAttributeName:            [NSFont boldSystemFontOfSize:18],
                   NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSString* lbl = [NSString stringWithUTF8String:type];
    NSSize sz = [lbl sizeWithAttributes:a];
    [lbl drawAtPoint:NSMakePoint(NSMinX(r) + (48 - sz.width)  / 2,
                                  NSMinY(r) + (48 - sz.height) / 2 - 2)
       withAttributes:a];
}

// ─── Geometry helpers ─────────────────────────────────────────────────────────

static const CGFloat kStep  = 64.0;
static const CGFloat kIconW = 48.0;
static const CGFloat kIconH = 48.0;
static const CGFloat kTileH  = 62.0;
static const CGFloat kRowGap = 10.0;
static const CGFloat kBtnSz  = 18.0;
static const CGFloat kBtnMg = 7.0;

// Returns the index of the item whose tile contains pt, or -1 if none.
// Uses kTileH (icon + label area) so clicks on the label below an icon still
// register. Clicks above g_contentClipY0 (the button row) are excluded.
static NSInteger hitTestItemAt(NSPoint pt, NSUInteger count) {
    if (count == 0 || g_tileRects.size() < count) return -1;
    if (pt.y < g_contentClipY0) return -1;
    for (NSUInteger i = 0; i < count; ++i) {
        NSRect r = NSMakeRect(g_tileRects[i].origin.x, g_tileRects[i].origin.y,
                              kIconW, kTileH);
        if (NSPointInRect(pt, r)) return (NSInteger)i;
    }
    return -1;
}

// ─── drawShelf ────────────────────────────────────────────────────────────────

static void drawShelf(CGContextRef ctx, CGRect bounds, const dd::ItemList& items) {
    // ── Static attribute dictionaries (allocated once, never per-frame) ───────
    static NSDictionary* labelAttrs;
    if (!labelAttrs) labelAttrs = @{
        NSFontAttributeName:            [NSFont systemFontOfSize:10],
        NSForegroundColorAttributeName: [NSColor colorWithWhite:0.9 alpha:1.0]
    };
    static NSDictionary* hintAttrs;
    if (!hintAttrs) hintAttrs = @{
        NSFontAttributeName:            [NSFont systemFontOfSize:13],
        NSForegroundColorAttributeName: [NSColor colorWithWhite:0.75 alpha:0.55]
    };

    // ── Background (CGPath cached, rebuilt only on resize) ────────────────────
    CGContextSetRGBFillColor(ctx, 0.0, 0.0, 0.0, 0.55);
    if (!g_bgPath || !CGRectEqualToRect(bounds, g_bgBounds)) {
        if (g_bgPath) CGPathRelease(g_bgPath);
        CGMutablePathRef mp = CGPathCreateMutable();
        CGPathAddRoundedRect(mp, nullptr, CGRectInset(bounds, 2, 2), 14, 14);
        g_bgPath   = CGPathCreateCopy(mp);
        CGPathRelease(mp);
        g_bgBounds = bounds;
    }
    CGContextAddPath(ctx, g_bgPath);
    CGContextFillPath(ctx);

    NSUInteger n = items.size();

    // ── Hide button — top-left amber circle ───────────────────────────────────
    g_hideBtnRect = NSMakeRect(kBtnMg, kBtnMg, kBtnSz, kBtnSz);
    {
        NSBezierPath* circle = [NSBezierPath bezierPathWithOvalInRect:g_hideBtnRect];
        [[NSColor colorWithRed:0.98 green:0.74 blue:0.18 alpha:0.90] setFill];
        [circle fill];
        CGContextSaveGState(ctx);
        CGContextSetStrokeColorWithColor(ctx,
            [[NSColor colorWithRed:0.55 green:0.40 blue:0.02 alpha:0.85] CGColor]);
        CGContextSetLineWidth(ctx, 1.5);
        CGContextSetLineCap(ctx, kCGLineCapRound);
        CGFloat cx = NSMidX(g_hideBtnRect), cy = NSMidY(g_hideBtnRect);
        CGContextMoveToPoint(ctx, cx - 3.5, cy);
        CGContextAddLineToPoint(ctx, cx + 3.5, cy);
        CGContextStrokePath(ctx);
        CGContextRestoreGState(ctx);
    }

    // ── Clear button — top-right gray circle (only when items present) ────────
    g_clearBtnRect = (n > 0)
        ? NSMakeRect(bounds.size.width - kBtnSz - kBtnMg, kBtnMg, kBtnSz, kBtnSz)
        : NSZeroRect;

    if (n > 0) {
        NSBezierPath* circle = [NSBezierPath bezierPathWithOvalInRect:g_clearBtnRect];
        [[NSColor colorWithWhite:1.0 alpha:0.18] setFill];
        [circle fill];
        CGContextSaveGState(ctx);
        CGContextSetStrokeColorWithColor(ctx,
            [[NSColor colorWithWhite:0.9 alpha:0.85] CGColor]);
        CGContextSetLineWidth(ctx, 1.5);
        CGContextSetLineCap(ctx, kCGLineCapRound);
        CGFloat cx = NSMidX(g_clearBtnRect), cy = NSMidY(g_clearBtnRect), d = 3.5;
        CGContextMoveToPoint(ctx, cx - d, cy - d); CGContextAddLineToPoint(ctx, cx + d, cy + d);
        CGContextMoveToPoint(ctx, cx + d, cy - d); CGContextAddLineToPoint(ctx, cx - d, cy + d);
        CGContextStrokePath(ctx);
        CGContextRestoreGState(ctx);
    }

    // ── Scroll-aware multi-row layout ─────────────────────────────────────────
    {
        const CGFloat margin    = 12.0;
        const CGFloat bottomPad = 14.0;
        NSInteger cols = (NSInteger)((bounds.size.width - 2.0 * margin) / kStep);
        if (cols < 1) cols = 1;
        NSInteger rows      = ((NSInteger)n + cols - 1) / cols;
        CGFloat topReserved = kBtnMg + kBtnSz + kBtnMg;
        CGFloat groupH      = (CGFloat)rows * kTileH + (CGFloat)(rows - 1) * kRowGap;
        CGFloat visibleH    = bounds.size.height - topReserved - bottomPad;

        g_contentClipY0 = topReserved;

        CGFloat startY;
        if (groupH <= visibleH) {
            startY            = std::max(topReserved, (bounds.size.height - groupH) / 2.0);
            g_scrollOffsetY   = 0.0;
            g_maxScrollOffset = 0.0;
        } else {
            g_maxScrollOffset = groupH - visibleH;
            g_scrollOffsetY   = std::max(0.0, std::min(g_scrollOffsetY, g_maxScrollOffset));
            startY            = topReserved - g_scrollOffsetY;
        }

        g_tileRects.resize(n);
        for (NSUInteger i = 0; i < n; ++i) {
            NSInteger row = (NSInteger)i / cols;
            NSInteger col = (NSInteger)i % cols;
            NSInteger itemsInRow = (row == rows - 1)
                ? (NSInteger)n - row * cols : cols;
            CGFloat rowW = (CGFloat)(itemsInRow - 1) * kStep + kIconW;
            CGFloat rowX = (bounds.size.width - rowW) / 2.0;
            g_tileRects[i] = NSMakeRect(
                rowX + (CGFloat)col * kStep,
                startY + (CGFloat)row * (kTileH + kRowGap),
                kIconW, kIconH);
        }
    }

    // ── Empty state ───────────────────────────────────────────────────────────
    if (n == 0) {
        static NSString* dropMsg    = @"Drop files here";
        static CGFloat   dropMsgW   = -1;
        static CGFloat   dropMsgH   = -1;
        if (dropMsgW < 0) {
            NSSize s  = [dropMsg sizeWithAttributes:hintAttrs];
            dropMsgW  = s.width;
            dropMsgH  = s.height;
        }
        [dropMsg drawAtPoint:NSMakePoint((bounds.size.width  - dropMsgW) / 2,
                                          (bounds.size.height - dropMsgH) / 2)
               withAttributes:hintAttrs];
        return;
    }

    // ── Item tiles (clipped to the scrollable content area) ───────────────────
    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, CGRectMake(0, g_contentClipY0,
                                         bounds.size.width,
                                         bounds.size.height - g_contentClipY0));

    for (NSUInteger i = 0; i < n; ++i) {
        NSRect iconRect = g_tileRects[i];
        if (iconRect.origin.y + kTileH <= g_contentClipY0) continue;
        if (iconRect.origin.y >= bounds.size.height)        continue;

        const auto& item = items[i];
        bool selected = g_selectedIndices.count(i) > 0;

        if (selected) {
            CGContextSaveGState(ctx);
            CGMutablePathRef selPath = CGPathCreateMutable();
            CGPathAddRoundedRect(selPath, nullptr,
                CGRectInset(NSRectToCGRect(iconRect), -3, -3), 10, 10);
            CGContextAddPath(ctx, selPath);
            CGContextSetRGBFillColor(ctx, 0.20, 0.50, 1.0, 0.30);
            CGContextFillPath(ctx);
            CGPathRelease(selPath);
            CGContextRestoreGState(ctx);
        }

        NSImage* icon = iconForItem(item);
        if (icon) {
            CGContextSaveGState(ctx);
            CGMutablePathRef clip = CGPathCreateMutable();
            CGPathAddRoundedRect(clip, nullptr, NSRectToCGRect(iconRect), 8, 8);
            CGContextAddPath(ctx, clip);
            CGContextClip(ctx);
            CGPathRelease(clip);
            [icon drawInRect:iconRect
                    fromRect:NSZeroRect
                   operation:NSCompositingOperationSourceOver
                    fraction:1.0
              respectFlipped:YES
                       hints:nil];
            CGContextRestoreGState(ctx);
        } else {
            drawColoredTile(iconRect, item);
        }

        if (selected) {
            NSBezierPath* border = [NSBezierPath
                bezierPathWithRoundedRect:NSInsetRect(iconRect, 1.5, 1.5)
                                  xRadius:7 yRadius:7];
            border.lineWidth = 2.5;
            [[NSColor colorWithRed:0.20 green:0.50 blue:1.0 alpha:0.95] setStroke];
            [border stroke];
        }

        // Label — looked up from cache; sizeWithAttributes: is never called
        // here after the first frame (the LabelEntry is populated once then reused).
        auto lit = g_labelCache.find(item.data.uuid);
        if (lit == g_labelCache.end()) {
            std::string name = item.data.file_name.value_or(
                item.data.title.value_or(item.data.text_content.value_or("item")));
            NSString* nStr = [NSString stringWithUTF8String:name.c_str()] ?: @"???";
            if (nStr.length > 16)
                nStr = [[nStr substringToIndex:14] stringByAppendingString:@"…"];
            CGFloat w = [nStr sizeWithAttributes:labelAttrs].width;
            g_labelCache[item.data.uuid] = {nStr, w};
            lit = g_labelCache.find(item.data.uuid);
        }
        const LabelEntry& lbl = lit->second;
        [lbl.text drawAtPoint:NSMakePoint(iconRect.origin.x + (kIconW - lbl.width) / 2,
                                           iconRect.origin.y + kIconH + 2)
               withAttributes:labelAttrs];
    }

    CGContextRestoreGState(ctx); // restore content-area clip

    // ── Scrollbar ─────────────────────────────────────────────────────────────
    if (g_maxScrollOffset > 0.0) {
        const CGFloat trackW    = 4.0;
        const CGFloat trackPad  = 5.0;
        const CGFloat bottomPad = 14.0;
        CGFloat trackX  = bounds.size.width - trackW - trackPad;
        CGFloat trackY0 = g_contentClipY0 + 4.0;
        CGFloat trackH  = bounds.size.height - g_contentClipY0 - bottomPad - 4.0;
        CGFloat thumbH  = std::max(20.0, trackH * trackH / (trackH + g_maxScrollOffset));
        CGFloat thumbY  = trackY0 + (trackH - thumbH) * (g_scrollOffsetY / g_maxScrollOffset);

        CGContextSaveGState(ctx);
        CGMutablePathRef trackPath = CGPathCreateMutable();
        CGPathAddRoundedRect(trackPath, nullptr,
            CGRectMake(trackX, trackY0, trackW, trackH), 2.0, 2.0);
        CGContextAddPath(ctx, trackPath);
        CGContextSetRGBFillColor(ctx, 1.0, 1.0, 1.0, 0.10);
        CGContextFillPath(ctx);
        CGPathRelease(trackPath);

        CGMutablePathRef thumbPath = CGPathCreateMutable();
        CGPathAddRoundedRect(thumbPath, nullptr,
            CGRectMake(trackX, thumbY, trackW, thumbH), 2.0, 2.0);
        CGContextAddPath(ctx, thumbPath);
        CGContextSetRGBFillColor(ctx, 1.0, 1.0, 1.0, 0.42);
        CGContextFillPath(ctx);
        CGPathRelease(thumbPath);
        CGContextRestoreGState(ctx);
    }
}

// ─── Renderer ────────────────────────────────────────────────────────────────

namespace dd {

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(void* view, int w, int h) {
    width_ = w; height_ = h;
    g_w = w; g_h = h;

    // Initialize the thumbnail semaphore once here, not on every drawRect:.
    if (!g_thumbSemaphore) g_thumbSemaphore = dispatch_semaphore_create(4);

    if (view) {
        g_view = (__bridge NSView*)view;

        // Capture shared_ptr copies so each ObjC block holds a strong reference
        // to the heap-allocated value.  This means:
        //   • Renderer::setItems() writes *si = items — the block sees the new list.
        //   • Renderer::setClearCallback() writes *cc = cb — the block sees the new fn.
        //   • Renderer::setHideCallback() writes *hc = cb — the block sees the new fn.
        // Because the blocks only hold the pointer, not the value, they always call
        // the most recently installed callback without needing to be re-registered.
        auto si = shared_items_;
        auto cc = clearCallback_;
        auto hc = hideCallback_;

        if ([g_view conformsToProtocol:@protocol(DDRenderable)]) {
            id<DDRenderable> rv = (id<DDRenderable>)g_view;

            // ── Draw ─────────────────────────────────────────────────────────
            rv.ddDrawBlock = ^(CGContextRef ctx, CGRect boundsIn) {
                drawShelf(ctx, boundsIn, *si);
            };

            // ── Hit-test ──────────────────────────────────────────────────────
            rv.ddHitTestBlock = ^BOOL(NSPoint pt) {
                if (NSPointInRect(pt, g_hideBtnRect))  return YES;
                if (!NSEqualRects(g_clearBtnRect, NSZeroRect) &&
                    NSPointInRect(pt, g_clearBtnRect)) return YES;
                return hitTestItemAt(pt, si->size()) >= 0;
            };

            // ── Click handling ───────────────────────────────────────────────
            rv.ddHandleClickBlock = ^BOOL(NSPoint pt) {
                if (NSPointInRect(pt, g_hideBtnRect)) {
                    if (*hc) (*hc)();
                    return YES;
                }
                if (!NSEqualRects(g_clearBtnRect, NSZeroRect) &&
                    NSPointInRect(pt, g_clearBtnRect) && !si->empty()) {
                    g_selectedIndices.clear();
                    si->clear();
                    if (*cc) (*cc)();
                    if (g_view) [g_view setNeedsDisplay:YES];
                    return YES;
                }
                NSInteger idx = hitTestItemAt(pt, si->size());
                if (idx >= 0) {
                    bool alreadySelected = g_selectedIndices.count((size_t)idx) > 0;
                    bool cmdDown = ([NSEvent modifierFlags] &
                                    NSEventModifierFlagCommand) != 0;
                    if (cmdDown) {
                        if (alreadySelected) g_selectedIndices.erase((size_t)idx);
                        else                 g_selectedIndices.insert((size_t)idx);
                        if (g_view) [g_view setNeedsDisplay:YES];
                    } else if (!alreadySelected || g_selectedIndices.size() <= 1) {
                        g_selectedIndices.clear();
                        g_selectedIndices.insert((size_t)idx);
                        if (g_view) [g_view setNeedsDisplay:YES];
                    }
                    return NO;
                }
                if (!g_selectedIndices.empty()) {
                    g_selectedIndices.clear();
                    if (g_view) [g_view setNeedsDisplay:YES];
                }
                return NO;
            };

            // ── Drag-out ─────────────────────────────────────────────────────
            // Called by DDDragView when the user drags from the shelf.
            // Multi-item: if the drag starts on an already-selected tile, all
            // selected tiles are dragged together.  If the drag starts on a
            // non-selected tile, only that tile is dragged (no selection change).
            // Each item becomes a separate NSDraggingItem so the target receives
            // one drop per file / URL / text, as AppKit natively expects.
            rv.ddDragOutBlock = ^NSArray<NSDraggingItem*>*(NSPoint pt) {
                const dd::ItemList& items = *si;
                if (items.empty()) return nil;

                NSInteger clickedIdx = hitTestItemAt(pt, items.size());
                if (clickedIdx < 0) return nil;

                std::vector<size_t> toDrag;
                if (g_selectedIndices.count((size_t)clickedIdx)) {
                    toDrag.assign(g_selectedIndices.begin(), g_selectedIndices.end());
                } else {
                    toDrag = { (size_t)clickedIdx };
                }

                NSMutableArray<NSDraggingItem*>* result = [NSMutableArray array];
                for (size_t i : toDrag) {
                    if (i >= items.size()) continue;
                    const auto& item = items[i];
                    NSRect tileR = (i < g_tileRects.size())
                        ? g_tileRects[i]
                        : NSMakeRect(0, 0, kIconW, kIconH);

                    NSImage* tile = iconForItem(item);
                    if (!tile) {
                        tile = [[NSImage alloc] initWithSize:NSMakeSize(kIconW, kIconH)];
                        [tile lockFocus];
                        drawColoredTile(NSMakeRect(0, 0, kIconW, kIconH), item);
                        [tile unlockFocus];
                    }

                    id<NSPasteboardWriting> writer = nil;
                    if (item.data.path.has_value()) {
                        writer = [NSURL fileURLWithPath:
                            [NSString stringWithUTF8String:item.data.path->c_str()]];
                    } else {
                        NSString* text = nil;
                        if (item.data.text_content.has_value())
                            text = [NSString stringWithUTF8String:
                                item.data.text_content->c_str()];
                        else if (item.data.title.has_value())
                            text = [NSString stringWithUTF8String:
                                item.data.title->c_str()];
                        if (text) {
                            NSPasteboardItem* pbi = [[NSPasteboardItem alloc] init];
                            [pbi setString:text forType:NSPasteboardTypeString];
                            writer = pbi;
                        }
                    }
                    if (!writer) continue;

                    NSDraggingItem* di =
                        [[NSDraggingItem alloc] initWithPasteboardWriter:writer];
                    [di setDraggingFrame:tileR contents:tile];
                    [result addObject:di];
                }
                return result.count > 0 ? result : nil;
            };

            // ── Rubber-band selection ─────────────────────────────────────────
            rv.ddRubberBandBlock = ^(NSRect selRect) {
                bool cmdDown = ([NSEvent modifierFlags] &
                                NSEventModifierFlagCommand) != 0;
                if (!cmdDown) g_selectedIndices.clear();
                for (NSUInteger i = 0; i < si->size() && i < g_tileRects.size(); ++i) {
                    NSRect tileArea = NSMakeRect(g_tileRects[i].origin.x,
                                                  g_tileRects[i].origin.y,
                                                  kIconW, kTileH);
                    if (NSIntersectsRect(tileArea, selRect))
                        g_selectedIndices.insert((size_t)i);
                }
                if (g_view) [g_view setNeedsDisplay:YES];
            };

            // ── Scroll ───────────────────────────────────────────────────────
            rv.ddScrollBlock = ^(CGFloat deltaY) {
                if (g_maxScrollOffset <= 0.0) return;
                g_scrollOffsetY = std::max(0.0,
                    std::min(g_scrollOffsetY - deltaY, g_maxScrollOffset));
                if (g_view) [g_view setNeedsDisplay:YES];
            };
        }
    }

    ok_ = true;
    return true;
}

void Renderer::shutdown() {
    // Nil all blocks before releasing the view reference so that any in-flight
    // completion handlers (thumbnails, favicons) that dispatch_async to the main
    // queue find nil blocks and do nothing rather than touching freed state.
    if (g_view && [g_view conformsToProtocol:@protocol(DDRenderable)]) {
        id<DDRenderable> rv = (id<DDRenderable>)g_view;
        rv.ddDrawBlock        = nil;
        rv.ddHitTestBlock     = nil;
        rv.ddHandleClickBlock = nil;
        rv.ddDragOutBlock     = nil;
        rv.ddRubberBandBlock  = nil;
        rv.ddScrollBlock      = nil;
    }
    g_view            = nil;
    ok_               = false;
    g_scrollOffsetY   = 0.0;
    g_maxScrollOffset = 0.0;
    g_selectedIndices.clear();
    g_tileRects.clear();
    g_labelCache.clear();

    // Release the CGPath before clearing bounds so the cache stays consistent.
    if (g_bgPath) { CGPathRelease(g_bgPath); g_bgPath = nullptr; }
    g_bgBounds = {};

    // Flush all image caches and in-flight request sets.  Any completion handler
    // that arrives after this point checks g_view == nil and skips the redraw.
    g_iconCache.clear();
    g_thumbCache.clear();
    g_faviconCache.clear();
    g_pendingThumbs.clear();
    g_pendingFavicons.clear();
}

void Renderer::setItems(const ItemList& items) {
    *shared_items_ = items;
    g_selectedIndices.clear();
    g_labelCache.clear(); // item list changed — all cached labels/widths are stale
    if (items.empty()) g_scrollOffsetY = 0.0;
    if (ok_ && g_view) [g_view setNeedsDisplay:YES];
}

ItemList Renderer::items() const { return *shared_items_; }

void Renderer::setClearCallback(std::function<void()> cb) {
    *clearCallback_ = std::move(cb);
}

void Renderer::setHideCallback(std::function<void()> cb) {
    *hideCallback_ = std::move(cb);
}

void Renderer::render(float) {
    if (!ok_ || !g_view) return;
    [g_view setNeedsDisplay:YES];
}

} // namespace dd
