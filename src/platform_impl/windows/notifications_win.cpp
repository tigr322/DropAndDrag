#include "platform/notifications/notifications.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

namespace dd {
namespace win {
namespace {

std::wstring toWide(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

void showMessageBox(std::string_view title, std::string_view message,
                    std::string_view action_label,
                    std::function<void()> callback) {
    auto wtitle = toWide(title);
    auto wmessage = toWide(message);

    UINT type = MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND | MB_TOPMOST;
    if (!action_label.empty()) {
        type = MB_OKCANCEL | MB_ICONINFORMATION | MB_SETFOREGROUND | MB_TOPMOST;
    }

    int result = MessageBoxW(nullptr, wmessage.c_str(), wtitle.c_str(), type);
    if (result == IDOK && callback) {
        callback();
    }
}

} // namespace

void notificationShow(std::string_view title, std::string_view message) {
    showMessageBox(title, message, {}, {});
}

void notificationShowWithAction(std::string_view title, std::string_view message,
                                std::string_view action_label,
                                std::function<void()> callback) {
    showMessageBox(title, message, action_label, std::move(callback));
}

} // namespace win

Notifications& Notifications::instance() {
    static Notifications n;
    return n;
}

void Notifications::show(std::string_view title, std::string_view message) {
    win::notificationShow(title, message);
}

void Notifications::showWithAction(std::string_view title, std::string_view message,
                                   std::string_view action_label,
                                   NotificationActionCallback callback) {
    win::notificationShowWithAction(title, message, action_label, std::move(callback));
}

} // namespace dd
