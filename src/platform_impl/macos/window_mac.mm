#include "platform/window/native_window.hpp"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

namespace {

// ─── DDWindowDelegate ────────────────────────────────────────────────────────

@interface DDWindowDelegate : NSObject <NSWindowDelegate, NSDraggingDestination>

@property (nonatomic, copy) void (^onPaint)(void);
@property (nonatomic, copy) void (^onResize)(int width, int height);
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

- (void)windowDidResize:(NSNotification *)notification {
    (void)notification;
    if (self.onResize) {
        NSWindow *window = [notification object];
        NSRect frame = [window contentRectForFrameRect:window.frame];
        self.onResize((int)frame.size.width, (int)frame.size.height);
    }
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
    (void)sender;
    if (self.onClose) {
        self.onClose();
    }
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
    (void)notification;
}

// ─── Mouse events ────────────────────────────────────────────────────────────

- (void)mouseDown:(NSEvent *)event fromWindow:(NSWindow *)window {
    NSPoint loc = [window.contentView convertPoint:event.locationInWindow fromView:nil];
    if (self.onMouseDown) {
        self.onMouseDown((int)loc.x, (int)loc.y, (int)event.buttonNumber);
    }
}

- (void)mouseUp:(NSEvent *)event fromWindow:(NSWindow *)window {
    NSPoint loc = [window.contentView convertPoint:event.locationInWindow fromView:nil];
    if (self.onMouseUp) {
        self.onMouseUp((int)loc.x, (int)loc.y, (int)event.buttonNumber);
    }
}

- (void)mouseMoved:(NSEvent *)event fromWindow:(NSWindow *)window {
    NSPoint loc = [window.contentView convertPoint:event.locationInWindow fromView:nil];
    if (self.onMouseMove) {
        self.onMouseMove((int)loc.x, (int)loc.y);
    }
}

- (void)mouseDragged:(NSEvent *)event fromWindow:(NSWindow *)window {
    [self mouseMoved:event fromWindow:window];
}

- (void)rightMouseDown:(NSEvent *)event fromWindow:(NSWindow *)window {
    [self mouseDown:event fromWindow:window];
}

- (void)rightMouseUp:(NSEvent *)event fromWindow:(NSWindow *)window {
    [self mouseUp:event fromWindow:window];
}

- (void)otherMouseDown:(NSEvent *)event fromWindow:(NSWindow *)window {
    [self mouseDown:event fromWindow:window];
}

- (void)otherMouseUp:(NSEvent *)event fromWindow:(NSWindow *)window {
    [self mouseUp:event fromWindow:window];
}

// ─── Key events ──────────────────────────────────────────────────────────────

- (void)keyDown:(NSEvent *)event fromWindow:(NSWindow *)window {
    (void)window;
    if (self.onKeyDown) {
        self.onKeyDown((int)event.keyCode, (int)event.modifierFlags);
    }
}

- (void)keyUp:(NSEvent *)event fromWindow:(NSWindow *)window {
    (void)window;
    if (self.onKeyUp) {
        self.onKeyUp((int)event.keyCode, (int)event.modifierFlags);
    }
}

// ─── NSDraggingDestination ───────────────────────────────────────────────────

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    NSPoint loc = [sender draggingLocation];
    if (self.onDragEnter) {
        self.onDragEnter((int)loc.x, (int)loc.y);
    }
    return NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    NSPoint loc = [sender draggingLocation];
    if (self.onDragOver) {
        self.onDragOver((int)loc.x, (int)loc.y);
    }
    return NSDragOperationCopy;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    (void)sender;
    if (self.onDragLeave) {
        self.onDragLeave();
    }
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
    (void)sender;
    return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPoint loc = [sender draggingLocation];
    if (self.onDrop) {
        self.onDrop((int)loc.x, (int)loc.y);
    }
    return YES;
}

- (void)concludeDragOperation:(id<NSDraggingInfo>)sender {
    (void)sender;
}

@end

