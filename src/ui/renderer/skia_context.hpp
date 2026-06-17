#pragma once

#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/GrBackendSurface.h>
#include <include/gpu/GrDirectContext.h>
#include <include/gpu/ganesh/SkSurfaceGanesh.h>

#if defined(__APPLE__)
    #define SK_METAL
    #include <include/gpu/ganesh/mtl/GrMtlBackendContext.h>
    #include <include/gpu/ganesh/mtl/GrMtlBackendSurface.h>
    #include <include/gpu/ganesh/mtl/GrMtlDirectContext.h>
#elif defined(_WIN32)
    #define SK_DIRECT3D
    #include <include/gpu/ganesh/d3d/GrD3DBackendContext.h>
#else
    #define SK_VULKAN
    #include <include/gpu/vk/GrVkBackendContext.h>
    #include <include/gpu/vk/GrVkExtensions.h>
#endif

#include <memory>

namespace dd {

class SkiaContext {
public:
    SkiaContext() = default;
    ~SkiaContext();

    SkiaContext(const SkiaContext&) = delete;
    SkiaContext& operator=(const SkiaContext&) = delete;
    SkiaContext(SkiaContext&&) = delete;
    SkiaContext& operator=(SkiaContext&&) = delete;

    [[nodiscard]] bool init(void* native_window_handle, int width, int height, float dpi_scale = 1.0f);
    void resize(int width, int height);
    void shutdown();

    [[nodiscard]] SkCanvas* beginFrame();
    void endFrame();

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] float dpi_scale() const noexcept { return dpi_scale_; }
    [[nodiscard]] GrDirectContext* gpu_context() const noexcept { return context_.get(); }
    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

private:
    void create_surface();

    sk_sp<GrDirectContext> context_;
    sk_sp<SkSurface> surface_;
    SkCanvas* canvas_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    float dpi_scale_ = 1.0f;
    bool initialized_ = false;

#if defined(__APPLE__)
    void* metal_layer_ = nullptr;
#elif defined(_WIN32)
    void* d3d_device_ = nullptr;
    void* d3d_queue_ = nullptr;
    void* d3d_swap_chain_ = nullptr;
#else
    void* vulkan_window_ = nullptr;
#endif
};

} // namespace dd
