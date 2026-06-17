#include "platform/window/native_window.hpp"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

// ─── ObjC delegates in global scope ───────────────────────────────────────────

@interface DDWindowDelegate : NSObject <NSWindowDelegate, NSDraggingDestination>
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
@property (nonatomic, copy) void (^onDrop)(int x, int y);
@property (nonatomic, copy) void (^onClose)(void);
@end

@implementation DDWindowDelegate

- (void)windowDidResize:(NSNotification*)n {
    if (self.onResize) {
        NSWindow* w = [n object];
        NSRect f = [w contentRectForFrameRect:w.frame];
        self.onResize((int)f.size.width, (int)f.size.height);
    }
}

- (BOOL)windowShouldClose:(NSWindow*)sender { (void)sender; if (self.onClose) self.onClose(); return YES; }
- (void)windowWillClose:(NSNotification*)n { (void)n; }

- (void)mouseDown:(NSEvent*)e fromWindow:(NSWindow*)w {
    NSPoint p = [w.contentView convertPoint:e.locationInWindow fromView:nil];
    if (self.onMouseDown) self.onMouseDown((int)p.x, (int)p.y, (int)e.buttonNumber);
}
- (void)mouseUp:(NSEvent*)e fromWindow:(NSWindow*)w {
    NSPoint p = [w.contentView convertPoint:e.locationInWindow fromView:nil];
    if (self.onMouseUp) self.onMouseUp((int)p.x, (int)p.y, (int)e.buttonNumber);
}
- (void)mouseMoved:(NSEvent*)e fromWindow:(NSWindow*)w {
    NSPoint p = [w.contentView convertPoint:e.locationInWindow fromView:nil];
    if (self.onMouseMove) self.onMouseMove((int)p.x, (int)p.y);
}
- (void)mouseDragged:(NSEvent*)e fromWindow:(NSWindow*)w { [self mouseMoved:e fromWindow:w]; }
- (void)rightMouseDown:(NSEvent*)e fromWindow:(NSWindow*)w { [self mouseDown:e fromWindow:w]; }
- (void)rightMouseUp:(NSEvent*)e fromWindow:(NSWindow*)w { [self mouseUp:e fromWindow:w]; }
- (void)otherMouseDown:(NSEvent*)e fromWindow:(NSWindow*)w { [self mouseDown:e fromWindow:w]; }
- (void)otherMouseUp:(NSEvent*)e fromWindow:(NSWindow*)w { [self mouseUp:e fromWindow:w]; }

- (void)keyDown:(NSEvent*)e fromWindow:(NSWindow*)w {
    (void)w;
    if (self.onKeyDown) self.onKeyDown((int)e.keyCode, (int)e.modifierFlags);
}
- (void)keyUp:(NSEvent*)e fromWindow:(NSWindow*)w {
    (void)w;
    if (self.onKeyUp) self.onKeyUp((int)e.keyCode, (int)e.modifierFlags);
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)s {
    NSPoint p = [s draggingLocation];
    if (self.onDragEnter) self.onDragEnter((int)p.x, (int)p.y);
    return NSDragOperationCopy;
}
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)s {
    NSPoint p = [s draggingLocation];
    if (self.onDragOver) self.onDragOver((int)p.x, (int)p.y);
    return NSDragOperationCopy;
}
- (void)draggingExited:(id<NSDraggingInfo>)s { (void)s; if (self.onDragLeave) self.onDragLeave(); }
- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)s { (void)s; return YES; }
- (BOOL)performDragOperation:(id<NSDraggingInfo>)s {
    NSPoint p = [s draggingLocation];
    if (self.onDrop) self.onDrop((int)p.x, (int)p.y);
    return YES;
}
- (void)concludeDragOperation:(id<NSDraggingInfo>)s { (void)s; }

@end

@interface DDWindow : NSPanel
- (instancetype)initWithStyle:(dd::WindowStyle)style;
@property (nonatomic, assign, readonly) dd::WindowStyle style;
@end

