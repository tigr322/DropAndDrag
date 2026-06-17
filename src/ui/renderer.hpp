#pragma once
#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/ganesh/GrBackendSurface.h>
#include <include/gpu/ganesh/GrDirectContext.h>
#include <include/gpu/ganesh/SkSurfaceGanesh.h>
#include <include/gpu/ganesh/mtl/GrMtlBackendContext.h>
#include <include/gpu/ganesh/mtl/GrMtlBackendSurface.h>
#include <include/gpu/ganesh/mtl/GrMtlDirectContext.h>
#include <core/items/item.hpp>
#include "theme.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace dd {

struct Item;
using ItemList = std::vector<Item>;
using ItemCallback = std::function<void(const Item&)>;

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    bool init(void* view, int w, int h);
    void resize(int w, int h);
    void shutdown();
    void render(float dt);

    int width() const { return width_; }
    int height() const { return height_; }
    bool initialized() const { return ok_; }

    void setItems(const ItemList& items) { items_ = items; }
    const ItemList& items() const { return items_; }

    void setItemDoubleClickCallback(ItemCallback cb) { on_double_click_ = std::move(cb); }

private:
    sk_sp<GrDirectContext> gpu_;
    sk_sp<SkSurface> surface_;
    void* metal_layer_ = nullptr;
    int width_ = 400;
    int height_ = 120;
    bool ok_ = false;
    ItemList items_;
    ItemCallback on_double_click_;
    float time_ = 0;
};

} // namespace dd
