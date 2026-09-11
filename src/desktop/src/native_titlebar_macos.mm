#import <Cocoa/Cocoa.h>
#include <QWindow>

void installEdwardTitlebar(QWindow *window) {
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    NSWindow *native = view.window;
    if (!native) return;
    native.titleVisibility = NSWindowTitleHidden;
    native.titlebarAppearsTransparent = YES;
    native.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    NSTitlebarAccessoryViewController *accessory = [NSTitlebarAccessoryViewController new];
    accessory.layoutAttribute = NSLayoutAttributeRight;
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
        button.contentTintColor = [NSColor colorNamed:@"controlAccentColor"] ?: [NSColor colorWithCalibratedRed:1.0 green:0.43 blue:0.0 alpha:1.0];
        [stack addArrangedSubview:button];
    }
    accessory.view = stack;
    [native addTitlebarAccessoryViewController:accessory];
}