@implementation DDWindow {
    DDWindowDelegate* _ddDelegate;
    NSTrackingArea* _trackingArea;
    dd::WindowStyle _style;
}

- (instancetype)initWithStyle:(dd::WindowStyle)style {
    NSRect frame = NSMakeRect(100, 100, 400, 120);
    NSWindowStyleMask mask = NSWindowStyleMaskBorderless;
    if (style == dd::WindowStyle::Normal) mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;

    self = [super initWithContentRect:frame styleMask:mask backing:NSBackingStoreBuffered defer:NO];
    if (!self) return nil;

    _style = style;
    _ddDelegate = [[DDWindowDelegate alloc] init];
    [self setDelegate:_ddDelegate];

    [self setLevel:NSFloatingWindowLevel];
    [self setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary];
    [self setHasShadow:YES];
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];

    if (style == dd::WindowStyle::Frameless || style == dd::WindowStyle::Transparent) {
        [self setTitlebarAppearsTransparent:YES];
        [self setTitleVisibility:NSWindowTitleHidden];
        [self standardWindowButton:NSWindowCloseButton].hidden = YES;
        [self standardWindowButton:NSWindowMiniaturizeButton].hidden = YES;
        [self standardWindowButton:NSWindowZoomButton].hidden = YES;
    }

    self.contentView.wantsLayer = YES;
    self.contentView.layer.opaque = NO;

    _trackingArea = [[NSTrackingArea alloc] initWithRect:frame
                                                 options:NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect
                                                   owner:_ddDelegate
                                                userInfo:nil];
    [self.contentView addTrackingArea:_trackingArea];

    [self registerForDraggedTypes:@[
        NSPasteboardTypeFileURL,
        NSPasteboardTypeURL,
        NSPasteboardTypeString,
        NSPasteboardTypeHTML,
        NSPasteboardTypeTIFF,
        NSPasteboardTypePNG
    ]];

    return self;
}

- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
- (BOOL)isMovableByWindowBackground { return YES; }

- (DDWindowDelegate*)ddDelegate { return _ddDelegate; }

@end

// ─── C++ anonymous namespace ─────────────────────────────────────────────────

namespace {

void setDelegateCallbacksFromWindow(DDWindow* w,
    dd::PaintCallback paintCb, dd::ResizeCallback resizeCb,
    dd::MouseCallback mouseDownCb, dd::MouseCallback mouseUpCb,
    dd::MouseMoveCallback mouseMoveCb,
    dd::KeyCallback keyDownCb, dd::KeyCallback keyUpCb,
    dd::WindowDragEnterCallback dragEnterCb, dd::WindowDragOverCallback dragOverCb,
    dd::WindowDragLeaveCallback dragLeaveCb, dd::WindowDropCallback dropCb,
    dd::CloseCallback closeCb)
{
    DDWindowDelegate* d = [w ddDelegate];
    d.onResize = [=](int w_, int h_) { if (resizeCb) resizeCb(w_, h_); };
    d.onMouseDown = [=](int x, int y, int b) { if (mouseDownCb) mouseDownCb(x, y, static_cast<dd::MouseButton>(b)); };
    d.onMouseUp = [=](int x, int y, int b) { if (mouseUpCb) mouseUpCb(x, y, static_cast<dd::MouseButton>(b)); };
    d.onMouseMove = [=](int x, int y) { if (mouseMoveCb) mouseMoveCb(x, y); };
    d.onKeyDown = [=](int k, int m) { if (keyDownCb) keyDownCb(k, m); };
    d.onKeyUp = [=](int k, int m) { if (keyUpCb) keyUpCb(k, m); };
    d.onDragEnter = [=](int x, int y) { if (dragEnterCb) dragEnterCb(x, y); };
    d.onDragOver = [=](int x, int y) { if (dragOverCb) dragOverCb(x, y); };
    d.onDragLeave = [=]() { if (dragLeaveCb) dragLeaveCb(); };
    d.onDrop = [=](int x, int y) { if (dropCb) dropCb(x, y); };
    d.onClose = [=]() { if (closeCb) closeCb(); };
    (void)paintCb;
}

} // anonymous namespace

