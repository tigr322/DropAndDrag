#include "platform/drag_drop/drag_drop.hpp"

#import <AppKit/AppKit.h>
#import <CoreServices/CoreServices.h>

namespace dd {

@class DDMacDragDrop;

} // namespace dd

@interface DDMacDragDrop : NSObject

- (DragOperation)onDragEnter:(int)x y:(int)y defaultOp:(DragOperation)defaultOp;
- (DragOperation)onDragOver:(int)x y:(int)y;
- (void)onDragLeave;
- (std::vector<dd::DropItemData>)onDropWithPasteboard:(NSPasteboard *)pasteboard
                                            flags:(NSEventModifierFlags)flags;

@end

@implementation DDMacDragDrop {
    dd::DragDropManager *_manager;
}

- (instancetype)initWithManager:(dd::DragDropManager *)manager {
    self = [super init];
    if (self) {
        _manager = manager;
    }
    return self;
}

static DropItemData parseItemFromPasteboard(NSPasteboard *pasteboard) {
    DropItemData item;

    NSArray *classes = @[[NSURL class], [NSString class]];
    NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey: @YES};

    NSArray *urls = [pasteboard readObjectsForClasses:classes options:options];
    if (urls.count > 0) {
        BOOL isDirectory = NO;
        for (id obj in urls) {
            if ([obj isKindOfClass:[NSURL class]]) {
                NSURL *url = (NSURL *)obj;
                if ([url isFileURL]) {
                    item.type = DropDataType::File;
                    item.file_path = std::string(url.path.UTF8String);

                    NSDictionary *attrs = [[NSFileManager defaultManager]
                        attributesOfItemAtPath:url.path error:nil];
                    if (attrs) {
                        item.file_size = attrs.fileSize;
                    }
                    return item;
                } else {
                    item.type = DropDataType::Url;
                    item.url = std::string(url.absoluteString.UTF8String);
                }
            } else if ([obj isKindOfClass:[NSString class]]) {
                NSString *str = (NSString *)obj;
                NSURL *testUrl = [NSURL URLWithString:str];
                if (testUrl && testUrl.scheme) {
                    item.type = DropDataType::Url;
                    item.url = std::string(str.UTF8String);
                } else {
                    item.type = DropDataType::Text;
                    item.text = std::string(str.UTF8String);
                }
            }
        }
    }

    // Check for plain text
    if (item.type == DropDataType::File && item.file_path.empty()) {
        NSString *text = [pasteboard stringForType:NSPasteboardTypeString];
        if (text.length > 0) {
            item.type = DropDataType::Text;
            item.text = std::string(text.UTF8String);
        }
    }

    return item;
}

- (DragOperation)onDragEnter:(int)x y:(int)y defaultOp:(DragOperation)defaultOp {
    return static_cast<DragOperation>(
        _manager->onDragEnter(x, y, defaultOp));
}

- (DragOperation)onDragOver:(int)x y:(int)y {
    return static_cast<DragOperation>(_manager->onDragOver(x, y));
}

- (void)onDragLeave {
    _manager->onDragLeave();
}

- (std::vector<dd::DropItemData>)onDropWithPasteboard:(NSPasteboard *)pasteboard
                                            flags:(NSEventModifierFlags)flags {
    (void)flags;
    std::vector<DropItemData> items;

    NSArray *classes = @[[NSURL class], [NSString class]];
    NSDictionary *options = @{
        NSPasteboardURLReadingFileURLsOnlyKey: @YES,
        NSPasteboardURLReadingContentsConformToTypesKey: @[(__bridge NSString *)kUTTypeItem]
    };

    NSArray *urls = [pasteboard readObjectsForClasses:classes options:options];

    if (urls.count > 0) {
        for (id obj in urls) {
            if ([obj isKindOfClass:[NSURL class]]) {
                NSURL *url = (NSURL *)obj;
                DropItemData item;
                if ([url isFileURL]) {
                    item.type = DropDataType::File;
                    item.file_path = std::string(url.path.UTF8String);

                    NSDictionary *attrs = [[NSFileManager defaultManager]
                        attributesOfItemAtPath:url.path error:nil];
                    if (attrs) {
                        NSNumber *sizeNum = attrs[NSFileSize];
                        if (sizeNum) {
                            item.file_size = sizeNum.unsignedLongLongValue;
                        }
                    }
                } else {
                    item.type = DropDataType::Url;
                    item.url = std::string(url.absoluteString.UTF8String);
                }
                items.push_back(std::move(item));
            } else if ([obj isKindOfClass:[NSString class]]) {
                NSString *str = (NSString *)obj;
                DropItemData item;
                NSURL *testUrl = [NSURL URLWithString:str];
                if (testUrl && testUrl.scheme) {
                    item.type = DropDataType::Url;
                    item.url = std::string(str.UTF8String);
                } else {
                    item.type = DropDataType::Text;
                    item.text = std::string(str.UTF8String);
                }
                items.push_back(std::move(item));
            }
        }
    }

    // Fallback: check for plain text if nothing parsed
    if (items.empty()) {
        NSString *text = [pasteboard stringForType:NSPasteboardTypeString];
        if (text.length > 0) {
            DropItemData item;
            item.type = DropDataType::Text;
            item.text = std::string(text.UTF8String);
            items.push_back(std::move(item));
        }
    }

    return items;
}

@end

// ─── DragDropManager macOS singleton ─────────────────────────────────────────

static DDMacDragDrop *_sharedDragDrop = nil;

DragDropManager& DragDropManager::instance() {
    static DragDropManager instance;
    return instance;
}

std::vector<DropItemData> DragDropManager::onDrop() {
    // The actual drop parsing is platform-specific and handled
    // by the window delegate's performDragOperation: method.
    // This returns the parsed items from the current dragging pasteboard.
    return {};
}

} // namespace dd
