#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace dd {

enum class LayoutDirection : uint8_t {
    Horizontal,
    Vertical,
    Grid,
};

struct ItemLayout {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

namespace LayoutConstants {
    constexpr float kItemSize = 64.0f;
    constexpr float kThumbnailSize = 128.0f;
    constexpr float kPadding = 8.0f;
    constexpr float kSpacing = 4.0f;
    constexpr int kGridColumns = 4;
} // namespace LayoutConstants

class LayoutEngine {
public:
    LayoutEngine() = default;
    ~LayoutEngine() = default;

    [[nodiscard]] std::vector<ItemLayout> calculateLayout(
        float container_width,
        float container_height,
        size_t item_count,
        float item_size = LayoutConstants::kItemSize,
        float spacing = LayoutConstants::kSpacing,
        float padding = LayoutConstants::kPadding,
        LayoutDirection direction = LayoutDirection::Horizontal
    ) const;

    [[nodiscard]] std::vector<ItemLayout> calculateGridLayout(
        float container_width,
        float container_height,
        size_t item_count,
        float item_size = LayoutConstants::kThumbnailSize,
        float spacing = LayoutConstants::kSpacing,
        float padding = LayoutConstants::kPadding,
        int columns = LayoutConstants::kGridColumns
    ) const;

    [[nodiscard]] std::pair<size_t, size_t> calculateVisibleRange(
        float scroll_offset,
        float viewport_width,
        float viewport_height,
        float item_size,
        float spacing,
        const std::vector<ItemLayout>& layouts
    ) const;

    [[nodiscard]] float getTotalContentSize(
        size_t item_count,
        float item_size,
        float spacing,
        float padding,
        LayoutDirection direction
    ) const;

    [[nodiscard]] float getTotalContentHeight(
        size_t item_count,
        float item_size,
        float spacing,
        float padding,
        float container_width,
        int columns = LayoutConstants::kGridColumns
    ) const;
};

} // namespace dd
