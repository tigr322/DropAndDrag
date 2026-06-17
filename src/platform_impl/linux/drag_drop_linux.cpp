#include "platform/drag_drop/drag_drop.hpp"
#include "platform/window/native_window.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace dd {
namespace linux_dnd {
namespace {

constexpr std::string_view XDND_PROTOCOL_VERSION = "DndProtocol";
constexpr std::string_view XDND_ACTION_COPY = "XdndActionCopy";
constexpr std::string_view XDND_ACTION_MOVE = "XdndActionMove";

Atom XdndAware = 0;
Atom XdndEnter = 0;
Atom XdndPosition = 0;
Atom XdndStatus = 0;
Atom XdndLeave = 0;
Atom XdndDrop = 0;
Atom XdndFinished = 0;
Atom XdndSelection = 0;
Atom XdndTypeList = 0;
Atom XdndActionCopy = 0;
Atom XdndActionMove = 0;
Atom text_uri_list = 0;
Atom text_plain = 0;
Atom UTF8_STRING = 0;
Atom clipboard_atom = 0;
Atom XdndActionDescription = 0;

bool atoms_initialized = false;
std::mutex atom_mutex;

void init_atoms(Display* display) {
    std::lock_guard lock(atom_mutex);
    if (atoms_initialized) return;
    XdndAware = XInternAtom(display, "XdndAware", False);
    XdndEnter = XInternAtom(display, "XdndEnter", False);
    XdndPosition = XInternAtom(display, "XdndPosition", False);
    XdndStatus = XInternAtom(display, "XdndStatus", False);
    XdndLeave = XInternAtom(display, "XdndLeave", False);
    XdndDrop = XInternAtom(display, "XdndDrop", False);
    XdndFinished = XInternAtom(display, "XdndFinished", False);
    XdndSelection = XInternAtom(display, "XdndSelection", False);
    XdndTypeList = XInternAtom(display, "XdndTypeList", False);
    XdndActionCopy = XInternAtom(display, "XdndActionCopy", False);
    XdndActionMove = XInternAtom(display, "XdndActionMove", False);
    text_uri_list = XInternAtom(display, "text/uri-list", False);
    text_plain = XInternAtom(display, "text/plain", False);
    UTF8_STRING = XInternAtom(display, "UTF8_STRING", False);
    clipboard_atom = XInternAtom(display, "CLIPBOARD", False);
    XdndActionDescription = XInternAtom(display, "XdndActionDescription", False);
    atoms_initialized = true;
}

} // namespace
} // namespace linux_dnd

DragDropManager& DragDropManager::instance() {
    static DragDropManager mgr;
    return mgr;
}

void DragDropManager::register_window(void* native_handle) {
    if (!native_handle) return;
    auto display = static_cast<Display*>(native_handle);
    linux_dnd::init_atoms(display);
}

void DragDropManager::unregister_window(void* native_handle) {
}

bool DragDropManager::handle_client_message(void* display_ptr, void* event_ptr) {
    auto display = static_cast<Display*>(display_ptr);
    auto event = static_cast<XClientMessageEvent*>(event_ptr);
    auto msg_type = event->message_type;

    if (msg_type == linux_dnd::XdndEnter) {
        Window source = static_cast<Window>(event->data.l[0]);
        int version = (event->data.l[1] >> 24);
        bool has_type_list = (event->data.l[1] & 1) != 0;

        drag_source_window_ = source;
        drag_version_ = std::min(version, 5);
        drag_has_type_list_ = has_type_list;

        if (has_type_list) {
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            unsigned char* data = nullptr;
            XGetWindowProperty(display, source, linux_dnd::XdndTypeList,
                             0, 65536, False, XA_ATOM,
                             &actual_type, &actual_format, &nitems, &bytes_after, &data);
            if (data && actual_type == XA_ATOM) {
                auto atoms = reinterpret_cast<Atom*>(data);
                for (unsigned long i = 0; i < nitems; ++i) {
                    char* name = XGetAtomName(display, atoms[i]);
                    if (name) {
                        accepted_types_.insert(std::string(name));
                        XFree(name);
                    }
                }
            }
            if (data) XFree(data);
        } else {
            for (int i = 2; i < 5; ++i) {
                Atom atom = static_cast<Atom>(event->data.l[i]);
                if (atom != None) {
                    char* name = XGetAtomName(display, atom);
                    if (name) {
                        accepted_types_.insert(std::string(name));
                        XFree(name);
                    }
                }
            }
        }
        return true;
    }

    if (msg_type == linux_dnd::XdndPosition) {
        Window source = static_cast<Window>(event->data.l[0]);
        int x = (event->data.l[2] >> 16) & 0xFFFF;
        int y = event->data.l[2] & 0xFFFF;
        Atom action = static_cast<Atom>(event->data.l[4]);

        drag_x_ = x;
        drag_y_ = y;

        unsigned int mod_state = 0;
        Window root, child;
        int rx, ry, wx, wy;
        XQueryPointer(display, DefaultRootWindow(display), &root, &child, &rx, &ry, &wx, &wy, &mod_state);

        bool ctrl_held = (mod_state & ControlMask) != 0;
        Atom response_action = ctrl_held ? linux_dnd::XdndActionMove : linux_dnd::XdndActionCopy;

        XEvent xevent{};
        xevent.xany.type = ClientMessage;
        xevent.xany.display = display;
        xevent.xclient.window = source;
        xevent.xclient.message_type = linux_dnd::XdndStatus;
        xevent.xclient.format = 32;
        xevent.xclient.data.l[0] = DefaultRootWindow(display);
        xevent.xclient.data.l[1] = 1;
        xevent.xclient.data.l[2] = 0;
        xevent.xclient.data.l[3] = 0;
        xevent.xclient.data.l[4] = static_cast<long>(response_action);

        XSendEvent(display, source, False, NoEventMask, &xevent);
        return true;
    }

    if (msg_type == linux_dnd::XdndLeave) {
        drag_source_window_ = 0;
        accepted_types_.clear();
        return true;
    }

    if (msg_type == linux_dnd::XdndDrop) {
        Window source = static_cast<Window>(event->data.l[0]);
        int x = (event->data.l[2] >> 16) & 0xFFFF;
        int y = event->data.l[2] & 0xFFFF;

        drag_x_ = x;
        drag_y_ = y;

        Atom select_type = None;
        if (accepted_types_.count("text/uri-list")) {
            select_type = linux_dnd::text_uri_list;
        } else if (accepted_types_.count("UTF8_STRING")) {
            select_type = linux_dnd::UTF8_STRING;
        } else if (accepted_types_.count("text/plain")) {
            select_type = linux_dnd::text_plain;
        }

        if (select_type != None) {
            XConvertSelection(display, linux_dnd::XdndSelection,
                            select_type, linux_dnd::XdndSelection,
                            DefaultRootWindow(display), CurrentTime);
            drop_requested_ = true;
        }

        XEvent finish{};
        finish.xany.type = ClientMessage;
        finish.xany.display = display;
        finish.xclient.window = source;
        finish.xclient.message_type = linux_dnd::XdndFinished;
        finish.xclient.format = 32;
        finish.xclient.data.l[0] = DefaultRootWindow(display);
        finish.xclient.data.l[1] = 1;
        finish.xclient.data.l[2] = static_cast<long>(linux_dnd::XdndActionCopy);

        XSendEvent(display, source, False, NoEventMask, &finish);

        drag_source_window_ = 0;
        accepted_types_.clear();
        drop_requested_ = false;
        return true;
    }

    return false;
}