// ─── DDWindow (NSPanel subclass) ─────────────────────────────────────────────

@interface DDWindow : NSPanel

@property (nonatomic, strong) DDWindowDelegate *ddDelegate;
@property (nonatomic, strong) NSView *contentHostView;
@property (nonatomic, strong) NSVisualEffectView *blurView;

- (instancetype)initWithStyle:(dd::WindowStyle)style;

@end

@implementation DDWindow {
    dd::WindowStyle _style;
    NSTrackingArea *_trackingArea;
}

- (instancetype)initWithStyle:(dd::WindowStyle)style {
    NSRect initialFrame = NSMakeRect(0, 0, 400, 300);

    NSUInteger styleMask = NSWindowStyleMaskBorderless;
    if (style == dd::WindowStyle::Normal) {
        styleMask = NSWindowStyleMaskTitled
                  | NSWindowStyleMaskClosable
                  | NSWindowStyleMaskMiniaturizable
                  | NSWindowStyleMaskResizable;
    }

    self = [super initWithContentRect:initialFrame
                            styleMask:styleMask
                              backing:NSBackingStoreBuffered
                                defer:NO];

    if (self) {
        _style = style;

        self.titlebarAppearsTransparent = YES;
        self.titleVisibility = NSWindowTitleHidden;
        self.movableByWindowBackground = YES;
        self.releasedWhenClosed = NO;
        self.hasShadow = (style != dd::WindowStyle::Transparent);
        self.opaque = (style == dd::WindowStyle::Normal);
        self.backgroundColor = (style == dd::WindowStyle::Transparent)
            ? [NSColor clearColor]
            : [NSColor windowBackgroundColor];

        if (style == dd::WindowStyle::Frameless || style == dd::WindowStyle::Transparent) {
            self.styleMask |= NSWindowStyleMaskFullSizeContentView;
        }

        if (style == dd::WindowStyle::Transparent) {
            self.level = NSFloatingWindowLevel;
            self.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
                                    | NSWindowCollectionBehaviorStationary
                                    | NSWindowCollectionBehaviorIgnoresCycle;
            self.ignoresMouseEvents = NO;
        }

        // Visual effect glassmorphism background
        _blurView = [[NSVisualEffectView alloc] initWithFrame:initialFrame];
        _blurView.wantsLayer = YES;
        _blurView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        _blurView.material = NSVisualEffectMaterialHUDWindow;
        _blurView.state = NSVisualEffectStateActive;
        _blurView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

        // Layer-backed content host view
        _contentHostView = [[NSView alloc] initWithFrame:initialFrame];
        _contentHostView.wantsLayer = YES;
        _contentHostView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [_contentHostView addSubview:_blurView];

        // Create a wrapper view that holds both the blur and the content host
        NSView *wrapper = [[NSView alloc] initWithFrame:initialFrame];
        wrapper.wantsLayer = YES;
        wrapper.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [wrapper addSubview:_contentHostView];

        self.contentView = wrapper;

        // Delegate
        _ddDelegate = [[DDWindowDelegate alloc] init];
        self.delegate = _ddDelegate;

        // Register for drag-and-drop
        [self registerForDraggedTypes:@[
            NSPasteboardTypeFileURL,
            (__bridge NSString *)kUTTypeFileURL,
            NSPasteboardTypeString,
            NSPasteboardTypeURL,
            NSFilenamesPboardType
        ]];

        // Mouse tracking area
        _trackingArea = [[NSTrackingArea alloc] initWithRect:initialFrame
                                                     options:NSTrackingMouseMoved
                                                       | NSTrackingActiveAlways
                                                       | NSTrackingInVisibleRect
                                                       | NSTrackingMouseEnteredAndExited
                                                       owner:self
                                                    userInfo:nil];
        [self.contentView addTrackingArea:_trackingArea];
    }

    return self;
}

- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (BOOL)canBecomeMainWindow {
    return YES;
}

