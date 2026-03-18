#include "SDLGPUDevice.h"
#include "../null/NullDevice.h"
#include "../../platform/Window.h"

#include "../../core/Log.h"

#include <SDL3/SDL.h>

namespace Wayfinder
{
    // ── Factory ──────────────────────────────────────────────

    std::unique_ptr<RenderDevice> RenderDevice::Create(RenderBackend backend)
    {
        switch (backend)
        {
        case RenderBackend::SDL_GPU:
            return std::make_unique<SDLGPUDevice>();
        case RenderBackend::Null:
            return std::make_unique<NullDevice>();
        }

        return nullptr;
    }

    // ── Lifecycle ────────────────────────────────────────────

    SDLGPUDevice::~SDLGPUDevice()
    {
        Shutdown();
    }

    bool SDLGPUDevice::Initialize(Window& window)
    {
        m_window = static_cast<SDL_Window*>(window.GetNativeHandle());
        if (!m_window)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Window has no valid native handle");
            return false;
        }

        m_device = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
            true, // debug mode
            nullptr);

        if (!m_device)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to create GPU device — {}", SDL_GetError());
            return false;
        }

        if (!SDL_ClaimWindowForGPUDevice(m_device, m_window))
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to claim window for GPU device — {}", SDL_GetError());
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            return false;
        }

        m_info.BackendName = "SDL_GPU";

        const char* driver = SDL_GetGPUDeviceDriver(m_device);
        m_info.DriverInfo = driver ? driver : "unknown";

        WAYFINDER_INFO(LogRenderer, "SDLGPUDevice: Initialized (driver: {})", m_info.DriverInfo);
        return true;
    }

    void SDLGPUDevice::Shutdown()
    {
        if (m_device)
        {
            if (m_window)
            {
                SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
            }

            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            m_window = nullptr;
        }
    }

    // ── Frame Lifecycle ──────────────────────────────────────

    bool SDLGPUDevice::BeginFrame()
    {
        m_commandBuffer = SDL_AcquireGPUCommandBuffer(m_device);
        if (!m_commandBuffer)
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to acquire command buffer — {}", SDL_GetError());
            return false;
        }

        if (!SDL_AcquireGPUSwapchainTexture(m_commandBuffer, m_window, &m_swapchainTexture, &m_swapchainWidth, &m_swapchainHeight))
        {
            WAYFINDER_ERROR(LogRenderer, "SDLGPUDevice: Failed to acquire swapchain texture — {}", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(m_commandBuffer);
            m_commandBuffer = nullptr;
            return false;
        }

        if (!m_swapchainTexture)
        {
            // Window is minimized or occluded — submit empty command buffer and skip rendering
            SDL_SubmitGPUCommandBuffer(m_commandBuffer);
            m_commandBuffer = nullptr;
            return false;
        }

        return true;
    }

    void SDLGPUDevice::EndFrame()
    {
        if (m_commandBuffer)
        {
            SDL_SubmitGPUCommandBuffer(m_commandBuffer);
            m_commandBuffer = nullptr;
        }

        m_swapchainTexture = nullptr;
    }

    // ── Render Pass ──────────────────────────────────────────

    void SDLGPUDevice::BeginRenderPass(const RenderPassDescriptor& descriptor)
    {
        if (!m_commandBuffer || !m_swapchainTexture)
        {
            return;
        }

        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = m_swapchainTexture;
        colorTarget.clear_color.r = descriptor.colorAttachment.clearValue.r;
        colorTarget.clear_color.g = descriptor.colorAttachment.clearValue.g;
        colorTarget.clear_color.b = descriptor.colorAttachment.clearValue.b;
        colorTarget.clear_color.a = descriptor.colorAttachment.clearValue.a;

        switch (descriptor.colorAttachment.loadOp)
        {
        case LoadOp::Clear:    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR; break;
        case LoadOp::Load:     colorTarget.load_op = SDL_GPU_LOADOP_LOAD; break;
        case LoadOp::DontCare: colorTarget.load_op = SDL_GPU_LOADOP_DONT_CARE; break;
        }

        switch (descriptor.colorAttachment.storeOp)
        {
        case StoreOp::Store:    colorTarget.store_op = SDL_GPU_STOREOP_STORE; break;
        case StoreOp::DontCare: colorTarget.store_op = SDL_GPU_STOREOP_DONT_CARE; break;
        }

        m_renderPass = SDL_BeginGPURenderPass(m_commandBuffer, &colorTarget, 1, nullptr);
    }

    void SDLGPUDevice::EndRenderPass()
    {
        if (m_renderPass)
        {
            SDL_EndGPURenderPass(m_renderPass);
            m_renderPass = nullptr;
        }
    }

} // namespace Wayfinder
