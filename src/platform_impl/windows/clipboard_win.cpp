#include "platform/clipboard/clipboard.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

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

std::string toUtf8(std::wstring_view s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len, nullptr, nullptr);
    return result;
}

std::string getClipboardText() {
    if (!OpenClipboard(nullptr)) return {};
    std::string result;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        auto* text = static_cast<wchar_t*>(GlobalLock(hData));
        if (text) {
            result = toUtf8(text);
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

bool hasClipboardFormat(UINT format) {
    if (!OpenClipboard(nullptr)) return false;
    bool has = IsClipboardFormatAvailable(format) != 0;
    CloseClipboard();
    return has;
}

} // namespace

void clipboardCopy(std::string_view text) {
    auto wtext = toWide(text);
    size_t size = (wtext.size() + 1) * sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hGlobal) return;
    auto* dst = static_cast<wchar_t*>(GlobalLock(hGlobal));
    if (dst) {
        memcpy(dst, wtext.c_str(), size);
        GlobalUnlock(hGlobal);
    }
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, hGlobal);
        CloseClipboard();
    } else {
        GlobalFree(hGlobal);
    }
}

void clipboardCopyFile(std::string_view path) {
    auto wpath = toWide(path);
    size_t size = sizeof(DROPFILES) + (wpath.size() + 2) * sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size);
    if (!hGlobal) return;
    auto* df = static_cast<DROPFILES*>(GlobalLock(hGlobal));
    if (df) {
        df->pFiles = sizeof(DROPFILES);
        df->fWide = TRUE;
        auto* buf = reinterpret_cast<wchar_t*>(reinterpret_cast<char*>(df) + sizeof(DROPFILES));
        memcpy(buf, wpath.c_str(), wpath.size() * sizeof(wchar_t));
        GlobalUnlock(hGlobal);
    }
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        SetClipboardData(CF_HDROP, hGlobal);
        CloseClipboard();
    } else {
        GlobalFree(hGlobal);
    }
}

} // namespace win

ClipboardManager& ClipboardManager::instance() {
    static ClipboardManager mgr;
    return mgr;
}

void ClipboardManager::copy(std::string_view text) {
    win::clipboardCopy(text);
}

void ClipboardManager::copyFile(std::string_view path) {
    win::clipboardCopyFile(path);
}

void ClipboardManager::copyImage(const std::vector<uint8_t>& /*data*/) {
}

std::string ClipboardManager::paste() const {
    return win::getClipboardText();
}

bool ClipboardManager::hasText() const {
    return win::hasClipboardFormat(CF_UNICODETEXT) || win::hasClipboardFormat(CF_TEXT);
}

bool ClipboardManager::hasImage() const {
    return win::hasClipboardFormat(CF_DIB) || win::hasClipboardFormat(CF_BITMAP);
}

} // namespace dd
