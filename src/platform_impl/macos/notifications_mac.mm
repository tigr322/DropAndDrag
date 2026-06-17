#include "platform/notifications/notifications.hpp"

#import <UserNotifications/UserNotifications.h>

namespace dd {
namespace {

@interface DDNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@property (nonatomic, copy) void (^onAction)(void);
@end

@implementation DDNotificationDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler {
    (void)center;
    (void)notification;
    completionHandler(UNNotificationPresentationOptionBanner
                      | UNNotificationPresentationOptionSound);
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
didReceiveNotificationResponse:(UNNotificationResponse *)response
         withCompletionHandler:(void (^)(void))completionHandler {
    (void)center;
    if ([response.actionIdentifier isEqualToString:UNNotificationDefaultActionIdentifier]
        || [response.actionIdentifier isEqualToString:@"ACTION_BUTTON"]) {
        if (self.onAction) {
            self.onAction();
        }
    }
    completionHandler();
}

@end

DDNotificationDelegate *gDelegate = nil;
bool gAuthorized = false;

void requestAuthorization(dispatch_block_t completion) {
    UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];

    if (!gDelegate) {
        gDelegate = [[DDNotificationDelegate alloc] init];
        center.delegate = gDelegate;
    }

    [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert
                                             | UNAuthorizationOptionSound
                                             | UNAuthorizationOptionBadge)
                          completionHandler:^(BOOL granted, NSError *_Nullable error) {
        (void)error;
        gAuthorized = granted;
        if (completion) {
            dispatch_async(dispatch_get_main_queue(), completion);
        }
    }];
}

} // anonymous namespace

Notifications& Notifications::instance() {
    static Notifications instance;

    // Request authorization on first access
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        requestAuthorization(nil);
    });

    return instance;
}

void Notifications::show(std::string_view title, std::string_view message) {
    @autoreleasepool {
        if (!gAuthorized) {
            requestAuthorization(^{
                UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];

                UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
                content.title = [NSString stringWithUTF8String:title.data()];
                content.body = [NSString stringWithUTF8String:message.data()];
                content.sound = [UNNotificationSound defaultSound];

                // Fire immediately
                UNNotificationRequest *request = [UNNotificationRequest
                    requestWithIdentifier:[[NSUUID UUID] UUIDString]
                    content:content
                    trigger:nil];

                [center addNotificationRequest:request withCompletionHandler:nil];
            });
            return;
        }

        UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];

        UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
        content.title = [NSString stringWithUTF8String:title.data()];
        content.body = [NSString stringWithUTF8String:message.data()];
        content.sound = [UNNotificationSound defaultSound];

        UNNotificationRequest *request = [UNNotificationRequest
            requestWithIdentifier:[[NSUUID UUID] UUIDString]
            content:content
            trigger:nil];

        [center addNotificationRequest:request withCompletionHandler:nil];
    }
}

void Notifications::showWithAction(std::string_view title,
                                    std::string_view message,
                                    std::string_view action_label,
                                    NotificationActionCallback callback) {
    @autoreleasepool {
        // Register the action button category
        UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];

        NSString *actionId = @"ACTION_BUTTON";
        NSString *categoryId = @"ACTION_CATEGORY";

        UNNotificationAction *action = [UNNotificationAction
            actionWithIdentifier:actionId
            title:[NSString stringWithUTF8String:action_label.data()]
            options:UNNotificationActionOptionForeground];

        UNNotificationCategory *category = [UNNotificationCategory
            categoryWithIdentifier:categoryId
            actions:@[action]
            intentIdentifiers:@[]
            options:UNNotificationCategoryOptionNone];

        [center setNotificationCategories:[NSSet setWithObject:category]];

        // Wire the callback
        if (gDelegate && callback) {
            gDelegate.onAction = ^{
                callback();
            };
        }

        auto showNotification = ^{
            UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
            content.title = [NSString stringWithUTF8String:title.data()];
            content.body = [NSString stringWithUTF8String:message.data()];
            content.sound = [UNNotificationSound defaultSound];
            content.categoryIdentifier = categoryId;

            UNNotificationRequest *request = [UNNotificationRequest
                requestWithIdentifier:[[NSUUID UUID] UUIDString]
                content:content
                trigger:nil];

            [center addNotificationRequest:request withCompletionHandler:nil];
        };

        if (!gAuthorized) {
            requestAuthorization(^{
                dispatch_async(dispatch_get_main_queue(), showNotification);
            });
        } else {
            showNotification();
        }
    }
}

} // namespace dd