bool DragDropManager::handle_selection_notify(void* display_ptr, void* event_ptr) {
    if (!drop_requested_) return false;

    auto display = static_cast<Display*>(display_ptr);
    auto event = static_cast<XSelectionEvent*>(event_ptr);

    if (event->property == None) return false;

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* data = nullptr;

    XGetWindowProperty(display, DefaultRootWindow(display),
                      linux_dnd::XdndSelection,
                      0, 65536, True, AnyPropertyType,
                      &actual_type, &actual_format, &nitems, &bytes_after, &data);

    if (!data) return false;

    parsed_items_.clear();

    if (actual_type == linux_dnd::text_uri_list) {
        std::string uri_list(reinterpret_cast<char*>(data), nitems);
        size_t pos = 0;
        while (pos < uri_list.size()) {
            size_t end = uri_list.find("\r\n", pos);
            if (end == std::string::npos) end = uri_list.find('\n', pos);
            if (end == std::string::npos) end = uri_list.size();
            std::string uri = uri_list.substr(pos, end - pos);
            if (!uri.empty() && uri[0] != '#') {
                DropItemData item;
                item.type = DropDataType::File;
                if (uri.starts_with("file://")) {
                    std::string path = uri.substr(7);
                    std::string decoded;
                    for (size_t i = 0; i < path.size(); ++i) {
                        if (path[i] == '%' && i + 2 < path.size()) {
                            char hex[3] = {path[i+1], path[i+2], 0};
                            decoded += static_cast<char>(strtol(hex, nullptr, 16));
                            i += 2;
                        } else {
                            decoded += path[i];
                        }
                    }
                    item.file_path = decoded;
                }
                parsed_items_.push_back(std::move(item));
            }
            pos = end + ((end < uri_list.size() && uri_list[end] == '\r') ? 2 : 1);
        }
    } else if (actual_type == linux_dnd::UTF8_STRING || actual_type == linux_dnd::text_plain) {
        DropItemData item;
        item.type = DropDataType::Text;
        item.text = std::string(reinterpret_cast<char*>(data), nitems);
        parsed_items_.push_back(std::move(item));
    }

    XFree(data);
    return true;
}

std::vector<DropItemData> DragDropManager::take_dropped_items() {
    std::vector<DropItemData> result;
    result.swap(parsed_items_);
    return result;
}

DragOperation DragDropManager::drag_operation_for_modifiers(void* display_ptr) {
    auto display = static_cast<Display*>(display_ptr);
    Window root = DefaultRootWindow(display);
    Window child;
    int rx, ry, wx, wy;
    unsigned int mask;
    XQueryPointer(display, root, &root, &child, &rx, &ry, &wx, &wy, &mask);
    return (mask & ControlMask) ? DragOperation::Move : DragOperation::Copy;
}

bool DragDropManager::is_drag_active() const {
    return drag_source_window_ != 0;
}

} // namespace dd
