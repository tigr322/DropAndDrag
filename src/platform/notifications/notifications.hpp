#pragma once

// notifications.hpp — Notifications abstract interface.
// Platform implementations: UNUserNotificationCenter (macOS), WinRT Toast (Win), libnotify (Linux).


#include <functional>
#include <string>
#include <string_view>

namespace dd {

using NotificationActionCallback = std::function<void()>;

class Notifications {
public:
    static Notifications& instance();

    Notifications(const Notifications&) = delete;
    Notifications& operator=(const Notifications&) = delete;
    Notifications(Notifications&&) = delete;
    Notifications& operator=(Notifications&&) = delete;

    void show(std::string_view title, std::string_view message);
    void showWithAction(std::string_view title, std::string_view message,
                        std::string_view action_label,
                        NotificationActionCallback callback);

private:
    Notifications() = default;
    ~Notifications() = default;
};

} // namespace dd
