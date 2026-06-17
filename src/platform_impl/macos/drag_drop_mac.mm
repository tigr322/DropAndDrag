#include "platform/drag_drop/drag_drop.hpp"

#import <AppKit/AppKit.h>

namespace dd {

DragDropManager& DragDropManager::instance() {
    static DragDropManager mgr;
    return mgr;
}

void DragDropManager::setDragEnterCallback(DragEnterCallback cb) {
    drag_enter_cb_ = std::move(cb);
}

void DragDropManager::setDragOverCallback(DragOverCallback cb) {
    drag_over_cb_ = std::move(cb);
}

void DragDropManager::setDragLeaveCallback(DragLeaveCallback cb) {
    drag_leave_cb_ = std::move(cb);
}

void DragDropManager::setDropCallback(DropCallback cb) {
    drop_cb_ = std::move(cb);
}

DragOperation DragDropManager::onDragEnter(int x, int y, DragOperation default_op) {
    if (drag_enter_cb_) drag_enter_cb_(x, y, default_op);
    return DragOperation::Copy;
}

DragOperation DragDropManager::onDragOver(int x, int y) {
    if (drag_over_cb_) return drag_over_cb_(x, y);
    return DragOperation::Copy;
}

void DragDropManager::onDragLeave() {
    if (drag_leave_cb_) drag_leave_cb_();
}

std::vector<DropItemData> DragDropManager::onDrop() {
    if (drop_cb_) drop_cb_({});
    return {};
}

DragOperation DragDropManager::defaultOperation() {
    return DragOperation::Copy;
}

std::string DragDropManager::operationToString(DragOperation op) {
    switch (op) {
        case DragOperation::Copy: return "Copy";
        case DragOperation::Move: return "Move";
        case DragOperation::Link: return "Link";
        case DragOperation::None: return "None";
    }
    return "None";
}

bool DragDropManager::isDragActive() const {
    return false;
}

} // namespace dd
