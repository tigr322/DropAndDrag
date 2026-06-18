// tray_win.cpp — Windows system tray stub (Shell_NotifyIcon implementation pending).

#include "platform/tray/tray.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dd {
namespace win {
namespace {

constexpr UINT WM_TRAY_CALLBACK = WM_APP + 100;
constexpr UINT TRAY_UID = 0xDA01;

std::wstring toWide(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

HICON loadIconFromFile(std::string_view path) {
    auto wpath = toWide(path);
    return static_cast<HICON>(LoadImageW(nullptr, wpath.c_str(), IMAGE_ICON,
                                         0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
}

} // namespace

static HWND g_trayHwnd = nullptr;
static HICON g_trayIcon = nullptr;
static HMENU g_trayMenu = nullptr;
static std::function<void(std::string_view)> g_trayMenuCallback;
static std::vector<MenuItem> g_trayMenuItems;
static bool g_trayVisible = false;

void trayCreate(HWND hwnd, std::string_view icon_path, std::string_view tooltip) {
    g_trayHwnd = hwnd;

    if (g_trayIcon) {
        DestroyIcon(g_trayIcon);
        g_trayIcon = nullptr;
    }
    if (!icon_path.empty()) {
        g_trayIcon = loadIconFromFile(icon_path);
    }

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = TRAY_UID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_CALLBACK;
    nid.hIcon = g_trayIcon;
    if (!tooltip.empty()) {
        auto wtip = toWide(tooltip);
        wcsncpy_s(nid.szTip, wtip.c_str(), _TRUNCATE);
    }
    Shell_NotifyIconW(NIM_ADD, &nid);

    NOTIFYICONDATAW nidVer{};
    nidVer.cbSize = sizeof(NOTIFYICONDATAW);
    nidVer.hWnd = hwnd;
    nidVer.uID = TRAY_UID;
    nidVer.uFlags = NIF_INFO;
    nidVer.dwInfoFlags = NIIF_USER;
    nidVer.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nidVer);

    g_trayVisible = true;
}

void trayShow() {
    if (!g_trayHwnd || g_trayVisible) return;
    g_trayVisible = true;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = g_trayHwnd;
    nid.uID = TRAY_UID;
    nid.uFlags = NIF_ICON;
    nid.hIcon = g_trayIcon;
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void trayHide() {
    if (!g_trayHwnd || !g_trayVisible) return;
    g_trayVisible = false;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = g_trayHwnd;
    nid.uID = TRAY_UID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void traySetMenu(const std::vector<MenuItem>& items) {
    g_trayMenuItems = items;
    if (g_trayMenu) {
        DestroyMenu(g_trayMenu);
        g_trayMenu = nullptr;
    }

    g_trayMenu = CreatePopupMenu();
    UINT id = 1;
    for (const auto& item : items) {
        if (item.separator) {
            AppendMenuW(g_trayMenu, MF_SEPARATOR, 0, nullptr);
            ++id;
            continue;
        }
        UINT flags = MF_STRING;
        if (!item.enabled) flags |= MF_GRAYED;
        if (item.checked) flags |= MF_CHECKED;
        auto wlabel = toWide(item.label);
        AppendMenuW(g_trayMenu, flags, id++, wlabel.c_str());
    }
}

void traySetMenuCallback(std::function<void(std::string_view)> cb) {
    g_trayMenuCallback = std::move(cb);
}

void traySetTooltip(std::string_view tooltip) {
    if (!g_trayHwnd) return;
    auto wtip = toWide(tooltip);

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = g_trayHwnd;
    nid.uID = TRAY_UID;
    nid.uFlags = NIF_TIP;
    wcsncpy_s(nid.szTip, wtip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void traySetIcon(std::string_view icon_path) {
    if (g_trayIcon) {
        DestroyIcon(g_trayIcon);
        g_trayIcon = nullptr;
    }
    g_trayIcon = loadIconFromFile(icon_path);

    if (!g_trayHwnd) return;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = g_trayHwnd;
    nid.uID = TRAY_UID;
    nid.uFlags = NIF_ICON;
    nid.hIcon = g_trayIcon;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void trayHandleCallbackMessage(WPARAM /*wp*/, LPARAM lp) {
    if (LOWORD(lp) == WM_RBUTTONUP) {
        if (!g_trayMenu || !g_trayHwnd) return;

        POINT pt{};
        GetCursorPos(&pt);
        SetForegroundWindow(g_trayHwnd);
        UINT cmd = TrackPopupMenu(g_trayMenu,
                                  TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                  pt.x, pt.y, 0, g_trayHwnd, nullptr);
        PostMessageW(g_trayHwnd, WM_NULL, 0, 0);

        if (cmd > 0 && cmd <= g_trayMenuItems.size()) {
            if (g_trayMenuCallback) {
                g_trayMenuCallback(g_trayMenuItems[cmd - 1].action);
            }
        }
    } else if (LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == NIN_SELECT) {
        if (g_trayMenuCallback) {
            g_trayMenuCallback("toggle");
        }
    }
}

void trayCleanup() {
    if (g_trayHwnd) {
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = g_trayHwnd;
        nid.uID = TRAY_UID;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        g_trayHwnd = nullptr;
    }
    if (g_trayIcon) {
        DestroyIcon(g_trayIcon);
        g_trayIcon = nullptr;
    }
    if (g_trayMenu) {
        DestroyMenu(g_trayMenu);
        g_trayMenu = nullptr;
    }
    g_trayVisible = false;
}

} // namespace win
} // namespace dd
