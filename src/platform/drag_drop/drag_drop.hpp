#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

enum class DragOperation : uint8_t {
    Copy,
    Move,
    Link,
    None,
};

enum class DropDataType : uint8_t {
    File,
    Text,
    Url,
    Image,
};

struct DropItemData {
    DropDataType type = DropDataType::File;
    std::string text;
    std::string file_path;
    std::string url;
    std::vector<uint8_t> image_data;
    std::optional<uint64_t> file_size;
};

using DragEnterCallback = std::function<void(int x, int y, DragOperation default_op)>;
using DragOverCallback = std::function<DragOperation(int x, int y)>;
using DragLeaveCallback = std::function<void()>;
using DropCallback = std::function<void(std::vector<DropItemData> items)>;

class DragDropManager {
public:
    static DragDropManager& instance();

    DragDropManager(const DragDropManager&) = delete;
    DragDropManager& operator=(const DragDropManager&) = delete;
    DragDropManager(DragDropManager&&) = delete;
    DragDropManager& operator=(DragDropManager&&) = delete;

    void setDragEnterCallback(DragEnterCallback cb);
    void setDragOverCallback(DragOverCallback cb);
    void setDragLeaveCallback(DragLeaveCallback cb);
    void setDropCallback(DropCallback cb);

    DragOperation onDragEnter(int x, int y, DragOperation default_op);
    DragOperation onDragOver(int x, int y);
    void onDragLeave();
    std::vector<DropItemData> onDrop();

    static DragOperation defaultOperation();
    static std::string operationToString(DragOperation op);

    [[nodiscard]] bool isDragActive() const;

private:
    DragDropManager() = default;
    ~DragDropManager() = default;

    DragEnterCallback drag_enter_cb_;
    DragOverCallback drag_over_cb_;
    DragLeaveCallback drag_leave_cb_;
    DropCallback drop_cb_;
};

} // namespace dd