// ─── DDMacWindow (C++ implementation) ────────────────────────────────────────

namespace dd {

class DDMacWindow final : public NativeWindow {
public:
    explicit DDMacWindow(WindowStyle style)
        : window_([[DDWindow alloc] initWithStyle:style])
    {
        setDelegateCallbacksFromWindow(
            window_, paintCallback_, resizeCallback_,
            mouseDownCallback_, mouseUpCallback_, mouseMoveCallback_,
            keyDownCallback_, keyUpCallback_,
            dragEnterCallback_, dragOverCallback_, dragLeaveCallback_, dropCallback_,
            closeCallback_
        );
    }

    ~DDMacWindow() override { close(); }

    void show() override { [window_ makeKeyAndOrderFront:nil]; }
    void hide() override { [window_ orderOut:nil]; }
    void close() override { [window_ close]; }
    void setBounds(int x, int y, int w, int h) override { [window_ setFrame:NSMakeRect(x, y, w, h) display:YES]; }
    Rect getBounds() const override {
        NSRect r = window_.frame;
        return {static_cast<int>(r.origin.x), static_cast<int>(r.origin.y),
                static_cast<int>(r.size.width), static_cast<int>(r.size.height)};
    }
    void setAlwaysOnTop(bool e) override { [window_ setLevel:e ? NSFloatingWindowLevel : NSNormalWindowLevel]; }
    void setTransparency(float a) override { window_.alphaValue = a; }
    void setVisible(bool v) override { v ? [window_ orderFront:nil] : [window_ orderOut:nil]; }
    bool isVisible() const override { return window_.isVisible; }
    void setTitle(std::string_view t) override { window_.title = [NSString stringWithUTF8String:t.data()]; }
    void minimize() override { [window_ miniaturize:nil]; }
    void restore() override { [window_ deminiaturize:nil]; }

    void setPaintCallback(PaintCallback cb) override { paintCallback_ = std::move(cb); }
    void setResizeCallback(ResizeCallback cb) override { resizeCallback_ = std::move(cb); }
    void setMouseDownCallback(MouseCallback cb) override { mouseDownCallback_ = std::move(cb); }
    void setMouseMoveCallback(MouseMoveCallback cb) override { mouseMoveCallback_ = std::move(cb); }
    void setMouseUpCallback(MouseCallback cb) override { mouseUpCallback_ = std::move(cb); }
    void setKeyDownCallback(KeyCallback cb) override { keyDownCallback_ = std::move(cb); }
    void setKeyUpCallback(KeyCallback cb) override { keyUpCallback_ = std::move(cb); }
    void setDragEnterCallback(WindowDragEnterCallback cb) override { dragEnterCallback_ = std::move(cb); }
    void setDragOverCallback(WindowDragOverCallback cb) override { dragOverCallback_ = std::move(cb); }
    void setDragLeaveCallback(WindowDragLeaveCallback cb) override { dragLeaveCallback_ = std::move(cb); }
    void setDropCallback(WindowDropCallback cb) override { dropCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) override { closeCallback_ = std::move(cb); }

private:
    DDWindow* window_;
    PaintCallback paintCallback_;
    ResizeCallback resizeCallback_;
    MouseCallback mouseDownCallback_;
    MouseCallback mouseUpCallback_;
    MouseMoveCallback mouseMoveCallback_;
    KeyCallback keyDownCallback_;
    KeyCallback keyUpCallback_;
    WindowDragEnterCallback dragEnterCallback_;
    WindowDragOverCallback dragOverCallback_;
    WindowDragLeaveCallback dragLeaveCallback_;
    WindowDropCallback dropCallback_;
    CloseCallback closeCallback_;
};

std::unique_ptr<NativeWindow> NativeWindow::create(WindowStyle style) {
    return std::make_unique<DDMacWindow>(style);
}

} // namespace dd
