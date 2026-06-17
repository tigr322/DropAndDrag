#pragma once
#import <Cocoa/Cocoa.h>

// Adopted by DDDragView so that renderer.mm can install a drawing callback and
// trigger redraws without depending on DDDragView's concrete Objective-C type.
// The block receives the CGContextRef already set up by AppKit (correct CTM,
// Retina scale, and flipped coordinate system) and the view's logical bounds.
@protocol DDRenderable <NSObject>
// Called from drawRect: — draw the shelf into AppKit's already-set-up context.
@property (nonatomic, copy, nullable) void (^ddDrawBlock)(CGContextRef ctx, CGRect bounds);
// Lightweight hit-test — YES iff `pt` falls on a tile.  Used by
// mouseDownCanMoveWindow so the result is available before any objects are
// allocated; keeps window-drag and file-drag-out mutually exclusive.
@property (nonatomic, copy, nullable) BOOL (^ddHitTestBlock)(NSPoint pt);
// Full drag-out builder — only called when a real drag gesture fires on a tile.
@property (nonatomic, copy, nullable) NSArray<NSDraggingItem*>* (^ddDragOutBlock)(NSPoint pt);
// Special click regions (clear button, etc.). Return YES to consume the click
// and prevent normal mouseDown: processing.
@property (nonatomic, copy, nullable) BOOL (^ddHandleClickBlock)(NSPoint pt);
@end
