#pragma once
#include <core/items/item.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace dd {

using ItemList = std::vector<Item>;

class Renderer {
public:
    Renderer() = default;
    ~Renderer();
    bool init(void* view, int w, int h);
    void shutdown();
    void render(float dt);
    int width() const { return width_; }
    int height() const { return height_; }
    bool initialized() const { return ok_; }
    void setItems(const ItemList& items);
    ItemList items() const;
    // Wire the clear-button action after init(). Stored behind a shared_ptr so
    // the ObjC block installed in init() always sees the latest callback even
    // when setClearCallback is called after init().
    void setClearCallback(std::function<void()> cb);
private:
    int width_ = 400;
    int height_ = 120;
    bool ok_ = false;
    std::shared_ptr<ItemList>              shared_items_{ std::make_shared<ItemList>() };
    std::shared_ptr<std::function<void()>> clearCallback_{ std::make_shared<std::function<void()>>() };
};

} // namespace dd
