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
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)s {
    (void)s;
    return NO;
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
