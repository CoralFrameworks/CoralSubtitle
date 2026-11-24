/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file SubtitleDemoWindow.mm
 *
 * The macOS half of the render demo.
 *
 * The demo builds a subtitle document, parses it, rasterises the active cues and composites the
 * overlay over a synthetic frame. None of that needs a platform API; what this file adds is the
 * window that shows the result. The frame is already a `graphics::Bitmap` in premultiplied RGBA8888,
 * which is exactly the layout an `NSBitmapImageRep` uses with no bitmap flags, so presenting it is a
 * row copy and an `NSImageView`.
 *
 * The window is a plain AppKit window rather than a GPU surface: the renderer is a CPU canvas, and a
 * demo that only has to show one frame does not need an RHI device or a Metal layer. Esc, Q, the
 * close button and Cmd+Q all end the run loop, and `--window-seconds N` closes it when nobody is
 * watching (a scripted smoke run).
 */

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "SubtitleDemoWindow.h"

#include <graphics/Bitmap.h>
#include <graphics/Pixmap.h>

#include <cstring>

namespace {

// ------------------------------------------------------------------------------------------------
// The view and the delegates
// ------------------------------------------------------------------------------------------------

/** Stops the run loop and lets `[NSApp run]` return to `presentSubtitleFrame`. */
void stopApplication() {
  [NSApp stop:nil];
  // `stop:` is only observed when the run loop wakes up, so an event is queued behind it. Without
  // this the window would stay on screen until the user touched it.
  NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                     location:NSZeroPoint
                                modifierFlags:0
                                    timestamp:0
                                 windowNumber:0
                                      context:nil
                                      subtype:0
                                        data1:0
                                        data2:0];
  [NSApp postEvent:event atStart:YES];
}

} // namespace

/** An image view that closes the demo on Esc or Q and keeps the frame's aspect ratio. */
@interface CoralSubtitleImageView : NSImageView
@end

@implementation CoralSubtitleImageView

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)becomeFirstResponder {
  return YES;
}

- (void)keyDown:(NSEvent*)event {
  NSString* characters = [event charactersIgnoringModifiers];
  if (characters.length == 0) {
    return;
  }
  const unichar character = [characters characterAtIndex:0];
  if (character == 27 /* Esc */ || character == 'q' || character == 'Q') {
    stopApplication();
  }
}

- (void)cancelOperation:(id)sender {
  stopApplication();
}

@end

/** Owns the termination policy: closing the only window ends the run loop. */
@interface CoralSubtitleWindowDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@implementation CoralSubtitleWindowDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)application {
  return YES;
}

- (void)windowWillClose:(NSNotification*)notification {
  stopApplication();
}

@end

namespace coral {
namespace demo {

bool presentSubtitleFrame(const graphics::Bitmap& frame, const double seconds,
                          const std::string& title) {
  if (frame.isEmpty()) {
    return false;
  }
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    // A minimal menu so that Cmd+Q does what a macOS user expects.
    NSMenu* menuBar = [[NSMenu alloc] init];
    NSMenuItem* applicationItem = [[NSMenuItem alloc] init];
    [menuBar addItem:applicationItem];
    NSMenu* applicationMenu = [[NSMenu alloc] init];
    NSString* applicationName = [[NSProcessInfo processInfo] processName];
    NSString* quitTitle = [@"Quit " stringByAppendingString:applicationName];
    [applicationMenu addItemWithTitle:quitTitle
                               action:@selector(terminate:)
                        keyEquivalent:@"q"];
    [applicationItem setSubmenu:applicationMenu];
    [NSApp setMainMenu:menuBar];

    // Owned by a static because NSApplication's delegate is not retained.
    static CoralSubtitleWindowDelegate* delegate = nil;
    if (delegate == nil) {
      delegate = [[CoralSubtitleWindowDelegate alloc] init];
    }
    [NSApp setDelegate:delegate];

    const int width = frame.width();
    const int height = frame.height();
    const size_t rowBytes = static_cast<size_t>(width) * 4u;
    NSBitmapImageRep* representation =
      [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                             pixelsWide:width
                                             pixelsHigh:height
                                          bitsPerSample:8
                                        samplesPerPixel:4
                                               hasAlpha:YES
                                               isPlanar:NO
                                         colorSpaceName:NSCalibratedRGBColorSpace
                                            bitmapFormat:0
                                             bytesPerRow:rowBytes
                                            bitsPerPixel:32];
    if (representation == nil || representation.bitmapData == nullptr) {
      return false;
    }
    const auto pixmap = frame.makePixmap();
    const auto* source = static_cast<const uint8_t*>(pixmap.pixels());
    if (source == nullptr || pixmap.rowBytes() < rowBytes) {
      return false;
    }
    for (int row = 0; row < height; ++row) {
      std::memcpy(representation.bitmapData +
                    static_cast<size_t>(row) * representation.bytesPerRow,
        source + static_cast<size_t>(row) * pixmap.rowBytes(), rowBytes);
    }

    NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    [image addRepresentation:representation];

    // Fit the frame on the screen, keeping its aspect ratio; a 4K render is otherwise larger than
    // the display and the title bar lands off-screen.
    NSScreen* screen = [NSScreen mainScreen];
    const NSRect visible = screen != nil ? screen.visibleFrame : NSMakeRect(0, 0, 1280, 720);
    const double fit = MIN(1.0, MIN(visible.size.width * 0.88 / width,
                                    visible.size.height * 0.88 / height));
    const NSRect contentRect =
      NSMakeRect(0, 0, static_cast<CGFloat>(width) * fit, static_cast<CGFloat>(height) * fit);
    NSWindow* window =
      [[NSWindow alloc] initWithContentRect:contentRect
                                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                            NSWindowStyleMaskMiniaturizable |
                                            NSWindowStyleMaskResizable
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
    window.title = [NSString stringWithUTF8String:title.c_str()];
    window.backgroundColor = [NSColor blackColor];
    window.contentAspectRatio = NSMakeSize(width, height);
    // NSWindow releases itself when it closes by default. Under ARC that is an over-release, which
    // crashes in the autorelease pool the moment the window goes away; ownership is this function's.
    window.releasedWhenClosed = NO;
    window.delegate = delegate;

    CoralSubtitleImageView* view = [[CoralSubtitleImageView alloc] initWithFrame:contentRect];
    view.image = image;
    view.imageScaling = NSImageScaleProportionallyUpOrDown;
    view.imageAlignment = NSImageAlignCenter;
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    view.wantsLayer = YES;
    view.layer.backgroundColor = [[NSColor blackColor] CGColor];
    window.contentView = view;

    [window center];
    [window makeKeyAndOrderFront:nil];
    [window makeFirstResponder:view];
    [NSApp activateIgnoringOtherApps:YES];

    if (seconds > 0.0) {
      [NSTimer scheduledTimerWithTimeInterval:seconds
                                      repeats:NO
                                        block:^(NSTimer* timer) {
                                          stopApplication();
                                        }];
    }

    [NSApp run];

    window.delegate = nil;
    [window orderOut:nil];
    [window close];
    return true;
  }
}

} // namespace demo
} // namespace coral
