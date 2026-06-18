#pragma once

// tray.hpp — SystemTray abstract interface.
// Platform implementations: NSStatusBar (macOS), Shell_NotifyIcon (Win), libappindicator (Linux).


#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

struct MenuItem {
    std::string label;
    std::string action;
    bool enabled = true;
    bool checked = false;
    bool separator = false;
};

using TrayMenuCallback = std::function<void(std::string_view action)>;

class SystemTray {
public:
    static SystemTray& instance();

    SystemTray(const SystemTray&) = delete;
    SystemTray& operator=(const SystemTray&) = delete;
    SystemTray(SystemTray&&) = delete;
    SystemTray& operator=(SystemTray&&) = delete;

    void create(std::string_view icon_path, std::string_view tooltip);
    void show();
    void hide();
    void setMenu(const std::vector<MenuItem>& items);
    void setMenuCallback(TrayMenuCallback cb);
    void setTooltip(std::string_view tooltip);
    void setIcon(std::string_view icon_path);
    void update_tooltip(std::string_view tooltip);
    void update_icon(std::string_view icon_path);

private:
    SystemTray() = default;
    ~SystemTray() = default;

    TrayMenuCallback menu_callback_;
    bool visible_{false};
};

} // namespace dd
