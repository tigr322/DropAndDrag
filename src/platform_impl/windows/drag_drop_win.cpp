#include "platform/drag_drop/drag_drop.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
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

bool hasFormat(IDataObject* pDataObj, CLIPFORMAT cf) {
    FORMATETC fmt{};
    fmt.cfFormat = cf;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;
    return SUCCEEDED(pDataObj->QueryGetData(&fmt));
}

DragOperation determineOperation(DWORD grfKeyState) {
    if (grfKeyState & MK_CONTROL) return DragOperation::Copy;
    if (grfKeyState & MK_SHIFT) return DragOperation::Move;
    if (grfKeyState & MK_ALT) return DragOperation::Link;
    return DragOperation::Copy;
}

std::vector<DropItemData> parseDropData(IDataObject* pDataObj) {
    std::vector<DropItemData> items;

    CLIPFORMAT cfHdrop = CF_HDROP;
    CLIPFORMAT cfText = CF_UNICODETEXT;

    if (hasFormat(pDataObj, cfHdrop)) {
        FORMATETC fmt{};
        fmt.cfFormat = cfHdrop;
        fmt.dwAspect = DVASPECT_CONTENT;
        fmt.lindex = -1;
        fmt.tymed = TYMED_HGLOBAL;

        STGMEDIUM stg{};
        if (SUCCEEDED(pDataObj->GetData(&fmt, &stg))) {
            HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
            if (hDrop) {
                UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < fileCount; ++i) {
                    UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
                    std::wstring wpath(len + 1, L'\0');
                    DragQueryFileW(hDrop, i, wpath.data(), static_cast<UINT>(wpath.size()));
                    wpath.resize(len);

                    DropItemData item;
                    item.type = DropDataType::File;
                    item.file_path = toUtf8(wpath);

                    WIN32_FILE_ATTRIBUTE_DATA attr{};
                    if (GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &attr)) {
                        ULARGE_INTEGER size{};
                        size.LowPart = attr.nFileSizeLow;
                        size.HighPart = attr.nFileSizeHigh;
                        item.file_size = size.QuadPart;
                    }
                    items.push_back(std::move(item));
                }
                GlobalUnlock(stg.hGlobal);
            }
            ReleaseStgMedium(&stg);
        }
    }

    if (hasFormat(pDataObj, cfText)) {
        FORMATETC fmt{};
        fmt.cfFormat = cfText;
        fmt.dwAspect = DVASPECT_CONTENT;
        fmt.lindex = -1;
        fmt.tymed = TYMED_HGLOBAL;

        STGMEDIUM stg{};
        if (SUCCEEDED(pDataObj->GetData(&fmt, &stg))) {
            auto* text = static_cast<wchar_t*>(GlobalLock(stg.hGlobal));
            if (text) {
                DropItemData item;
                item.type = DropDataType::Text;
                item.text = toUtf8(text);
                items.push_back(std::move(item));
                GlobalUnlock(stg.hGlobal);
            }
            ReleaseStgMedium(&stg);
        }
    }

    return items;
}

} // namespace

static std::mutex g_dropMutex;
static std::vector<DropItemData> g_pendingDropItems;

class WinDropTarget final : public IDropTarget {
public:
    WinDropTarget() : refCount_(1) {
        OleInitialize(nullptr);
    }

    ~WinDropTarget() override {
        OleUninitialize();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return ++refCount_;
    }

    STDMETHODIMP_(ULONG) Release() override {
        LONG c = --refCount_;
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState,
                           POINTL pt, DWORD* pdwEffect) override {
        auto& mgr = DragDropManager::instance();
        DragOperation op = determineOperation(grfKeyState);
        mgr.onDragEnter(pt.x, pt.y, op);
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        auto& mgr = DragDropManager::instance();
        DragOperation op = determineOperation(grfKeyState);
        op = mgr.onDragOver(pt.x, pt.y);
        switch (op) {
        case DragOperation::Copy: *pdwEffect = DROPEFFECT_COPY; break;
        case DragOperation::Move: *pdwEffect = DROPEFFECT_MOVE; break;
        case DragOperation::Link: *pdwEffect = DROPEFFECT_LINK; break;
        case DragOperation::None: *pdwEffect = DROPEFFECT_NONE; break;
        }
        return S_OK;
    }

    STDMETHODIMP DragLeave() override {
        DragDropManager::instance().onDragLeave();
        return S_OK;
    }

    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD, POINTL, DWORD* pdwEffect) override {
        auto items = parseDropData(pDataObj);
        {
            std::lock_guard lock(g_dropMutex);
            g_pendingDropItems = std::move(items);
        }
        DragDropManager::instance().onDrop();
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }

    void registerOnWindow(HWND hwnd) {
        RegisterDragDrop(hwnd, static_cast<IDropTarget*>(this));
    }

    void revokeOnWindow(HWND hwnd) {
        RevokeDragDrop(hwnd);
    }

private:
    std::atomic<LONG> refCount_;
};

std::vector<DropItemData> takeDropItems() {
    std::lock_guard lock(g_dropMutex);
    return std::move(g_pendingDropItems);
}

} // namespace win
} // namespace dd
