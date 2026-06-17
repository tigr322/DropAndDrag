#include "layout.hpp"

#include <cmath>

namespace dd {

std::vector<ItemLayout> LayoutEngine::calculateLayout(
    float container_width,
    float container_height,
    size_t item_count,
    float item_size,
    float spacing,
    float padding,
    LayoutDirection direction
) const {
    std::vector<ItemLayout> layouts;
    layouts.reserve(item_count);

    switch (direction) {
        case LayoutDirection::Horizontal: {
            float x = padding;
            const float y = padding;
            const float step = item_size + spacing;

            for (size_t i = 0; i < item_count; ++i) {
                if (x + item_size > container_width - padding) {
                    break;
                }
                layouts.emplace_back(x, y, item_size, item_size);
                x += step;
            }
            break;
        }
        case LayoutDirection::Vertical: {
            float y = padding;
            const float x = padding;
            const float step = item_size + spacing;

            for (size_t i = 0; i < item_count; ++i) {
                if (y + item_size > container_height - padding) {
                    break;
                }
                layouts.emplace_back(x, y, item_size, item_size);
                y += step;
            }
            break;
        }
        case LayoutDirection::Grid: {
            const int cols = std::max(1, static_cast<int>((container_width - padding * 2.0f + spacing) / (item_size + spacing)));
            const float step = item_size + spacing;

            for (size_t i = 0; i < item_count; ++i) {
                const int col = static_cast<int>(i) % cols;
                const int row = static_cast<int>(i) / cols;
                const float x = padding + col * step;
                const float y = padding + row * step;

                if (y + item_size > container_height - padding) {
                    break;
                }
                layouts.emplace_back(x, y, item_size, item_size);
            }
            break;
        }
    }

    return layouts;
}

std::vector<ItemLayout> LayoutEngine::calculateGridLayout(
    float container_width,
    float container_height,
    size_t item_count,
    float item_size,
    float spacing,
    float padding,
    int columns
) const {
    std::vector<ItemLayout> layouts;
    layouts.reserve(item_count);

    const float step = item_size + spacing;
    const int cols = std::max(1, columns);

    for (size_t i = 0; i < item_count; ++i) {
        const int col = static_cast<int>(i) % cols;
        const int row = static_cast<int>(i) / cols;
        const float x = padding + col * step;
        const float y = padding + row * step;

        if (y + item_size > container_height - padding) {
            break;
        }
        layouts.emplace_back(x, y, item_size, item_size);
    }

    return layouts;
}

std::pair<size_t, size_t> LayoutEngine::calculateVisibleRange(
    float scroll_offset,
    float viewport_width,
    float viewport_height,
    float item_size,
    float spacing,
    const std::vector<ItemLayout>& layouts
) const {
    if (layouts.empty()) return {0, 0};

    const float step = item_size + spacing;

    size_t first = static_cast<size_t>(std::max(0.0f, std::floor(scroll_offset / step)));

    size_t rows_in_view = static_cast<size_t>(std::ceil(viewport_height / step)) + 1;
    const int cols = std::max(1, static_cast<int>((viewport_width - LayoutConstants::kPadding * 2.0f + spacing) / step));
    size_t visible_count = rows_in_view * cols;

    if (first >= layouts.size()) first = layouts.size() - 1;

    size_t last = std::min(layouts.size(), first + visible_count);

    return {first, last};
}

float LayoutEngine::getTotalContentSize(
    size_t item_count,
    float item_size,
    float spacing,
    float padding,
    LayoutDirection direction
) const {
    if (item_count == 0) return 0.0f;

    switch (direction) {
        case LayoutDirection::Horizontal:
            return padding * 2.0f + item_count * (item_size + spacing) - spacing;
        case LayoutDirection::Vertical:
            return padding * 2.0f + item_count * (item_size + spacing) - spacing;
        case LayoutDirection::Grid:
            return padding * 2.0f + item_count * (item_size + spacing) - spacing;
    }
    return 0.0f;
}

float LayoutEngine::getTotalContentHeight(
    size_t item_count,
    float item_size,
    float spacing,
    float padding,
    float container_width,
    int columns
) const {
    if (item_count == 0) return 0.0f;

    const int cols = std::max(1, columns);
    const size_t rows = (item_count + cols - 1) / cols;
    return padding * 2.0f + rows * (item_size + spacing) - spacing;
}

} // namespace dd
