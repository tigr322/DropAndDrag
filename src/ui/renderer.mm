#import <Cocoa/Cocoa.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import "../platform_impl/macos/dd_renderable.h"
#include "renderer.hpp"
#include <core/items/item.hpp>
#include <algorithm>
#include <set>

// ─── File-scope globals ───────────────────────────────────────────────────────

static NSView* g_view = nil;
static int     g_w    = 400;
static int     g_h    = 120;

// Updated by drawShelf every frame; read by all interaction blocks.
// Main-thread only — no locking needed.
static std::vector<NSRect> g_tileRects;                  // icon rect per item (multi-row)
static NSRect          g_clearBtnRect = {};
static NSRect          g_hideBtnRect  = {};
static std::set<size_t> g_selectedIndices;   // currently selected tile indices

// ─── Caches — main thread only ───────────────────────────────────────────────

static NSMutableDictionary<NSString*, NSImage*>* g_iconCache;
static NSMutableDictionary<NSString*, NSImage*>* g_thumbCache;
static NSMutableDictionary<NSString*, NSImage*>* g_faviconCache;
static NSMutableSet<NSString*>*                  g_pendingThumbs;
static NSMutableSet<NSString*>*                  g_pendingFavicons;
// Limits simultaneous QLThumbnailGenerator requests. Without this, dropping
// 50 images fires 50 concurrent async requests. Value 4 keeps throughput
// high while avoiding system-level resource contention.
static dispatch_semaphore_t g_thumbSemaphore;

static void ensureCaches(void) {
    if (!g_iconCache)       g_iconCache      = [NSMutableDictionary dictionary];
    if (!g_thumbCache)      g_thumbCache     = [NSMutableDictionary dictionary];
    if (!g_faviconCache)    g_faviconCache   = [NSMutableDictionary dictionary];
    if (!g_pendingThumbs)   g_pendingThumbs  = [NSMutableSet set];
    if (!g_pendingFavicons) g_pendingFavicons = [NSMutableSet set];
    if (!g_thumbSemaphore)  g_thumbSemaphore = dispatch_semaphore_create(4);
}

// ─── systemIconForPath ───────────────────────────────────────────────────────

static NSImage* systemIconForPath(NSString* path) {
    NSString* key = (path.length > 0) ? path : @"__generic__";
    NSImage* hit = g_iconCache[key];
    if (hit) return hit;
    NSImage* icon = (path.length > 0)
        ? [[NSWorkspace sharedWorkspace] iconForFile:path]
        : [[NSWorkspace sharedWorkspace] iconForFileType:@"public.data"];
    if (!icon) icon = [NSImage imageNamed:NSImageNameMultipleDocuments];
    if (icon) g_iconCache[key] = icon;
    return icon;
}

// ─── requestThumbnail ────────────────────────────────────────────────────────

static void requestThumbnail(NSString* filePath, NSString* uuid) {
    if ([g_pendingThumbs containsObject:uuid]) return;
    [g_pendingThumbs addObject:uuid];

    if (@available(macOS 10.15, *)) {
        NSURL* url = [NSURL fileURLWithPath:filePath];
        QLThumbnailGenerationRequest* req =
            [[QLThumbnailGenerationRequest alloc]
                initWithFileAtURL:url
                             size:CGSizeMake(96, 96)
                            scale:2.0
                representationTypes:QLThumbnailGenerationRequestRepresentationTypeThumbnail];
        // Dispatch to a background thread so the semaphore wait doesn't block
        // the main thread. At most 4 QL requests run simultaneously; extras
        // queue behind the semaphore and proceed as slots free up.
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
            dispatch_semaphore_wait(g_thumbSemaphore, DISPATCH_TIME_FOREVER);
            [[QLThumbnailGenerator sharedGenerator]
                generateBestRepresentationForRequest:req
                completionHandler:^(QLThumbnailRepresentation* rep, NSError* err) {
                    dispatch_semaphore_signal(g_thumbSemaphore); // release slot
                    NSImage* img = (!err && rep) ? rep.NSImage : nil;
                    dispatch_async(dispatch_get_main_queue(), ^{
                        if (img) { [img setSize:NSMakeSize(48, 48)]; g_thumbCache[uuid] = img; }
                        else     [g_pendingThumbs removeObject:uuid];
                        if (g_view) [g_view setNeedsDisplay:YES];
                    });
                }];
        });
    } else {
        [g_pendingThumbs removeObject:uuid];
    }
}

// ─── requestFavicon ──────────────────────────────────────────────────────────

