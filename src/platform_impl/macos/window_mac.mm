#include "platform/window/native_window.hpp"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import "dd_renderable.h"

// ─── DDWindowDelegate ─────────────────────────────────────────────────────────
// Holds all event callbacks and handles NSWindowDelegate notifications.
// Mouse / key / drag events come through DDDragView (the content view) below;
// DDDragView forwards them by calling the callbacks stored here.

@interface DDWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, copy) void (^onPaint)(void);
@property (nonatomic, copy) void (^onResize)(int w, int h);
@property (nonatomic, copy) void (^onMouseDown)(int x, int y, int button);
@property (nonatomic, copy) void (^onMouseUp)(int x, int y, int button);
@property (nonatomic, copy) void (^onMouseMove)(int x, int y);
@property (nonatomic, copy) void (^onKeyDown)(int keyCode, int modifiers);
@property (nonatomic, copy) void (^onKeyUp)(int keyCode, int modifiers);
@property (nonatomic, copy) void (^onDragEnter)(int x, int y);
@property (nonatomic, copy) void (^onDragOver)(int x, int y);
@property (nonatomic, copy) void (^onDragLeave)(void);
@property (nonatomic, copy) void (^onDrop)(NSArray<NSString*>* items);
@property (nonatomic, copy) void (^onClose)(void);
@end

@implementation DDWindowDelegate

- (void)windowDidResize:(NSNotification*)n {
    if (!self.onResize) return;
    NSWindow* w = [n object];
    NSRect f = [w contentRectForFrameRect:w.frame];
    self.onResize((int)f.size.width, (int)f.size.height);
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    if (self.onClose) self.onClose();
    return YES;
}

- (void)windowWillClose:(NSNotification*)n { (void)n; }

@end


// ─── DDDragView ───────────────────────────────────────────────────────────────
// The window's content view. Sits in the responder chain so mouse / key / drag
// events arrive here via the normal Cocoa dispatch mechanism, then get forwarded
// to DDWindowDelegate's callbacks.

// Drag-grip dimensions. The top strip, bottom strip, and thin side edges act as
// window-drag grips; the interior background is the rubber-band selection zone.
// kWindowDragZoneH must match `topReserved` in drawShelf (7+18+7 = 32).
static const CGFloat kWindowDragZoneH = 32.0;
static const CGFloat kBottomGripH     = 20.0; // px from the bottom edge
static const CGFloat kSideGripW       = 10.0; // px from each side edge

@interface DDDragView : NSView <NSDraggingDestination, NSDraggingSource, DDRenderable>
@property (nonatomic, weak) DDWindowDelegate* ddDelegate;
@property (nonatomic, copy, nullable) void (^ddDrawBlock)(CGContextRef ctx, CGRect bounds);
@property (nonatomic, copy, nullable) BOOL (^ddHitTestBlock)(NSPoint pt);
@property (nonatomic, copy, nullable) NSArray<NSDraggingItem*>* (^ddDragOutBlock)(NSPoint pt);
@property (nonatomic, copy, nullable) BOOL (^ddHandleClickBlock)(NSPoint pt);
@property (nonatomic, copy, nullable) void (^ddRubberBandBlock)(NSRect selRect);
@property (nonatomic, copy, nullable) void (^ddScrollBlock)(CGFloat deltaY);
@end

@implementation DDDragView {
    NSPoint _mouseDownPt;       // saved in mouseDown: for drag-gesture detection
    BOOL    _dragInitiated;     // prevents multiple session starts per gesture
    BOOL    _mouseDownOnTile;   // YES iff mouseDown landed on an item tile
    BOOL    _rubberBandActive;  // YES while a rubber-band selection is in progress
    NSRect  _rubberBandRect;    // current rubber-band selection rect (view coords)
    // Manual window drag — does not depend on AppKit movableByWindowBackground.
    // AppKit's movableByWindowBackground stops working after programmatic setFrame:
    // (e.g. a drop resizes the shelf) until the window is hidden+shown; manual
    // drag is immune to this because it never touches AppKit's drag machinery.
    BOOL    _windowDragActive;      // YES while the user is dragging the window
    NSPoint _windowDragScreenPt;    // [NSEvent mouseLocation] at mouseDown
    NSPoint _windowDragOrigin;      // window.frame.origin at mouseDown
}

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)isOpaque              { return NO;  }
// Y=0 at top-left, matching drawShelf's coordinate assumptions.
- (BOOL)isFlipped             { return YES; }

