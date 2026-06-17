#include "component.hpp"

namespace dd {

void Component::addChild(std::unique_ptr<Component> child) {
    if (!child) return;
    child->parent_ = this;
    children_.push_back(std::move(child));
}

void Component::removeChild(Component* child) {
    if (!child) return;

    auto it = std::find_if(children_.begin(), children_.end(),
                           [child](const auto& c) { return c.get() == child; });

    if (it != children_.end()) {
        (*it)->parent_ = nullptr;
        children_.erase(it);
    }
}

bool Component::propagateMouseEvent(const MouseEvent& event) {
    for (auto& child : children_) {
        if (child->handleEvent(event)) {
            return true;
        }
    }
    return false;
}

bool Component::propagateKeyEvent(const KeyEvent& event) {
    for (auto& child : children_) {
        if (child->handleKeyEvent(event)) {
            return true;
        }
    }
    return false;
}

bool Component::propagateDragEvent(const DragEvent& event) {
    for (auto& child : children_) {
        if (child->handleDragEvent(event)) {
            return true;
        }
    }
    return false;
}

} // namespace dd
