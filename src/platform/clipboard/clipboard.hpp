#pragma once

// clipboard.hpp — ClipboardManager abstract interface.
// Platform implementations: clipboard_mac.mm, clipboard_win.cpp, clipboard_linux.cpp.


#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

class ClipboardManager {
public:
    static ClipboardManager& instance();

    ClipboardManager(const ClipboardManager&) = delete;
    ClipboardManager& operator=(const ClipboardManager&) = delete;
    ClipboardManager(ClipboardManager&&) = delete;
    ClipboardManager& operator=(ClipboardManager&&) = delete;

    void copy(std::string_view text);
    void copyFile(std::string_view path);
    void copyImage(const std::vector<uint8_t>& data);

    [[nodiscard]] std::string paste() const;
    [[nodiscard]] bool hasText() const;
    [[nodiscard]] bool hasImage() const;

private:
    ClipboardManager() = default;
    ~ClipboardManager() = default;
};

} // namespace dd