// Controls whether clicking-and-dragging on this view moves the underlying
// NSPanel window.
//   • Buttons and tiles              → NO  (they own their interactions)
//   • Top strip (y < 32)            → YES (header drag grip)
//   • Bottom strip (last 20 px)     → YES (bottom drag grip)
//   • Left / right edges (10 px ea) → YES (side drag grips)
//   • Interior background            → NO  (rubber-band selection zone)
//
// Always NO — window drag is implemented manually in mouseDragged: via
// _windowDragActive so that it survives programmatic setFrame: calls.
// AppKit's movableByWindowBackground mechanism loses its state after a
// programmatic frame change (e.g. shelf resizes on drop), which is why
// returning YES here caused dragging to break until hide+show reset it.
- (BOOL)mouseDownCanMoveWindow { return NO; }

// AppKit calls this with a correctly scaled, flipped CGContext — no manual
// CGBitmapContext, coordinate transforms, or contentsScale bookkeeping needed.
- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    CGContextRef ctx = (CGContextRef)[[NSGraphicsContext currentContext] CGContext];
    CGContextClearRect(ctx, NSRectToCGRect(self.bounds));
    if (_ddDrawBlock) _ddDrawBlock(ctx, NSRectToCGRect(self.bounds));

    // Rubber-band selection overlay — drawn on top of everything else.
    if (_rubberBandActive && !NSIsEmptyRect(_rubberBandRect)) {
        CGContextSaveGState(ctx);
        CGRect rb = NSRectToCGRect(_rubberBandRect);
        CGContextSetRGBFillColor(ctx,   0.20, 0.50, 1.0, 0.12);
        CGContextFillRect(ctx, rb);
        CGContextSetRGBStrokeColor(ctx, 0.20, 0.50, 1.0, 0.75);
        CGContextSetLineWidth(ctx, 1.0);
        CGContextStrokeRect(ctx, CGRectInset(rb, 0.5, 0.5));
        CGContextRestoreGState(ctx);
    }
}

// ── Mouse ────────────────────────────────────────────────────────────────────

