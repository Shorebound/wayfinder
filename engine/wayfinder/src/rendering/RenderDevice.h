#pragma once

#include <memory>

#include "RenderTypes.h"

namespace Wayfinder
{
    class Window;

    class WAYFINDER_API RenderDevice
    {
    public:
        virtual ~RenderDevice() = default;

        // ── Lifecycle ────────────────────────────────────────

        virtual bool Initialize(Window& window) = 0;
        virtual void Shutdown() = 0;

        // ── Frame Lifecycle ──────────────────────────────────

        // Acquires a command buffer and the swapchain texture for this frame.
        // Returns false if the swapchain is unavailable (e.g. window minimized). 
        // When false, skip rendering and call EndFrame().
        virtual bool BeginFrame() = 0;

        // Submits the command buffer and presents the swapchain.
        virtual void EndFrame() = 0;

        // ── Render Pass ──────────────────────────────────────

        virtual void BeginRenderPass(const RenderPassDescriptor& descriptor) = 0;
        virtual void EndRenderPass() = 0;

        // ── Device Info ──────────────────────────────────────

        virtual const RenderDeviceInfo& GetDeviceInfo() const = 0;

        // ── Factory ──────────────────────────────────────────

        static std::unique_ptr<RenderDevice> Create(RenderBackend backend);
    };

} // namespace Wayfinder
