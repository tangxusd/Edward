#import <Cocoa/Cocoa.h>
#include <QWindow>
#include <QMetaObject>
#include <QVariant>
#include <objc/runtime.h>

static NSAttributedString *buttonTitle(NSString *title, NSColor *color);
static NSColor *accentColor();

@interface EdwardTitlebarTarget : NSObject
@property(nonatomic, assign) QWindow *window;
@property(nonatomic, copy) NSString *action;
@property(nonatomic, assign) BOOL english;
@end

@implementation EdwardTitlebarTarget
- (void)clicked:(id)sender {
    if (self.window) {
        NSString *dispatchedAction = self.action;
        if ([self.action isEqualToString:@"language"]) {
            self.english = !self.english;
            NSButton *button = (NSButton *)sender;
            button.title = self.english ? @"中文" : @"ENG";
            button.attributedTitle = buttonTitle(button.title, [NSColor whiteColor]);
            dispatchedAction = self.english ? @"language-en" : @"language-zh";
        }
        if ([self.action isEqualToString:@"S"] || [self.action isEqualToString:@"M"] || [self.action isEqualToString:@"L"]) {
            NSView *container = ((NSButton *)sender).superview;
            for (NSView *view in container.subviews) {
                if (![view isKindOfClass:[NSButton class]]) continue;
                NSButton *button = (NSButton *)view;
                if (!([button.title isEqualToString:@"S"] || [button.title isEqualToString:@"M"] || [button.title isEqualToString:@"L"])) continue;
                BOOL selected = [button.title isEqualToString:self.action];
                button.state = selected ? NSControlStateValueOn : NSControlStateValueOff;
                button.layer.borderWidth = 1.0;
                button.layer.borderColor = selected ? accentColor().CGColor : [NSColor colorWithCalibratedWhite:0.35 alpha:1.0].CGColor;
                button.layer.backgroundColor = selected ? [accentColor() colorWithAlphaComponent:0.2].CGColor : [NSColor clearColor].CGColor;
                button.attributedTitle = buttonTitle(button.title, selected ? accentColor() : [NSColor whiteColor]);
            }
        }
        QMetaObject::invokeMethod(self.window, "handleNativeTitlebarAction",
                                  Qt::QueuedConnection,
                                  Q_ARG(QVariant, QVariant(dispatchedAction.UTF8String)));
    }
}
@end

static char kEdwardTitlebarTargets;
static char kEdwardAuthButton;

static NSView *titlebarHost(NSWindow *window) {
    NSView *host = [window standardWindowButton:NSWindowCloseButton].superview;
    if (!host) return nil;
    while (host.superview &&
           (host.bounds.size.width < window.contentView.bounds.size.width * 0.7 ||
            host.bounds.size.height > 100.0)) {
        host = host.superview;
    }
    return host;
}

static NSColor *accentColor() {
    return [NSColor colorWithCalibratedRed:1.0 green:0.43 blue:0.0 alpha:1.0];
}

static NSAttributedString *buttonTitle(NSString *title, NSColor *color) {
    return [[NSAttributedString alloc] initWithString:title attributes:@{
        NSForegroundColorAttributeName: color,
        NSFontAttributeName: [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium]
    }];
}