- (void)mouseDown:(NSEvent*)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    // Always reset drag state first — even if we return early below.
    _mouseDownPt       = p;
    _dragInitiated     = NO;
    _rubberBandActive  = NO;
    _rubberBandRect    = NSZeroRect;
    _windowDragActive  = NO;
    _mouseDownOnTile   = _ddHitTestBlock ? _ddHitTestBlock(p) : NO;
    // Detect grip zones for manual window drag (only when not on a tile/button).
    if (!_mouseDownOnTile && self.window) {
        NSSize sz = self.bounds.size;
        BOOL inGrip = (p.y < kWindowDragZoneH)
                   || (p.y > sz.height - kBottomGripH)
                   || (p.x < kSideGripW)
                   || (p.x > sz.width  - kSideGripW);
        if (inGrip) {
            _windowDragActive   = YES;
            _windowDragScreenPt = [NSEvent mouseLocation];
            _windowDragOrigin   = self.window.frame.origin;
        }
    }
    // Let the renderer consume special click regions (buttons, etc.).
    if (_ddHandleClickBlock && _ddHandleClickBlock(p)) return;
    if (self.ddDelegate.onMouseDown)
        self.ddDelegate.onMouseDown((int)p.x, (int)p.y, (int)e.buttonNumber);
}
- (void)mouseUp:(NSEvent*)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    _windowDragActive = NO;
    // Finalize any in-progress rubber-band.
    if (_rubberBandActive) {
        _rubberBandActive = NO;
        _rubberBandRect   = NSZeroRect;
        [self setNeedsDisplay:YES];
    }
    if (self.ddDelegate.onMouseUp)
        self.ddDelegate.onMouseUp((int)p.x, (int)p.y, (int)e.buttonNumber);
}
- (void)mouseMoved:(NSEvent*)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    if (self.ddDelegate.onMouseMove)
        self.ddDelegate.onMouseMove((int)p.x, (int)p.y);
}
- (void)mouseDragged:(NSEvent*)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];

    // ── Manual window drag from grip zones ───────────────────────────────────
    // _windowDragActive is set in mouseDown: when the press landed in a grip
    // zone (not on any tile or button).  We track the delta from the original
    // screen position ourselves so this works even after a programmatic
    // setFrame: has reset AppKit's internal movableByWindowBackground state.
    if (_windowDragActive) {
        NSPoint cur = [NSEvent mouseLocation];
        NSPoint newOrigin = NSMakePoint(
            _windowDragOrigin.x + cur.x - _windowDragScreenPt.x,
            _windowDragOrigin.y + cur.y - _windowDragScreenPt.y);
        [self.window setFrameOrigin:newOrigin];
        return;
    }

    // ── File drag-out from a tile ─────────────────────────────────────────────
    if (!_dragInitiated && _mouseDownOnTile && _ddDragOutBlock) {
        if (hypot(p.x - _mouseDownPt.x, p.y - _mouseDownPt.y) > 5.0) {
            NSArray<NSDraggingItem*>* dragItems = _ddDragOutBlock(_mouseDownPt);
            if (dragItems.count > 0) {
                _dragInitiated    = YES;
                _rubberBandActive = NO;
                // Hide the shelf so it doesn't cover the drop target.
                // draggingSession:endedAtPoint:operation: will restore it.
                [self.window orderOut:nil];
                [self beginDraggingSessionWithItems:dragItems event:e source:self];
                return;
            }
        }
        return; // on a tile but no items yet — don't move window or rubber-band
    }

    // ── Rubber-band selection (interior background only) ─────────────────────
    // Only starts when the mouseDown was in the INTERIOR of the shelf — i.e.
    // below the top grip, above the bottom grip, and away from the side grips.
    // Grip-zone drags are handled above via _windowDragActive.
    {
        NSSize sz  = self.bounds.size;
        BOOL inInterior = !_mouseDownOnTile
            && _mouseDownPt.y >= kWindowDragZoneH
            && _mouseDownPt.y <= (sz.height - kBottomGripH)
            && _mouseDownPt.x >= kSideGripW
            && _mouseDownPt.x <= (sz.width  - kSideGripW);
        if (inInterior) {
            if (!_rubberBandActive) {
                if (hypot(p.x - _mouseDownPt.x, p.y - _mouseDownPt.y) > 3.0)
                    _rubberBandActive = YES;
            }
            if (_rubberBandActive) {
                _rubberBandRect = NSMakeRect(
                    std::min(p.x, _mouseDownPt.x),
                    std::min(p.y, _mouseDownPt.y),
                    std::abs(p.x - _mouseDownPt.x),
                    std::abs(p.y - _mouseDownPt.y));
                if (_ddRubberBandBlock) _ddRubberBandBlock(_rubberBandRect);
                [self setNeedsDisplay:YES];
                return;
            }
        }
    }
}

// ── NSDraggingSource ─────────────────────────────────────────────────────────

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    (void)session; (void)context;
    return NSDragOperationEvery; // let the destination pick Copy / Move / Link
}
- (void)draggingSession:(NSDraggingSession*)session
         endedAtPoint:(NSPoint)screenPoint
            operation:(NSDragOperation)operation {
    (void)session; (void)screenPoint; (void)operation;
    // Restore the shelf after the drag completes or is cancelled.
    [self.window orderFrontRegardless];
}
- (void)rightMouseDown:(NSEvent*)e    { [self mouseDown:e];  }
- (void)rightMouseUp:(NSEvent*)e      { [self mouseUp:e];    }
- (void)otherMouseDown:(NSEvent*)e    { [self mouseDown:e];  }
- (void)otherMouseUp:(NSEvent*)e      { [self mouseUp:e];    }

