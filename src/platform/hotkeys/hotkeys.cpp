#include "hotkeys.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace dd {

HotkeyManager& HotkeyManager::instance() {
    static HotkeyManager mgr;
    return mgr;
}

bool HotkeyManager::registerHotkey(uint8_t modifiers, int key_code) {
    (void)modifiers;
    (void)key_code;
    registered_ = true;
    return true;
}

void HotkeyManager::unregisterHotkey() {
    registered_ = false;
}

bool HotkeyManager::isRegistered() const {
    return registered_;
}

void HotkeyManager::setCallback(HotkeyCallback cb) {
    callback_ = std::move(cb);
}

std::optional<HotkeyDefinition> HotkeyManager::parseHotkeyString(std::string_view str) {
    HotkeyDefinition def;
    std::string s{str};
    std::istringstream ss(s);
    std::string token;

    while (std::getline(ss, token, '+')) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);

        auto lower = std::string(token.size(), '\0');
        std::transform(token.begin(), token.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower == "ctrl" || lower == "control") {
            def.modifiers = def.modifiers | Modifier::Ctrl;
        } else if (lower == "alt") {
            def.modifiers = def.modifiers | Modifier::Alt;
        } else if (lower == "shift") {
            def.modifiers = def.modifiers | Modifier::Shift;
        } else if (lower == "meta" || lower == "cmd" || lower == "command" || lower == "win" || lower == "windows") {
            def.modifiers = def.modifiers | Modifier::Meta;
        } else {
            if (token.length() == 1) {
                def.key_code = static_cast<int>(std::toupper(static_cast<unsigned char>(token[0])));
            } else {
                def.key_code = 0;
            }
        }
    }

    def.display_string = std::string(str);
    return def;
}

} // namespace dd
