#include "tray.hpp"

namespace dd {

SystemTray& SystemTray::instance() {
    static SystemTray tray;
    return tray;
}

void SystemTray::create(std::string_view /*icon_path*/, std::string_view /*tooltip*/) {
}

void SystemTray::show() {
    visible_ = true;
}

void SystemTray::hide() {
    visible_ = false;
}

void SystemTray::setMenu(const std::vector<MenuItem>& /*items*/) {
}

void SystemTray::setMenuCallback(TrayMenuCallback cb) {
    menu_callback_ = std::move(cb);
}

void SystemTray::setTooltip(std::string_view /*tooltip*/) {
}

void SystemTray::setIcon(std::string_view /*icon_path*/) {
}

} // namespace dd