void installEdwardTitlebar(QWindow *window, bool localServiceStarted) {
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    NSWindow *native = view.window;
    if (!native) return;
    native.titleVisibility = NSWindowTitleHidden;
    native.titlebarAppearsTransparent = YES;
    native.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    native.movableByWindowBackground = YES;

    NSView *host = titlebarHost(native);
    if (!host) return;

    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown handler:^NSEvent *(NSEvent *event) {
        if (event.window != native) return event;
        NSPoint point = [host convertPoint:event.locationInWindow fromView:nil];
        if (point.y < 0 || point.y > host.bounds.size.height) return event;
        NSView *hit = [host hitTest:point];
        if ([hit isKindOfClass:[NSButton class]] || [hit.superview isKindOfClass:[NSButton class]]) return event;
        if (event.clickCount >= 2) {
            [native performZoom:nil];
            return nil;
        }
        [native performWindowDragWithEvent:event];
        return nil;
    }];

    NSTextField *status = [NSTextField labelWithString:@""];
    NSString *stateText = localServiceStarted ? @"connected" : @"未连接";
    NSColor *stateColor = localServiceStarted ? [NSColor systemGreenColor] : [NSColor systemOrangeColor];
    NSMutableAttributedString *statusText = [[NSMutableAttributedString alloc] initWithString:[NSString stringWithFormat:@"未命名项目 · ● %@", stateText]];
    [statusText addAttribute:NSForegroundColorAttributeName value:[NSColor whiteColor] range:NSMakeRange(0, statusText.length)];
    [statusText addAttribute:NSFontAttributeName value:[NSFont systemFontOfSize:12.0 weight:NSFontWeightRegular] range:NSMakeRange(0, statusText.length)];
    NSRange dotRange = [[statusText string] rangeOfString:@"●"];
    [statusText addAttribute:NSForegroundColorAttributeName value:stateColor range:dotRange];
    status.attributedStringValue = statusText;
    status.alignment = NSTextAlignmentCenter;
    status.textColor = [NSColor whiteColor];
    status.translatesAutoresizingMaskIntoConstraints = NO;
    [host addSubview:status positioned:NSWindowAbove relativeTo:nil];

    NSStackView *buttons = [NSStackView stackViewWithViews:@[]];
    buttons.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    buttons.spacing = 6.0;
    buttons.translatesAutoresizingMaskIntoConstraints = NO;
    [host addSubview:buttons positioned:NSWindowAbove relativeTo:nil];

    NSMutableArray *targets = [NSMutableArray array];
    NSArray *titles = @[@"登录", @"布局", @"S", @"M", @"L", @"设置", @"导出", @"ENG"];
    NSArray *actions = @[@"login", @"layout", @"S", @"M", @"L", @"settings", @"export", @"language"];
    for (NSUInteger index = 0; index < titles.count; ++index) {
        NSString *title = titles[index];
        EdwardTitlebarTarget *target = [EdwardTitlebarTarget new];
        target.window = window;
        target.action = actions[index];
        [targets addObject:target];
        NSButton *button = [NSButton buttonWithTitle:title target:target action:@selector(clicked:)];
        button.bezelStyle = NSBezelStyleInline;
        button.controlSize = NSControlSizeSmall;
        button.bordered = NO;
        button.wantsLayer = YES;
        button.layer.backgroundColor = [NSColor clearColor].CGColor;
        button.layer.cornerRadius = 5.0;
        button.layer.borderColor = [NSColor colorWithCalibratedWhite:0.35 alpha:1.0].CGColor;
        button.layer.borderWidth = 1.0;
        [button.widthAnchor constraintGreaterThanOrEqualToConstant:24.0].active = YES;
        [button.heightAnchor constraintEqualToConstant:15.0].active = YES;
        button.attributedTitle = buttonTitle(title, [NSColor whiteColor]);
        if ([title isEqualToString:@"导出"]) {
            button.layer.backgroundColor = accentColor().CGColor;
            button.layer.borderWidth = 0.0;
            button.attributedTitle = buttonTitle(title, [NSColor whiteColor]);
        }
        if ([title isEqualToString:@"L"]) {
            button.state = NSControlStateValueOn;
            button.layer.backgroundColor = [accentColor() colorWithAlphaComponent:0.2].CGColor;
            button.layer.borderColor = accentColor().CGColor;
            button.layer.borderWidth = 1.0;
            button.attributedTitle = buttonTitle(title, accentColor());
        }
        [buttons addArrangedSubview:button];
        if ([title isEqualToString:@"登录"]) {
            objc_setAssociatedObject(host, &kEdwardAuthButton, button, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        }
    }
    objc_setAssociatedObject(host, &kEdwardTitlebarTargets, targets, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

    [NSLayoutConstraint activateConstraints:@[
        [status.centerXAnchor constraintEqualToAnchor:host.centerXAnchor],
        [status.centerYAnchor constraintEqualToAnchor:host.centerYAnchor],
        [status.leadingAnchor constraintGreaterThanOrEqualToAnchor:host.leadingAnchor constant:80.0],
        [buttons.trailingAnchor constraintEqualToAnchor:host.trailingAnchor constant:-8.0],
        [buttons.centerYAnchor constraintEqualToAnchor:host.centerYAnchor],
        [buttons.leadingAnchor constraintGreaterThanOrEqualToAnchor:status.trailingAnchor constant:24.0]
    ]];
}

void updateEdwardTitlebarAuthState(QWindow *window, bool authenticated) {
    if (!window) return;
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    NSWindow *native = view.window;
    NSView *host = native ? titlebarHost(native) : nil;
    NSButton *button = host ? objc_getAssociatedObject(host, &kEdwardAuthButton) : nil;
    if (!button) return;
    button.title = authenticated ? @"已登录" : @"登录";
    button.layer.backgroundColor = authenticated ? [NSColor colorWithCalibratedRed:0.10 green:0.65 blue:0.65 alpha:1.0].CGColor : [NSColor clearColor].CGColor;
    button.layer.borderColor = authenticated ? [NSColor colorWithCalibratedRed:0.22 green:0.82 blue:0.78 alpha:1.0].CGColor : [NSColor colorWithCalibratedWhite:0.35 alpha:1.0].CGColor;
    button.layer.borderWidth = 1.0;
    button.attributedTitle = buttonTitle(button.title, authenticated ? [NSColor colorWithCalibratedRed:0.02 green:0.10 blue:0.10 alpha:1.0] : [NSColor whiteColor]);
}