// ── Keyboard ─────────────────────────────────────────────────────────────────

- (void)keyDown:(NSEvent*)e {
    if (self.ddDelegate.onKeyDown)
        self.ddDelegate.onKeyDown((int)e.keyCode, (int)e.modifierFlags);
}
- (void)keyUp:(NSEvent*)e {
    if (self.ddDelegate.onKeyUp)
        self.ddDelegate.onKeyUp((int)e.keyCode, (int)e.modifierFlags);
}

// ── Scroll wheel ─────────────────────────────────────────────────────────────

- (void)scrollWheel:(NSEvent*)e {
    // Use precise scrolling deltas for trackpad/Magic Mouse; fall back to
    // the coarser deltaY (scaled by 12 px per tick) for traditional wheels.
    CGFloat delta = e.hasPreciseScrollingDeltas ? e.scrollingDeltaY : e.deltaY * 12.0;
    if (_ddScrollBlock && delta != 0.0) _ddScrollBlock(delta);
    else [super scrollWheel:e];
}

// ── NSDraggingDestination ─────────────────────────────────────────────────────

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)s {
    NSPoint p = [self convertPoint:[s draggingLocation] fromView:nil];
    if (self.ddDelegate.onDragEnter)
        self.ddDelegate.onDragEnter((int)p.x, (int)p.y);
    return NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)s {
    NSPoint p = [self convertPoint:[s draggingLocation] fromView:nil];
    if (self.ddDelegate.onDragOver)
        self.ddDelegate.onDragOver((int)p.x, (int)p.y);
    return NSDragOperationCopy;
}

- (void)draggingExited:(id<NSDraggingInfo>)s {
    (void)s;
    if (self.ddDelegate.onDragLeave) self.ddDelegate.onDragLeave();
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)s { (void)s; return YES; }

- (BOOL)performDragOperation:(id<NSDraggingInfo>)s {
    NSPasteboard* pb = [s draggingPasteboard];
    NSMutableArray<NSString*>* items = [NSMutableArray array];

    // 1. File URLs — highest priority
    NSArray<NSURL*>* fileURLs = [pb readObjectsForClasses:@[[NSURL class]]
        options:@{NSPasteboardURLReadingFileURLsOnlyKey: @YES}];
    for (NSURL* url in fileURLs) {
        if (url.isFileURL) [items addObject:url.path];
    }

    // 2. Web / generic URLs (only if no file paths were found)
    if (items.count == 0) {
        NSArray<NSURL*>* urls = [pb readObjectsForClasses:@[[NSURL class]] options:@{}];
        for (NSURL* url in urls) {
            if (!url.isFileURL) [items addObject:url.absoluteString];
        }
    }

    // 3. Plain text
    if (items.count == 0) {
        NSString* str = [pb stringForType:NSPasteboardTypeString];
        if (str.length > 0) [items addObject:str];
    }

    // 4. Dropped image → write to a uniquely-named temp PNG
    if (items.count == 0) {
        NSImage* img = [[NSImage alloc] initWithPasteboard:pb];
        if (img) {
            NSBitmapImageRep* rep =
                [NSBitmapImageRep imageRepWithData:img.TIFFRepresentation];
            NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG
                                             properties:@{}];
            if (png) {
                NSString* tmp = [[NSTemporaryDirectory()
                    stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]]
                    stringByAppendingPathExtension:@"png"];
                if ([png writeToFile:tmp atomically:YES])
                    [items addObject:tmp];
            }
        }
    }

    if (self.ddDelegate.onDrop) self.ddDelegate.onDrop(items);
    return items.count > 0;
}

