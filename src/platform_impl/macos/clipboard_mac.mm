#include "platform/clipboard/clipboard.hpp"

#import <AppKit/AppKit.h>

namespace dd {

ClipboardManager& ClipboardManager::instance() {
    static ClipboardManager instance;
    return instance;
}

void ClipboardManager::copy(std::string_view text) {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        NSString *nsStr = [NSString stringWithUTF8String:text.data()];
        [pasteboard setString:nsStr forType:NSPasteboardTypeString];
    }
}

void ClipboardManager::copyFile(std::string_view path) {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];

        NSString *nsPath = [NSString stringWithUTF8String:path.data()];
        NSURL *fileURL = [NSURL fileURLWithPath:nsPath];

        [pasteboard writeObjects:@[fileURL]];
    }
}

void ClipboardManager::copyImage(const std::vector<uint8_t>& data) {
    @autoreleasepool {
        if (data.empty()) return;

        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];

        NSData *imageData = [NSData dataWithBytes:data.data() length:data.size()];
        NSImage *image = [[NSImage alloc] initWithData:imageData];
        if (image) {
            [pasteboard clearContents];
            [pasteboard writeObjects:@[image]];
        }
    }
}

std::string ClipboardManager::paste() const {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSString *text = [pasteboard stringForType:NSPasteboardTypeString];
        if (text) {
            return std::string(text.UTF8String);
        }
        return {};
    }
}

bool ClipboardManager::hasText() const {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSString *text = [pasteboard stringForType:NSPasteboardTypeString];
        return text.length > 0;
    }
}

bool ClipboardManager::hasImage() const {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSArray *classes = @[[NSImage class]];
        NSDictionary *options = @{};
        NSArray *images = [pasteboard readObjectsForClasses:classes options:options];
        return images.count > 0;
    }
}

} // namespace dd
