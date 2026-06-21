// notifications_linux.cpp — Linux notifications stub (libnotify implementation pending).

#include "platform/notifications/notifications.hpp"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>

namespace dd {

namespace {

void send_dbus_notification(std::string_view title, std::string_view message) {
    std::string cmd = "notify-send";
    cmd += " '";
    cmd += title;
    cmd += "' '";
    cmd += message;
    cmd += "' --app-name=DropAndDrag --icon=dropanddrag --urgency=normal";
    system(cmd.c_str());
}

} // namespace

Notifications& Notifications::instance() {
    static Notifications notif;
    return notif;
}

void Notifications::show(std::string_view title, std::string_view message) {
    send_dbus_notification(title, message);
}

void Notifications::showWithAction(std::string_view title, std::string_view message,
                                   std::string_view action_label,
                                   NotificationActionCallback callback) {
    send_dbus_notification(title, message);

    if (callback) {
        callback();
    }
}

} // namespace dd
