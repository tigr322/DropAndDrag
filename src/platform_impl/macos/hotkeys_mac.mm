// hotkeys_mac.mm — macOS global hotkeys via Carbon RegisterEventHotKey.
//
// Carbon hotkeys fire in all app states (frontmost or background), which is
// required since the shelf runs as an accessory (no activation on click).
// The Carbon event handler dispatches to HotkeyManager::instance().callback_.

#include "platform/hotkeys/hotkeys.hpp"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <sstream>

namespace dd {
namespace {

EventHotKeyRef gHotKeyRef = nullptr;
EventTargetRef gEventTarget = nullptr;
EventHandlerRef gEventHandler = nullptr;

OSStatus hotkeyHandler(EventHandlerCallRef /*nextHandler*/,
                       EventRef event,
                       void *userData) {
    (void)event;
    auto *callback = static_cast<HotkeyCallback *>(userData);
    if (callback && *callback) {
        (*callback)();
    }
    return noErr;
}

uint32_t carbonModifiers(uint8_t ddMods) {
    uint32_t result = 0;
    if (ddMods & static_cast<uint8_t>(Modifier::Ctrl))  result |= controlKey;
    if (ddMods & static_cast<uint8_t>(Modifier::Alt))   result |= optionKey;
    if (ddMods & static_cast<uint8_t>(Modifier::Shift)) result |= shiftKey;
    if (ddMods & static_cast<uint8_t>(Modifier::Meta))  result |= cmdKey;
    return result;
}

__attribute__((unused)) uint8_t modifierFromCarbon(uint32_t carbonMod) {
    uint8_t result = 0;
    if (carbonMod & controlKey) result |= static_cast<uint8_t>(Modifier::Ctrl);
    if (carbonMod & optionKey)  result |= static_cast<uint8_t>(Modifier::Alt);
    if (carbonMod & shiftKey)   result |= static_cast<uint8_t>(Modifier::Shift);
    if (carbonMod & cmdKey)     result |= static_cast<uint8_t>(Modifier::Meta);
    return result;
}

int keyCodeFromChar(unichar c) {
    // Map common characters to Carbon key codes
    switch (c) {
        case 'A': case 'a': return kVK_ANSI_A;
        case 'B': case 'b': return kVK_ANSI_B;
        case 'C': case 'c': return kVK_ANSI_C;
        case 'D': case 'd': return kVK_ANSI_D;
        case 'E': case 'e': return kVK_ANSI_E;
        case 'F': case 'f': return kVK_ANSI_F;
        case 'G': case 'g': return kVK_ANSI_G;
        case 'H': case 'h': return kVK_ANSI_H;
        case 'I': case 'i': return kVK_ANSI_I;
        case 'J': case 'j': return kVK_ANSI_J;
        case 'K': case 'k': return kVK_ANSI_K;
        case 'L': case 'l': return kVK_ANSI_L;
        case 'M': case 'm': return kVK_ANSI_M;
        case 'N': case 'n': return kVK_ANSI_N;
        case 'O': case 'o': return kVK_ANSI_O;
        case 'P': case 'p': return kVK_ANSI_P;
        case 'Q': case 'q': return kVK_ANSI_Q;
        case 'R': case 'r': return kVK_ANSI_R;
        case 'S': case 's': return kVK_ANSI_S;
        case 'T': case 't': return kVK_ANSI_T;
        case 'U': case 'u': return kVK_ANSI_U;
        case 'V': case 'v': return kVK_ANSI_V;
        case 'W': case 'w': return kVK_ANSI_W;
        case 'X': case 'x': return kVK_ANSI_X;
        case 'Y': case 'y': return kVK_ANSI_Y;
        case 'Z': case 'z': return kVK_ANSI_Z;
        case '0': return kVK_ANSI_0;
        case '1': return kVK_ANSI_1;
        case '2': return kVK_ANSI_2;
        case '3': return kVK_ANSI_3;
        case '4': return kVK_ANSI_4;
        case '5': return kVK_ANSI_5;
        case '6': return kVK_ANSI_6;
        case '7': return kVK_ANSI_7;
        case '8': return kVK_ANSI_8;
        case '9': return kVK_ANSI_9;
        case ' ': return kVK_Space;
        case '\t': return kVK_Tab;
        case '\r': return kVK_Return;
        case 0x1B: return kVK_Escape;
        default: return 0;
    }
}

} // anonymous namespace

HotkeyManager& HotkeyManager::instance() {
    static HotkeyManager instance;
    return instance;
}

bool HotkeyManager::registerHotkey(uint8_t modifiers, int key_code) {
    @autoreleasepool {
        unregisterHotkey();

        EventTypeSpec eventType;
        eventType.eventClass = kEventClassKeyboard;
        eventType.eventKind = kEventHotKeyPressed;

        gEventTarget = GetApplicationEventTarget();

        InstallEventHandler(gEventTarget,
                           &hotkeyHandler,
                           1, &eventType,
                           &callback_,
                           &gEventHandler);

        uint32_t carbonMods = carbonModifiers(modifiers);

        EventHotKeyID hotKeyID;
        hotKeyID.signature = 'DDHK';
        hotKeyID.id = 1;

        OSStatus status = RegisterEventHotKey((UInt32)key_code,
                                              carbonMods,
                                              hotKeyID,
                                              gEventTarget,
                                              0,
                                              &gHotKeyRef);

        if (status != noErr) {
            return false;
        }

        registered_ = true;
        return true;
    }
}

void HotkeyManager::unregisterHotkey() {
    @autoreleasepool {
        if (gHotKeyRef) {
            UnregisterEventHotKey(gHotKeyRef);
            gHotKeyRef = nullptr;
        }
        if (gEventHandler) {
            RemoveEventHandler(gEventHandler);
            gEventHandler = nullptr;
        }
        registered_ = false;
    }
}

bool HotkeyManager::isRegistered() const {
    return registered_;
}

void HotkeyManager::setCallback(HotkeyCallback cb) {
    callback_ = std::move(cb);
}

std::optional<HotkeyDefinition> HotkeyManager::parseHotkeyString(std::string_view str) {
    // The base class already has a parseHotkeyString implementation in hotkeys.cpp.
    // This overrides with macOS-specific key code mapping for better accuracy.
    HotkeyDefinition def;
    std::string s{str};
    std::istringstream ss(s);
    std::string token;

    while (std::getline(ss, token, '+')) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);