- (void)concludeDragOperation:(id<NSDraggingInfo>)s { (void)s; }

@end


// ─── DDWindow ─────────────────────────────────────────────────────────────────

@interface DDWindow : NSPanel
- (instancetype)initWithStyle:(dd::WindowStyle)style;
@property (nonatomic, assign, readonly) dd::WindowStyle ddStyle;
@property (nonatomic, strong, readonly) DDWindowDelegate* ddDelegate;
@end

@implementation DDWindow {
    DDWindowDelegate* _ddDelegate;
    NSTrackingArea*   _trackingArea;
    dd::WindowStyle   _ddStyle;
}

- (instancetype)initWithStyle:(dd::WindowStyle)style {
    NSRect frame = NSMakeRect(100, 100, 400, 120);
    const BOOL isShelf = (style != dd::WindowStyle::Normal);

    // Non-activating mask so the panel never steals focus from the active app.
    NSWindowStyleMask mask = isShelf
        ? (NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel)
        : (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable);

    self = [super initWithContentRect:frame styleMask:mask
                              backing:NSBackingStoreBuffered defer:NO];
    if (!self) return nil;

    _ddStyle   = style;
    _ddDelegate = [[DDWindowDelegate alloc] init];
    [self setDelegate:_ddDelegate];

    if (isShelf) {
        // Float above normal windows.
        [self setLevel:NSFloatingWindowLevel];
        // Stay visible even when the application deactivates.
        [self setHidesOnDeactivate:NO];
        // Follow the user across all Spaces; don't appear in Mission Control.
        [self setCollectionBehavior:
            NSWindowCollectionBehaviorCanJoinAllSpaces |
            NSWindowCollectionBehaviorTransient];
    }

    [self setHasShadow:YES];
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];

    if (style == dd::WindowStyle::Frameless || style == dd::WindowStyle::Transparent) {
        [self setTitlebarAppearsTransparent:YES];
        [self setTitleVisibility:NSWindowTitleHidden];
        [[self standardWindowButton:NSWindowCloseButton]      setHidden:YES];
        [[self standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
        [[self standardWindowButton:NSWindowZoomButton]        setHidden:YES];
    }

    // DDDragView IS the content view. It implements NSDraggingDestination so
    // the system delivers drag events to it directly (view-level registration
    // survives any later changes to the layer hierarchy made by the renderer).
    DDDragView* dragView = [[DDDragView alloc] initWithFrame:frame];
    dragView.ddDelegate  = _ddDelegate;
    dragView.wantsLayer  = YES;
    dragView.layer.opaque = NO;
    dragView.layer.backgroundColor = [[NSColor clearColor] CGColor];
    dragView.layer.cornerRadius  = 14;
    dragView.layer.masksToBounds = YES;
    [self setContentView:dragView];
    [self makeFirstResponder:dragView];

    // Register drag types on the VIEW, not the window.
    [dragView registerForDraggedTypes:@[
        NSPasteboardTypeFileURL,
        NSPasteboardTypeURL,
        NSPasteboardTypeString,
        NSPasteboardTypeHTML,
        NSPasteboardTypeTIFF,
        NSPasteboardTypePNG,
    ]];

    // Tracking area for mouseMoved: events.
    _trackingArea = [[NSTrackingArea alloc]
        initWithRect:frame
             options:NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect
               owner:dragView
            userInfo:nil];
    [dragView addTrackingArea:_trackingArea];

    return self;
}

- (BOOL)canBecomeKeyWindow  { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
- (BOOL)isMovableByWindowBackground { return YES; }

- (dd::WindowStyle)ddStyle    { return _ddStyle;    }
- (DDWindowDelegate*)ddDelegate { return _ddDelegate; }

@end


// ─── C++ helpers ──────────────────────────────────────────────────────────────

namespace {

void wireCallbacks(DDWindow* w,
    dd::PaintCallback            paintCb,
    dd::ResizeCallback           resizeCb,
    dd::MouseCallback            mouseDownCb,
    dd::MouseCallback            mouseUpCb,
    dd::MouseMoveCallback        mouseMoveCb,
    dd::KeyCallback              keyDownCb,
    dd::KeyCallback              keyUpCb,
    dd::WindowDragEnterCallback  dragEnterCb,
    dd::WindowDragOverCallback   dragOverCb,
    dd::WindowDragLeaveCallback  dragLeaveCb,
    dd::WindowDropCallback       dropCb,
    dd::CloseCallback            closeCb)
{
    DDWindowDelegate* d = w.ddDelegate;
    d.onResize    = [=](int w_, int h_) { if (resizeCb)    resizeCb(w_, h_); };
    d.onMouseDown = [=](int x, int y, int b) { if (mouseDownCb) mouseDownCb(x, y, static_cast<dd::MouseButton>(b)); };
    d.onMouseUp   = [=](int x, int y, int b) { if (mouseUpCb)   mouseUpCb  (x, y, static_cast<dd::MouseButton>(b)); };
    d.onMouseMove = [=](int x, int y)        { if (mouseMoveCb) mouseMoveCb(x, y); };
    d.onKeyDown   = [=](int k, int m)        { if (keyDownCb)   keyDownCb(k, m);   };
    d.onKeyUp     = [=](int k, int m)        { if (keyUpCb)     keyUpCb(k, m);     };
    d.onDragEnter = [=](int x, int y)        { if (dragEnterCb) dragEnterCb(x, y); };
    d.onDragOver  = [=](int x, int y)        { if (dragOverCb)  dragOverCb(x, y);  };
    d.onDragLeave = [=]()                    { if (dragLeaveCb) dragLeaveCb();      };
    d.onDrop      = [=](NSArray<NSString*>* files) {
        if (!dropCb) return;
        std::vector<std::string> paths;
        paths.reserve(files.count);
        for (NSString* p in files) paths.emplace_back(p.UTF8String);
        dropCb(std::move(paths));
    };
    d.onClose = [=]() { if (closeCb) closeCb(); };
    (void)paintCb;
}

} // anonymous namespace


// ─── DDMacWindow (C++ NativeWindow implementation) ────────────────────────────

namespace dd {

class DDMacWindow final : public NativeWindow {
public:
    explicit DDMacWindow(WindowStyle style)
        : window_([[DDWindow alloc] initWithStyle:style])
    {
        wireCallbacks(window_,
            paintCallback_,  resizeCallback_,
            mouseDownCallback_, mouseUpCallback_, mouseMoveCallback_,
            keyDownCallback_,   keyUpCallback_,
            dragEnterCallback_, dragOverCallback_, dragLeaveCallback_,
            dropCallback_, closeCallback_);
    }

    ~DDMacWindow() override { close(); }

    // Use orderFrontRegardless so the window surfaces even when the app is in
    // the background (e.g. triggered by a global hotkey while Finder is active).
    void show() override { [window_ orderFrontRegardless]; }
    void hide() override { [window_ orderOut:nil]; }
    void close() override { [window_ close]; }

    void setBounds(int x, int y, int w, int h) override {
        [window_ setFrame:NSMakeRect(x, y, w, h) display:YES];
    }
    Rect getBounds() const override {
        NSRect r = window_.frame;
        return { static_cast<int>(r.origin.x),    static_cast<int>(r.origin.y),
                 static_cast<int>(r.size.width),   static_cast<int>(r.size.height) };
    }

    void setAlwaysOnTop(bool e) override {
        [window_ setLevel:e ? NSFloatingWindowLevel : NSNormalWindowLevel];
    }
    void setTransparency(float a) override { window_.alphaValue = a; }
    void setVisible(bool v) override {
        v ? [window_ orderFrontRegardless] : [window_ orderOut:nil];
    }
    bool isVisible() const override { return window_.isVisible; }
    void setTitle(std::string_view t) override {
        window_.title = [NSString stringWithUTF8String:t.data()];
    }
    void minimize() override  { [window_ miniaturize:nil]; }
    void restore() override   { [window_ deminiaturize:nil]; }
    void* nativeHandle() const override { return (__bridge void*)window_.contentView; }

    void setPaintCallback(PaintCallback cb)              override { paintCallback_     = std::move(cb); }
    void setResizeCallback(ResizeCallback cb)            override { resizeCallback_    = std::move(cb); }
    void setMouseDownCallback(MouseCallback cb)          override { mouseDownCallback_ = std::move(cb); }
    void setMouseMoveCallback(MouseMoveCallback cb)      override { mouseMoveCallback_ = std::move(cb); }
    void setMouseUpCallback(MouseCallback cb)            override { mouseUpCallback_   = std::move(cb); }
    void setKeyDownCallback(KeyCallback cb)              override { keyDownCallback_   = std::move(cb); }
    void setKeyUpCallback(KeyCallback cb)                override { keyUpCallback_     = std::move(cb); }
    void setDragEnterCallback(WindowDragEnterCallback cb) override { dragEnterCallback_ = std::move(cb); }
    void setDragOverCallback(WindowDragOverCallback cb)  override { dragOverCallback_  = std::move(cb); }
    void setDragLeaveCallback(WindowDragLeaveCallback cb) override { dragLeaveCallback_ = std::move(cb); }
    void setDropCallback(WindowDropCallback cb) override {
        dropCallback_ = std::move(cb);
        // wireCallbacks captured callbacks by value at construction time (when
        // they were empty). Re-wire onDrop now so it reads dropCallback_ through
        // `this` at call time, picking up whatever was set most recently.
        // Capturing `this` is safe: the delegate is owned by the DDWindow which
        // is owned by this DDMacWindow — the block cannot outlive us.
        window_.ddDelegate.onDrop = ^(NSArray<NSString*>* files) {
            if (!this->dropCallback_) return;
            std::vector<std::string> paths;
            paths.reserve(files.count);
            for (NSString* p in files) paths.emplace_back(p.UTF8String);
            this->dropCallback_(std::move(paths));
        };
    }
    void setCloseCallback(CloseCallback cb)              override { closeCallback_     = std::move(cb); }

    void positionNearCursor(int w, int h) override {
        NSPoint cursor = [NSEvent mouseLocation];
        NSScreen* screen = [NSScreen mainScreen] ?: [[NSScreen screens] firstObject];
        NSRect sf = screen ? screen.visibleFrame : NSMakeRect(0, 0, 1440, 900);
        // Center shelf on cursor X; place just above cursor.
        int x = static_cast<int>(cursor.x) - w / 2;
        int y = static_cast<int>(cursor.y) + 20;
        // Clamp so the shelf stays fully on screen.
        x = std::max((int)sf.origin.x,
                std::min(x, (int)(sf.origin.x + sf.size.width  - w)));
        y = std::max((int)sf.origin.y,
                std::min(y, (int)(sf.origin.y + sf.size.height - h)));
        [window_ setFrame:NSMakeRect(x, y, w, h) display:YES];
    }

private:
    DDWindow*                window_;
    PaintCallback            paintCallback_;
    ResizeCallback           resizeCallback_;
    MouseCallback            mouseDownCallback_;
    MouseCallback            mouseUpCallback_;
    MouseMoveCallback        mouseMoveCallback_;
    KeyCallback              keyDownCallback_;
    KeyCallback              keyUpCallback_;
    WindowDragEnterCallback  dragEnterCallback_;
    WindowDragOverCallback   dragOverCallback_;
    WindowDragLeaveCallback  dragLeaveCallback_;
    WindowDropCallback       dropCallback_;
    CloseCallback            closeCallback_;
};

std::unique_ptr<NativeWindow> NativeWindow::create(WindowStyle style) {
    return std::make_unique<DDMacWindow>(style);
}

} // namespace dd