static void requestFavicon(NSString* domain) {
    if ([g_pendingFavicons containsObject:domain]) return;
    [g_pendingFavicons addObject:domain];

    NSURL* url = [NSURL URLWithString:
        [NSString stringWithFormat:
            @"https://www.google.com/s2/favicons?domain=%@&sz=64", domain]];
    [[[NSURLSession sharedSession]
        dataTaskWithURL:url
        completionHandler:^(NSData* data, NSURLResponse*, NSError* err) {
            NSImage* img = (!err && data.length > 0)
                ? [[NSImage alloc] initWithData:data] : nil;
            dispatch_async(dispatch_get_main_queue(), ^{
                if (img) g_faviconCache[domain] = img;
                else     [g_pendingFavicons removeObject:domain];
                if (g_view) [g_view setNeedsDisplay:YES];
            });
        }] resume];
}

// ─── iconForItem ─────────────────────────────────────────────────────────────

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
            NSString* uuid = [NSString stringWithUTF8String:item.data.uuid.c_str()];
            NSImage* thumb = g_thumbCache[uuid];
            if (thumb) return thumb;
            if (item.data.path.has_value())
                requestThumbnail(
                    [NSString stringWithUTF8String:item.data.path->c_str()], uuid);
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
                NSString* domain = [NSURL URLWithString:urlStr].host;
                if (domain.length > 0) {
                    NSImage* fav = g_faviconCache[domain];
                    if (fav) return fav;
                    requestFavicon(domain);
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
    NSDictionary* a = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:18],
                         NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSString* lbl = [NSString stringWithUTF8String:type];
    NSSize sz = [lbl sizeWithAttributes:a];
    [lbl drawAtPoint:NSMakePoint(NSMinX(r) + (48 - sz.width)  / 2,
                                  NSMinY(r) + (48 - sz.height) / 2 - 2)
       withAttributes:a];
}

// ─── Geometry helpers ─────────────────────────────────────────────────────────

// Layout constants
static const CGFloat kStep  = 64.0;
static const CGFloat kIconW = 48.0;
static const CGFloat kIconH = 48.0;
static const CGFloat kTileH  = 62.0; // icon + label row height
static const CGFloat kRowGap = 10.0; // vertical gap between rows
static const CGFloat kBtnSz  = 18.0;
static const CGFloat kBtnMg = 7.0;

// Returns the tile index under `pt`, or -1 if none.
// Searches g_tileRects (filled by drawShelf) — works for any number of rows.
static NSInteger hitTestItemAt(NSPoint pt, NSUInteger count) {
    if (count == 0 || g_tileRects.size() < count) return -1;
    for (NSUInteger i = 0; i < count; ++i) {
        // Hit area covers the icon plus the label row below it.
        NSRect r = NSMakeRect(g_tileRects[i].origin.x, g_tileRects[i].origin.y,
                              kIconW, kTileH);
        if (NSPointInRect(pt, r)) return (NSInteger)i;
    }
    return -1;
}

// ─── drawShelf ────────────────────────────────────────────────────────────────