        std::string lower = token;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower == "ctrl" || lower == "control") {
            def.modifiers = def.modifiers | Modifier::Ctrl;
        } else if (lower == "alt" || lower == "option") {
            def.modifiers = def.modifiers | Modifier::Alt;
        } else if (lower == "shift") {
            def.modifiers = def.modifiers | Modifier::Shift;
        } else if (lower == "meta" || lower == "cmd" || lower == "command" || lower == "win" || lower == "windows") {
            def.modifiers = def.modifiers | Modifier::Meta;
        } else {
            if (token.length() == 1) {
                def.key_code = keyCodeFromChar(
                    static_cast<unichar>(std::toupper(static_cast<unsigned char>(token[0]))));
            } else if (token.length() > 1) {
                // Function keys etc.
                std::string upper = token;
                std::transform(upper.begin(), upper.end(), upper.begin(),
                              [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                if (upper == "F1") def.key_code = kVK_F1;
                else if (upper == "F2") def.key_code = kVK_F2;
                else if (upper == "F3") def.key_code = kVK_F3;
                else if (upper == "F4") def.key_code = kVK_F4;
                else if (upper == "F5") def.key_code = kVK_F5;
                else if (upper == "F6") def.key_code = kVK_F6;
                else if (upper == "F7") def.key_code = kVK_F7;
                else if (upper == "F8") def.key_code = kVK_F8;
                else if (upper == "F9") def.key_code = kVK_F9;
                else if (upper == "F10") def.key_code = kVK_F10;
                else if (upper == "F11") def.key_code = kVK_F11;
                else if (upper == "F12") def.key_code = kVK_F12;
                else if (upper == "SPACE") def.key_code = kVK_Space;
                else if (upper == "TAB") def.key_code = kVK_Tab;
                else if (upper == "RETURN" || upper == "ENTER") def.key_code = kVK_Return;
                else if (upper == "ESCAPE" || upper == "ESC") def.key_code = kVK_Escape;
                else if (upper == "DELETE" || upper == "BACKSPACE") def.key_code = kVK_Delete;
                else if (upper == "LEFT") def.key_code = kVK_LeftArrow;
                else if (upper == "RIGHT") def.key_code = kVK_RightArrow;
                else if (upper == "UP") def.key_code = kVK_UpArrow;
                else if (upper == "DOWN") def.key_code = kVK_DownArrow;
                else def.key_code = 0;
            }
        }
    }

    def.display_string = std::string(str);
    return def;
}

} // namespace dd
