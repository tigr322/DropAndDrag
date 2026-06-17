#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace dd {

enum class Modifier : uint8_t {
    Ctrl = 1 << 0,
    Alt = 1 << 1,
    Shift = 1 << 2,
    Meta = 1 << 3,
};

constexpr uint8_t operator|(Modifier a, Modifier b) {
    return static_cast<uint8_t>(a) | static_cast<uint8_t>(b);
}

constexpr uint8_t operator|(uint8_t a, Modifier b) {
    return a | static_cast<uint8_t>(b);
}

constexpr bool operator&(uint8_t a, Modifier b) {
    return (a & static_cast<uint8_t>(b)) != 0;
}

struct HotkeyDefinition {
    uint8_t modifiers = 0;
    int key_code = 0;
    std::string display_string;
};

using HotkeyCallback = std::function<void()>;

class HotkeyManager {
public:
    static HotkeyManager& instance();

    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;
    HotkeyManager(HotkeyManager&&) = delete;
    HotkeyManager& operator=(HotkeyManager&&) = delete;

    [[nodiscard]] bool registerHotkey(uint8_t modifiers, int key_code);
    void unregisterHotkey();
    [[nodiscard]] bool isRegistered() const;

    void setCallback(HotkeyCallback cb);

    [[nodiscard]] bool check_key_event(void* event_ptr);

    [[nodiscard]] static std::optional<HotkeyDefinition> parseHotkeyString(std::string_view str);

private:
    HotkeyManager() = default;
    ~HotkeyManager() = default;

    HotkeyCallback callback_;
    bool registered_{false};
};

} // namespace dd
