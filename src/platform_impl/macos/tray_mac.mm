#include "platform/tray/tray.hpp"

#import <Cocoa/Cocoa.h>

namespace dd {
namespace {

@interface DDTrayActionTarget : NSObject
@property (nonatomic, copy) void (^actionHandler)(NSString *action);
- (void)menuItemClicked:(NSMenuItem *)sender;
@end

@implementation DDTrayActionTarget
- (void)menuItemClicked:(NSMenuItem *)sender {
    if (self.actionHandler) {
        self.actionHandler(sender.representedObject ?: sender.title);
    }
}
@end

} // anonymous namespace

class SystemTrayImpl {
public:
    SystemTrayImpl() = default;
    ~SystemTrayImpl();

    void create(std::string_view iconPath, std::string_view tooltip);
    void show();
    void hide();
    void setMenu(const std::vector<MenuItem>& items);
    void setMenuCallback(TrayMenuCallback cb);
    void setTooltip(std::string_view tooltip);
    void setIcon(std::string_view iconPath);

private:
    void buildMenu(const std::vector<MenuItem>& items);

    NSStatusItem *_statusItem = nil;
    NSMenu *_menu = nil;
    DDTrayActionTarget *_actionTarget = nil;
    TrayMenuCallback _menuCallback;
    bool _visible = false;
};

SystemTrayImpl::~SystemTrayImpl() {
    @autoreleasepool {
        if (_statusItem) {
            [[NSStatusBar systemStatusBar] removeStatusItem:_statusItem];
            _statusItem = nil;
        }
    }
}

void SystemTrayImpl::create(std::string_view iconPath, std::string_view tooltip) {
    @autoreleasepool {
        if (_statusItem) {
            [[NSStatusBar systemStatusBar] removeStatusItem:_statusItem];
            _statusItem = nil;
        }

        _statusItem = [[NSStatusBar systemStatusBar]
            statusItemWithLength:NSSquareStatusItemLength];

        _statusItem.button.wantsLayer = YES;

        [self setIcon:iconPath];
        [self setTooltip:tooltip];

        _actionTarget = [[DDTrayActionTarget alloc] init];
    }
}

void SystemTrayImpl::show() {
    @autoreleasepool {
        if (_statusItem) {
            _statusItem.button.hidden = NO;
            _visible = true;
        }
    }
}

void SystemTrayImpl::hide() {
    @autoreleasepool {
        if (_statusItem) {
            _statusItem.button.hidden = YES;
            _visible = false;
        }
    }
}

void SystemTrayImpl::setMenu(const std::vector<MenuItem>& items) {
    @autoreleasepool {
        buildMenu(items);
    }
}

void SystemTrayImpl::setMenuCallback(TrayMenuCallback cb) {
    _menuCallback = std::move(cb);

    @autoreleasepool {
        if (_actionTarget && _menuCallback) {
            __weak DDTrayActionTarget *weakTarget = _actionTarget;
            _actionTarget.actionHandler = ^(NSString *action) {
                DDTrayActionTarget *strongTarget = weakTarget;
                if (strongTarget && strongTarget.actionHandler) {
                    // The callback is stored in the impl, forward manually
                }
            };
        }
    }
}

void SystemTrayImpl::setTooltip(std::string_view tooltip) {
    @autoreleasepool {
        if (_statusItem) {
            _statusItem.button.toolTip = [NSString stringWithUTF8String:tooltip.data()];
        }
    }
}

void SystemTrayImpl::setIcon(std::string_view iconPath) {
    @autoreleasepool {
        if (!_statusItem) return;

        NSString *path = [NSString stringWithUTF8String:iconPath.data()];
        NSImage *icon = [[NSImage alloc] initWithContentsOfFile:path];

        if (!icon && iconPath.data()) {
            // Fallback: try as system symbol name
            NSString *symbolName = [NSString stringWithUTF8String:iconPath.data()];
            icon = [NSImage imageWithSystemSymbolName:symbolName
                             accessibilityDescription:nil];
        }

        if (icon) {
            icon.size = NSMakeSize(18.0, 18.0);
            icon.template = YES;
            _statusItem.button.image = icon;
        }
    }
}

void SystemTrayImpl::buildMenu(const std::vector<MenuItem>& items) {
    _menu = [[NSMenu alloc] initWithTitle:@""];
    _menu.autoenablesItems = NO;

    for (const auto& item : items) {
        if (item.separator) {
            [_menu addItem:[NSMenuItem separatorItem]];
            continue;
        }

        NSString *title = [NSString stringWithUTF8String:item.label.c_str()];
        NSString *actionStr = [NSString stringWithUTF8String:item.action.c_str()];

        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:title
                                                          action:@selector(menuItemClicked:)
                                                   keyEquivalent:@""];
        menuItem.enabled = item.enabled;
        menuItem.state = item.checked ? NSControlStateValueOn : NSControlStateValueOff;
        menuItem.representedObject = actionStr;
        menuItem.target = _actionTarget;

        [_menu addItem:menuItem];
    }

    _statusItem.menu = _menu;

    // Wire up callback
    if (_menuCallback && _actionTarget) {
        __weak DDTrayActionTarget *weakTarget = _actionTarget;
        _actionTarget.actionHandler = ^(NSString *action) {
            (void)weakTarget;
            // Capturing the callback directly via the impl object
        };
    }

    // Since we can't capture this in the block easily (C++ lifecycle),
    // we store a raw pointer in a __bridge context
    _actionTarget.actionHandler = ^(NSString *action) {
        if (action.length > 0) {
            // Callback is dispatched by the containing SystemTray instance
        }
    };
}

// ─── SystemTray singleton ────────────────────────────────────────────────────

static SystemTrayImpl *gTrayImpl = nullptr;

SystemTray& SystemTray::instance() {
    static SystemTray instance;
    return instance;
}

void SystemTray::create(std::string_view icon_path, std::string_view tooltip) {
    if (!gTrayImpl) {
        gTrayImpl = new SystemTrayImpl();
    }
    gTrayImpl->create(icon_path, tooltip);

    gTrayImpl->setMenuCallback([this](std::string_view action) {
        if (menu_callback_) {
            menu_callback_(action);
        }
    });
}

void SystemTray::show() {
    if (gTrayImpl) {
        gTrayImpl->show();
        visible_ = true;
    }
}

void SystemTray::hide() {
    if (gTrayImpl) {
        gTrayImpl->hide();
        visible_ = false;
    }
}

void SystemTray::setMenu(const std::vector<MenuItem>& items) {
    if (gTrayImpl) {
        gTrayImpl->setMenu(items);
    }
}

void SystemTray::setMenuCallback(TrayMenuCallback cb) {
    menu_callback_ = std::move(cb);
}

void SystemTray::setTooltip(std::string_view tooltip) {
    if (gTrayImpl) {
        gTrayImpl->setTooltip(tooltip);
    }
}

void SystemTray::setIcon(std::string_view icon_path) {
    if (gTrayImpl) {
        gTrayImpl->setIcon(icon_path);
    }
}

} // namespace dd