- (void)mouseDown:(NSEvent *)event {
    [self.ddDelegate mouseDown:event fromWindow:self];
    [super mouseDown:event];
}

- (void)mouseUp:(NSEvent *)event {
    [self.ddDelegate mouseUp:event fromWindow:self];
    [super mouseUp:event];
}

- (void)mouseMoved:(NSEvent *)event {
    [self.ddDelegate mouseMoved:event fromWindow:self];
    [super mouseMoved:event];
}

- (void)mouseDragged:(NSEvent *)event {
    [self.ddDelegate mouseDragged:event fromWindow:self];
    [super mouseDragged:event];
}

- (void)rightMouseDown:(NSEvent *)event {
    [self.ddDelegate rightMouseDown:event fromWindow:self];
    [super rightMouseDown:event];
}

- (void)rightMouseUp:(NSEvent *)event {
    [self.ddDelegate rightMouseUp:event fromWindow:self];
    [super rightMouseUp:event];
}

- (void)otherMouseDown:(NSEvent *)event {
    [self.ddDelegate otherMouseDown:event fromWindow:self];
    [super otherMouseDown:event];
}

- (void)otherMouseUp:(NSEvent *)event {
    [self.ddDelegate otherMouseUp:event fromWindow:self];
    [super otherMouseUp:event];
}

- (void)keyDown:(NSEvent *)event {
    [self.ddDelegate keyDown:event fromWindow:self];
}

- (void)keyUp:(NSEvent *)event {
    [self.ddDelegate keyUp:event fromWindow:self];
}

@end

} // anonymous namespace

// ─── DDMacWindow ─────────────────────────────────────────────────────────────

namespace dd {

class DDMacWindow final : public NativeWindow {
public:
    explicit DDMacWindow(WindowStyle style);
    ~DDMacWindow() override;

    void show() override;
    void hide() override;
    void close() override;
    void setBounds(int x, int y, int w, int h) override;
    Rect getBounds() const override;
    void setAlwaysOnTop(bool enabled) override;
    void setTransparency(float alpha) override;
    void setVisible(bool visible) override;
    bool isVisible() const override;
    void setTitle(std::string_view title) override;
    void minimize() override;
    void restore() override;

