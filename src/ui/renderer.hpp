#pragma once

// renderer.hpp — C++ facade over the Objective-C++ Skia/Cocoa rendering layer.
//
// Renderer owns the drawing pipeline and all frame-level caches (icons,
// thumbnails, labels, background path).  It is initialised once with a native
// view handle (NSView* as void*) and communicates back to the Application via
// callbacks stored behind shared_ptr so ObjC blocks always see the latest value.
//
// All public methods must be called on the main thread.
//
// Rendering is reactive — drawRect: fires only when setNeedsDisplay:YES is
// called explicitly; there is no polling render loop.  Callers that mutate
// visible state (setItems, setHideCallback/result, etc.) are responsible for
// triggering a redraw where needed; setItems() calls setNeedsDisplay:YES
// automatically.
//
// Callback wiring pattern (shared_ptr indirection):
//   The ObjC blocks installed in init() capture shared_ptr<function<void()>>.
//   When Application calls setClearCallback()/setHideCallback() after init(),
//   the new function is written through the shared_ptr and the already-installed
//   block picks it up on next invocation — no re-init required.

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

    // Initialise the renderer for the given native view (NSView* on macOS).
    // w, h are the initial logical dimensions in points.
    // Returns false if the view is null or the renderer fails to set up.
    bool init(void* view, int w, int h);

    // Release all caches and disconnect from the view.
    void shutdown();

    // Trigger an immediate redraw (calls setNeedsDisplay:YES on the view).
    // dt is unused — kept for API symmetry with a timer-driven render loop.
    void render(float dt);

    // Replace the full item list.  Clears the label cache, resets scroll to 0
    // if the list is empty, and calls setNeedsDisplay:YES.
    void setItems(const ItemList& items);

    // Return a snapshot of the current item list.
    [[nodiscard]] ItemList items() const;

    [[nodiscard]] int  width()       const { return width_; }
    [[nodiscard]] int  height()      const { return height_; }
    [[nodiscard]] bool initialized() const { return ok_; }

    // --- Action callbacks (wired by Application after init) ---

    // Called when the user clicks the "Clear" button.
    // Application uses this to reset the window to its default size.
    void setClearCallback(std::function<void()> cb);

    // Called when the user clicks the "Hide" button (amber circle, top-left).
    // Application must call native_window_->hide() AND set_shelf_visible(false)
    // so the CGEventTap guard resets and shake detection resumes.
    void setHideCallback(std::function<void()> cb);

private:
    int  width_  = 400;
    int  height_ = 120;
    bool ok_     = false;

    // Shared with the ObjC blocks installed during init().
    // Writing through these pointers after init() is safe and effective.
    std::shared_ptr<ItemList>              shared_items_  { std::make_shared<ItemList>() };
    std::shared_ptr<std::function<void()>> clearCallback_ { std::make_shared<std::function<void()>>() };
    std::shared_ptr<std::function<void()>> hideCallback_  { std::make_shared<std::function<void()>>() };
};

} // namespace dd
