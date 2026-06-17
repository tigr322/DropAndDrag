#pragma once

#include <include/core/SkCanvas.h>
#include <include/core/SkRect.h>

#include <memory>
#include <vector>

namespace dd {

struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    [[nodiscard]] constexpr bool contains(float px, float py) const noexcept {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }

    [[nodiscard]] constexpr SkRect toSkRect() const noexcept {
        return SkRect::MakeXYWH(x, y, width, height);
    }

    [[nodiscard]] static constexpr Rect fromSkRect(const SkRect& r) noexcept {
        return {r.x(), r.y(), r.width(), r.height()};
    }
};

struct MouseEvent {
    enum class Type : uint8_t {
        Move,
        Down,
        Up,
        Drag,
        Scroll,
        Enter,
        Exit,
    };

    Type type{Type::Move};
    float x = 0.0f;
    float y = 0.0f;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
    int button = 0;
    bool ctrl_down = false;
    bool shift_down = false;
    bool alt_down = false;
};

struct KeyEvent {
    enum class Type : uint8_t {
        Down,
        Up,
        Char,
    };

    Type type{Type::Down};
    int key_code = 0;
    uint32_t character = 0;
    bool ctrl_down = false;
    bool shift_down = false;
    bool alt_down = false;
};

struct DragEvent {
    enum class Type : uint8_t {
        Enter,
        Exit,
        Over,
        Drop,
    };

    Type type{Type::Over};
    float x = 0.0f;
    float y = 0.0f;
    std::vector<std::string> file_paths;
    std::string text_data;
    std::string url_data;
};

class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&) = default;
    Component& operator=(Component&&) = default;

    virtual void render(SkCanvas* canvas, const Rect& bounds) = 0;
    virtual bool handleEvent(const MouseEvent& event) { return false; }
    virtual bool handleKeyEvent(const KeyEvent& event) { return false; }
    virtual bool handleDragEvent(const DragEvent& event) { return false; }

    virtual void layout(const Rect& bounds) { bounds_ = bounds; }
    [[nodiscard]] virtual Size getDesiredSize() const { return {0.0f, 0.0f}; }

    [[nodiscard]] bool hitTest(float x, float y) const noexcept {
        return bounds_.contains(x, y);
    }

    void addChild(std::unique_ptr<Component> child);
    void removeChild(Component* child);

    [[nodiscard]] Component* parent() const noexcept { return parent_; }
    [[nodiscard]] const std::vector<std::unique_ptr<Component>>& children() const noexcept { return children_; }

protected:
    [[nodiscard]] const Rect& bounds() const noexcept { return bounds_; }

    bool propagateMouseEvent(const MouseEvent& event);
    bool propagateKeyEvent(const KeyEvent& event);
    bool propagateDragEvent(const DragEvent& event);

    Rect bounds_;
    Component* parent_ = nullptr;
    std::vector<std::unique_ptr<Component>> children_;
};

} // namespace dd
