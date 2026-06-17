#pragma once

#include <include/core/SkCanvas.h>
#include <include/core/SkRRect.h>

#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace dd {

struct Item;
class Theme;

enum class MenuAction : uint8_t {
    Open,
    RevealInFolder,
    CopyPath,
    CopyContent,
    Rename,
    AddTag,
    RemoveTag,
    AddToFavorites,
    RemoveFromFavorites,
    Delete,
    ClearShelf,
};

struct MenuEntry {
    MenuAction action{};
    std::string label;
    bool enabled = true;
    bool separator = false;
    std::vector<MenuEntry> children;
};

using MenuCallback = std::function<void(MenuAction, const Item&)>;

class ContextMenu {
public:
    ContextMenu() = default;
    ~ContextMenu() = default;

    ContextMenu(const ContextMenu&) = delete;
    ContextMenu& operator=(const ContextMenu&) = delete;
    ContextMenu(ContextMenu&&) = delete;
    ContextMenu& operator=(ContextMenu&&) = delete;

    void show(int x, int y, const Item& context);
    void hide();
    [[nodiscard]] bool isVisible() const noexcept { return visible_; }

    void render(SkCanvas* canvas, const Theme& theme);

    void handleMouseMove(float x, float y);
    void handleMouseDown(float x, float y);
    void handleKey(int key_code);
    [[nodiscard]] bool handleCharInput(uint32_t character);

    void setCallback(MenuCallback callback) { callback_ = std::move(callback); }

    [[nodiscard]] const Item& contextItem() const noexcept { return context_item_; }

private:
    void buildMenu(const Item& context);
    void calculateLayout(int screen_width, int screen_height);
    [[nodiscard]] int entryAt(float x, float y) const;

    struct EntryLayout {
        SkRect bounds;
        int depth = 0;
        int parent_index = -1;
    };

    void renderEntry(SkCanvas* canvas, const Theme& theme, size_t index,
                     const MenuEntry& entry, const SkFont& font);

    bool visible_ = false;
    Item context_item_;
    std::vector<MenuEntry> entries_;
    std::vector<EntryLayout> layouts_;
    SkRect menu_bounds_;
    int hovered_index_ = -1;
    int selected_index_ = -1;
    int submenu_open_index_ = -1;
    MenuCallback callback_;

    static constexpr float kEntryHeight = 28.0f;
    static constexpr float kMenuWidth = 220.0f;
    static constexpr float kSubmenuWidth = 160.0f;
    static constexpr float kCornerRadius = 8.0f;
    static constexpr float kSeparatorHeight = 1.0f;
    static constexpr float kPaddingX = 12.0f;
    static constexpr float kPaddingY = 6.0f;
    static constexpr float kShadowSigma = 12.0f;
    static constexpr float kActionIndent = 20.0f;
};

} // namespace dd
