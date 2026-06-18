// native_app_mac.mm — macOS implementations of init_native_app() and
// native_loop_step().
//
// init_native_app(): configures NSApp as an accessory (no Dock icon, no menu
// bar activation focus-steal), then activates it so the shelf window accepts
// mouse events immediately.
//
// native_loop_step(): pumps one batch of NSEvents (non-blocking) — called by
// Application::run_cocoa_loop() on every iteration.

#include "platform/native_app.hpp"

#import <Cocoa/Cocoa.h>
#include <atomic>

@interface DDAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation DDAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)n {
    (void)n;
    [NSApp activateIgnoringOtherApps:YES];
}
- (void)applicationWillFinishLaunching:(NSNotification*)n {
    (void)n;
    NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
    [em setEventHandler:self
        andSelector:@selector(handleGetURLEvent:withReplyEvent:)
        forEventClass:kInternetEventClass
        andEventID:kAEGetURL];
}
- (void)handleGetURLEvent:(NSAppleEventDescriptor*)event
           withReplyEvent:(NSAppleEventDescriptor*)reply {
    (void)event; (void)reply;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)s {
    (void)s;
    return NO;
}
- (void)application:(NSApplication*)app openURLs:(NSArray<NSURL*>*)urls {
    (void)app; (void)urls;
}
@end

namespace dd {

static std::atomic<bool> g_app_initialized{false};

void init_native_app() {
    if (g_app_initialized.exchange(true)) return;

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

    static DDAppDelegate* delegate = [[DDAppDelegate alloc] init];
    [NSApp setDelegate:delegate];

    [NSApp finishLaunching];
}

void run_native_loop() {
    [NSApp run];
}

void terminate_native_app() {
    [NSApp terminate:nil];
}

bool native_loop_step() {
    @autoreleasepool {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate dateWithTimeIntervalSinceNow:0.016]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }
        [NSApp updateWindows];
    }
    return true;
}

} // namespace dd