    void setPaintCallback(PaintCallback cb) override;
    void setResizeCallback(ResizeCallback cb) override;
    void setMouseDownCallback(MouseCallback cb) override;
    void setMouseMoveCallback(MouseMoveCallback cb) override;
    void setMouseUpCallback(MouseCallback cb) override;
    void setKeyDownCallback(KeyCallback cb) override;
    void setKeyUpCallback(KeyCallback cb) override;
    void setDragEnterCallback(dd::DragEnterCallback cb) override;
    void setDragOverCallback(dd::DragOverCallback cb) override;
    void setDragLeaveCallback(dd::DragLeaveCallback cb) override;
    void setDropCallback(dd::DropCallback cb) override;
    void setCloseCallback(CloseCallback cb) override;

private:
    DDWindow *_window;
};

// ─── Static factory ──────────────────────────────────────────────────────────

std::unique_ptr<NativeWindow> NativeWindow::create(WindowStyle style) {
    return std::make_unique<DDMacWindow>(style);
}

// ─── DDMacWindow implementation ──────────────────────────────────────────────

DDMacWindow::DDMacWindow(WindowStyle style) {
    @autoreleasepool {
        _window = [[DDWindow alloc] initWithStyle:style];
    }
}

DDMacWindow::~DDMacWindow() {
    @autoreleasepool {
        if (_window) {
            [_window close];
            _window = nil;
        }
    }
}

void DDMacWindow::show() {
    @autoreleasepool {
        [_window makeKeyAndOrderFront:nil];
    }
}

void DDMacWindow::hide() {
    @autoreleasepool {
        [_window orderOut:nil];
    }
}

void DDMacWindow::close() {
    @autoreleasepool {
        [_window close];
    }
}

void DDMacWindow::setBounds(int x, int y, int w, int h) {
    @autoreleasepool {
        NSRect frame = NSMakeRect(x, y, w, h);
        [_window setFrame:frame display:YES animate:NO];
    }
}

Rect DDMacWindow::getBounds() const {
    __block Rect result;
    @autoreleasepool {
        NSRect frame = _window.frame;
        result = { (int)frame.origin.x, (int)frame.origin.y,
                   (int)frame.size.width, (int)frame.size.height };
    }
    return result;
}

void DDMacWindow::setAlwaysOnTop(bool enabled) {
    @autoreleasepool {
        if (enabled) {
            _window.level = NSFloatingWindowLevel;
        } else {
            _window.level = NSNormalWindowLevel;
        }
    }
}

void DDMacWindow::setTransparency(float alpha) {
    @autoreleasepool {
        _window.alphaValue = (CGFloat)alpha;
    }
}

void DDMacWindow::setVisible(bool visible) {
    @autoreleasepool {
        if (visible) {
            [_window orderFront:nil];
        } else {
            [_window orderOut:nil];
        }
    }
}

bool DDMacWindow::isVisible() const {
    return _window.isVisible;
}

void DDMacWindow::setTitle(std::string_view title) {
    @autoreleasepool {
        _window.title = [NSString stringWithUTF8String:title.data()];
    }
}

void DDMacWindow::minimize() {
    @autoreleasepool {
        [_window miniaturize:nil];
    }
}

void DDMacWindow::restore() {
    @autoreleasepool {
        [_window deminiaturize:nil];
    }
}

// ─── Callback setters ────────────────────────────────────────────────────────

void DDMacWindow::setPaintCallback(PaintCallback cb) {
    @autoreleasepool {
        __weak DDWindow *weakWindow = _window;
        _window.ddDelegate.onPaint = ^{
            if (cb) cb();
            DDWindow *strongWindow = weakWindow;
            if (strongWindow) {
                [strongWindow.contentHostView setNeedsDisplay:YES];
            }
        };
    }
}

void DDMacWindow::setResizeCallback(ResizeCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onResize = ^(int w, int h) {
            if (cb) cb(w, h);
        };
    }
}

void DDMacWindow::setMouseDownCallback(MouseCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onMouseDown = ^(int x, int y, int button) {
            if (cb) cb(x, y, static_cast<MouseButton>(button));
        };
    }
}

void DDMacWindow::setMouseMoveCallback(MouseMoveCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onMouseMove = ^(int x, int y) {
            if (cb) cb(x, y);
        };
    }
}

void DDMacWindow::setMouseUpCallback(MouseCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onMouseUp = ^(int x, int y, int button) {
            if (cb) cb(x, y, static_cast<MouseButton>(button));
        };
    }
}

void DDMacWindow::setKeyDownCallback(KeyCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onKeyDown = ^(int keyCode, int modifiers) {
            if (cb) cb(keyCode, modifiers);
        };
    }
}

void DDMacWindow::setKeyUpCallback(KeyCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onKeyUp = ^(int keyCode, int modifiers) {
            if (cb) cb(keyCode, modifiers);
        };
    }
}

void DDMacWindow::setDragEnterCallback(dd::DragEnterCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onDragEnter = ^(int x, int y) {
            if (cb) cb(x, y, DragOperation::Copy);
        };
    }
}

void DDMacWindow::setDragOverCallback(dd::DragOverCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onDragOver = ^(int x, int y) {
            if (cb) cb(x, y);
        };
    }
}

void DDMacWindow::setDragLeaveCallback(dd::DragLeaveCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onDragLeave = ^{
            if (cb) cb();
        };
    }
}

void DDMacWindow::setDropCallback(dd::DropCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onDrop = ^(int x, int y) {
            if (cb) cb(x, y);
        };
    }
}

void DDMacWindow::setCloseCallback(CloseCallback cb) {
    @autoreleasepool {
        _window.ddDelegate.onClose = ^{
            if (cb) cb();
        };
    }
}

} // namespace dd