static void drawShelf(CGContextRef ctx, CGRect bounds, const dd::ItemList& items) {
    ensureCaches();

    // Background
    CGContextSetRGBFillColor(ctx, 0.0, 0.0, 0.0, 0.55);
    CGRect bg = CGRectInset(bounds, 2, 2);
    CGMutablePathRef bgPath = CGPathCreateMutable();
    CGPathAddRoundedRect(bgPath, nullptr, bg, 14, 14);
    CGContextAddPath(ctx, bgPath);
    CGContextFillPath(ctx);
    CGPathRelease(bgPath);

    NSUInteger n = items.size();

    // ── Hide button — top-left amber circle ───────────────────────────────────
    g_hideBtnRect = NSMakeRect(kBtnMg, kBtnMg, kBtnSz, kBtnSz);
    {
        NSBezierPath* circle = [NSBezierPath bezierPathWithOvalInRect:g_hideBtnRect];
        [[NSColor colorWithRed:0.98 green:0.74 blue:0.18 alpha:0.90] setFill];
        [circle fill];
        // Minus symbol
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

    // ── Multi-row layout ──────────────────────────────────────────────────────
    // Fill g_tileRects: wrap items into rows, center each row independently.
    // hitTestItemAt and ddDragOutBlock read g_tileRects (main thread only).
    {
        const CGFloat margin = 12.0;
        NSInteger cols = (NSInteger)((bounds.size.width - 2.0 * margin) / kStep);
        if (cols < 1) cols = 1;
        NSInteger rows = ((NSInteger)n + cols - 1) / cols;
        CGFloat groupH = (CGFloat)rows * kTileH + (CGFloat)(rows - 1) * kRowGap;
        // Start below the top buttons; vertically center the grid in the window.
        CGFloat topReserved = kBtnMg + kBtnSz + kBtnMg;
        CGFloat startY = std::max(topReserved, (bounds.size.height - groupH) / 2.0);

        g_tileRects.resize(n);
        for (NSUInteger i = 0; i < n; ++i) {
            NSInteger row = (NSInteger)i / cols;
            NSInteger col = (NSInteger)i % cols;
            // Last row may have fewer items — center it independently.
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
        NSDictionary* hint = @{
            NSFontAttributeName: [NSFont systemFontOfSize:13],
            NSForegroundColorAttributeName: [NSColor colorWithWhite:0.75 alpha:0.55]
        };
        NSString* msg = @"Drop files here";
        NSSize ms = [msg sizeWithAttributes:hint];
        [msg drawAtPoint:NSMakePoint((bounds.size.width  - ms.width)  / 2,
                                      (bounds.size.height - ms.height) / 2)
           withAttributes:hint];
        return;
    }

    // ── Item tiles ────────────────────────────────────────────────────────────
    NSDictionary* labelAttrs = @{
        NSFontAttributeName: [NSFont systemFontOfSize:10],
        NSForegroundColorAttributeName: [NSColor colorWithWhite:0.9 alpha:1.0]
    };

    for (NSUInteger i = 0; i < n; ++i) {
        const auto& item = items[i];
        NSRect iconRect = g_tileRects[i];
        bool selected = g_selectedIndices.count(i) > 0;

        // Selection highlight behind the icon (before clipping)
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

        // Icon (clipped to rounded rect)
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

        // Selection border on top of icon
        if (selected) {
            NSBezierPath* border = [NSBezierPath
                bezierPathWithRoundedRect:NSInsetRect(iconRect, 1.5, 1.5)
                                  xRadius:7 yRadius:7];
            border.lineWidth = 2.5;
            [[NSColor colorWithRed:0.20 green:0.50 blue:1.0 alpha:0.95] setStroke];
            [border stroke];
        }

        // Label — truncate on NSString (character-aware, not byte-count).
        // Truncating std::string by bytes splits multi-byte UTF-8 sequences,
        // making stringWithUTF8String: return nil → label invisible.
        std::string name = item.data.file_name.value_or(
            item.data.title.value_or(item.data.text_content.value_or("item")));
        NSString* nStr = [NSString stringWithUTF8String:name.c_str()] ?: @"???";
        if (nStr.length > 16)
            nStr = [[nStr substringToIndex:14] stringByAppendingString:@"…"];
        NSSize ns = [nStr sizeWithAttributes:labelAttrs];
        [nStr drawAtPoint:NSMakePoint(iconRect.origin.x + (kIconW - ns.width) / 2,
                                       iconRect.origin.y + kIconH + 2)
           withAttributes:labelAttrs];
    }
}

// ─── Renderer ────────────────────────────────────────────────────────────────

namespace dd {

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(void* view, int w, int h) {
    width_ = w; height_ = h;
    g_w = w; g_h = h;

    if (view) {
        g_view = (__bridge NSView*)view;
        auto si = shared_items_;
        auto cc = clearCallback_;

        if ([g_view conformsToProtocol:@protocol(DDRenderable)]) {
            id<DDRenderable> rv = (id<DDRenderable>)g_view;

            // ── Draw ─────────────────────────────────────────────────────────
            rv.ddDrawBlock = ^(CGContextRef ctx, CGRect boundsIn) {
                drawShelf(ctx, boundsIn, *si);
            };

            // ── Hit-test (mouseDownCanMoveWindow) ─────────────────────────────
            // Returns YES for buttons and tiles → prevents window drag on those.
            rv.ddHitTestBlock = ^BOOL(NSPoint pt) {
                if (NSPointInRect(pt, g_hideBtnRect))  return YES;
                if (!NSEqualRects(g_clearBtnRect, NSZeroRect) &&
                    NSPointInRect(pt, g_clearBtnRect)) return YES;
                return hitTestItemAt(pt, si->size()) >= 0;
            };

            // ── Click handling ───────────────────────────────────────────────
            // Handles buttons and selection changes.
            // Returns YES to consume (skip normal mouseDown:); NO to let
            // mouseDown: continue (needed so _mouseDownPt is set for drag).
            rv.ddHandleClickBlock = ^BOOL(NSPoint pt) {
                // Hide button
                if (NSPointInRect(pt, g_hideBtnRect)) {
                    if (g_view) [[g_view window] orderOut:nil];
                    return YES;
                }
                // Clear button
                if (!NSEqualRects(g_clearBtnRect, NSZeroRect) &&
                    NSPointInRect(pt, g_clearBtnRect) && !si->empty()) {
                    g_selectedIndices.clear();
                    si->clear();
                    if (*cc) (*cc)();
                    if (g_view) [g_view setNeedsDisplay:YES];
                    return YES;
                }
                // Item tile — update selection but return NO so mouseDown:
                // still records _mouseDownPt (needed for drag-out detection).
                NSInteger idx = hitTestItemAt(pt, si->size());
                if (idx >= 0) {
                    bool alreadySelected = g_selectedIndices.count((size_t)idx) > 0;
                    bool cmdDown = ([NSEvent modifierFlags] &
                                    NSEventModifierFlagCommand) != 0;
                    if (cmdDown) {
                        // Toggle
                        if (alreadySelected) g_selectedIndices.erase((size_t)idx);
                        else                 g_selectedIndices.insert((size_t)idx);
                        if (g_view) [g_view setNeedsDisplay:YES];
                    } else if (!alreadySelected || g_selectedIndices.size() <= 1) {
                        // Select only this one (unless it's already part of a
                        // multi-selection — we preserve the group for drag).
                        g_selectedIndices.clear();
                        g_selectedIndices.insert((size_t)idx);
                        if (g_view) [g_view setNeedsDisplay:YES];
                    }
                    return NO; // let mouseDown: set _mouseDownPt
                }
                // Background click — deselect all
                if (!g_selectedIndices.empty()) {
                    g_selectedIndices.clear();
                    if (g_view) [g_view setNeedsDisplay:YES];
                }
                return NO;
            };

            // ── Drag-out ─────────────────────────────────────────────────────
            // If the drag starts on a selected tile, drags ALL selected tiles.
            // Otherwise drags only the tile under the cursor.
            rv.ddDragOutBlock = ^NSArray<NSDraggingItem*>*(NSPoint pt) {
                const dd::ItemList& items = *si;
                if (items.empty()) return nil;

                NSInteger clickedIdx = hitTestItemAt(pt, items.size());
                if (clickedIdx < 0) return nil;

                // Build the set to drag
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

                    // Drag ghost
                    NSImage* tile = iconForItem(item);
                    if (!tile) {
                        tile = [[NSImage alloc] initWithSize:NSMakeSize(kIconW, kIconH)];
                        [tile lockFocus];
                        drawColoredTile(NSMakeRect(0, 0, kIconW, kIconH), item);
                        [tile unlockFocus];
                    }

                    // Pasteboard writer
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
            // Called every mouseDragged: tick while a rubber-band is active.
            // Without ⌘: replaces selection with all tiles intersecting selRect.
            // With ⌘: adds intersecting tiles to the existing selection.
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
        }
    }

    ok_ = true;
    return true;
}

void Renderer::shutdown() {
    if (g_view && [g_view conformsToProtocol:@protocol(DDRenderable)]) {
        id<DDRenderable> rv = (id<DDRenderable>)g_view;
        rv.ddDrawBlock        = nil;
        rv.ddHitTestBlock     = nil;
        rv.ddHandleClickBlock = nil;
        rv.ddDragOutBlock     = nil;
    }
    g_view = nil;
    ok_    = false;
    g_selectedIndices.clear();
    g_tileRects.clear();

    [g_iconCache       removeAllObjects];
    [g_thumbCache      removeAllObjects];
    [g_faviconCache    removeAllObjects];
    [g_pendingThumbs   removeAllObjects];
    [g_pendingFavicons removeAllObjects];
}

void Renderer::setItems(const ItemList& items) {
    *shared_items_ = items;
    g_selectedIndices.clear(); // indices are stale after a list change
    if (ok_ && g_view) [g_view setNeedsDisplay:YES];
}

ItemList Renderer::items() const { return *shared_items_; }

void Renderer::setClearCallback(std::function<void()> cb) {
    *clearCallback_ = std::move(cb);
}

void Renderer::render(float) {
    if (!ok_ || !g_view) return;
    [g_view setNeedsDisplay:YES];
}

} // namespace dd
