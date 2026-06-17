#include "skia_context.hpp"

#include <include/core/SkSurface.h>
#include <include/gpu/ganesh/GrBackendSurface.h>
#include <include/gpu/ganesh/GrDirectContext.h>
#include <include/gpu/ganesh/SkSurfaceGanesh.h>

#if defined(__APPLE__)
    #include <QuartzCore/CAMetalLayer.h>
    #include <include/gpu/ganesh/mtl/GrMtlBackendContext.h>
    #include <include/gpu/ganesh/mtl/GrMtlBackendSurface.h>
    #include <include/gpu/ganesh/mtl/GrMtlDirectContext.h>
#elif defined(_WIN32)
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <d3d11.h>
    #include <dxgi1_2.h>
    #include <include/gpu/ganesh/d3d/GrD3DBackendContext.h>
#else
    #include <include/gpu/ganesh/vk/GrVkBackendContext.h>
#endif

namespace dd {

SkiaContext::~SkiaContext() {
    shutdown();
}

bool SkiaContext::init(void* native_window_handle, int width, int height, float dpi_scale) {
    if (initialized_) return true;

    width_ = width;
    height_ = height;
    dpi_scale_ = dpi_scale;

#if defined(__APPLE__)
    metal_layer_ = native_window_handle;

    GrMtlBackendContext backend_ctx;
    backend_ctx.fDevice.retain(MTLCreateSystemDefaultDevice());
    backend_ctx.fQueue.retain([id<MTLDevice>(backend_ctx.fDevice.get()) newCommandQueue]);

    context_ = GrDirectContexts::MakeMetal(backend_ctx);
#elif defined(_WIN32)
    IDXGIAdapter* adapter = static_cast<IDXGIAdapter*>(native_window_handle);

    GrD3DBackendContext backend_ctx{};
    D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

    D3D11CreateDevice(
        adapter, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
        feature_levels, _countof(feature_levels),
        D3D11_SDK_VERSION,
        reinterpret_cast<ID3D11Device**>(&d3d_device_), nullptr,
        reinterpret_cast<ID3D11DeviceContext**>(&d3d_queue_)
    );

    context_ = GrDirectContexts::MakeDirect3D(backend_ctx);
#else
    vulkan_window_ = native_window_handle;

    GrVkBackendContext vk_ctx{};
    context_ = GrDirectContexts::MakeVulkan(vk_ctx);
#endif

    if (!context_) {
        return false;
    }

    create_surface();

    if (!surface_) {
        return false;
    }

    initialized_ = true;
    return true;
}

void SkiaContext::resize(int width, int height) {
    width_ = width;
    height_ = height;
    create_surface();
}

void SkiaContext::shutdown() {
    surface_.reset();
    canvas_ = nullptr;

    if (context_) {
        context_->flushAndSubmit(GrSyncCpu::kNo);
        context_.reset();
    }

#if defined(__APPLE__)
    metal_layer_ = nullptr;
#elif defined(_WIN32)
    if (d3d_swap_chain_) {
        static_cast<IDXGISwapChain*>(d3d_swap_chain_)->Release();
        d3d_swap_chain_ = nullptr;
    }
    if (d3d_queue_) {
        static_cast<ID3D11DeviceContext*>(d3d_queue_)->Release();
        d3d_queue_ = nullptr;
    }
    if (d3d_device_) {
        static_cast<ID3D11Device*>(d3d_device_)->Release();
        d3d_device_ = nullptr;
    }
#else
    vulkan_window_ = nullptr;
#endif

    initialized_ = false;
}

SkCanvas* SkiaContext::beginFrame() {
    if (!surface_) return nullptr;

    canvas_ = surface_->getCanvas();
    return canvas_;
}

void SkiaContext::endFrame() {
    if (!context_ || !surface_) return;

    context_->flushAndSubmit(GrSyncCpu::kNo);
}

void SkiaContext::create_surface() {
    if (!context_) return;

    surface_.reset();

    const int scaled_w = static_cast<int>(width_ * dpi_scale_);
    const int scaled_h = static_cast<int>(height_ * dpi_scale_);

#if defined(__APPLE__)
    if (metal_layer_) {
        GrMtlTextureInfo info;
        info.fTexture.retain(nullptr);

        GrBackendTexture backend_tex(
            scaled_w, scaled_h, skgpu::Mipmapped::kNo, info
        );

        surface_ = SkSurfaces::WrapBackendTexture(
            context_.get(),
            backend_tex,
            kTopLeft_GrSurfaceOrigin,
            1,
            kBGRA_8888_SkColorType,
            SkColorSpace::MakeSRGB(),
            nullptr, nullptr
        );
    }
#else
    surface_ = SkSurfaces::RenderTarget(
        context_.get(),
        skgpu::Budgeted::kYes,
        SkImageInfo::MakeN32Premul(scaled_w, scaled_h)
    );
#endif

    canvas_ = surface_ ? surface_->getCanvas() : nullptr;
}

} // namespace dd
