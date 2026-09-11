#import <Cocoa/Cocoa.h>
#include <QWindow>

void installEdwardTitlebar(QWindow *window) {
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    NSWindow *native = view.window;
    if (!native) return;
    native.titleVisibility = NSWindowTitleVisible;
    native.titlebarAppearsTransparent = NO;
    NSTitlebarAccessoryViewController *accessory = [NSTitlebarAccessoryViewController new];
    NSStackView *stack = [NSStackView stackViewWithViews:@[]];
    stack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    stack.spacing = 8;
    stack.edgeInsets = NSEdgeInsetsMake(0, 8, 0, 8);
    NSTextField *status = [NSTextField labelWithString:@"未命名项目 · 未连接"];
    status.textColor = [NSColor secondaryLabelColor];
    [stack addArrangedSubview:status];
    for (NSString *title in @[@"布局", @"S", @"M", @"L", @"设置", @"?", @"导出", @"中文"]) {
        NSButton *button = [NSButton buttonWithTitle:title target:nil action:nil];
        button.bezelStyle = NSBezelStyleTexturedRounded;
        button.controlSize = NSControlSizeSmall;
        [stack addArrangedSubview:button];
    }
    accessory.view = stack;
    [native addTitlebarAccessoryViewController:accessory];
}
