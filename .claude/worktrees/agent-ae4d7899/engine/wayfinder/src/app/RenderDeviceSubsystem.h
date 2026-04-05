#pragma once

#include "app/AppSubsystem.h"
#include "core/Result.h"
#include "wayfinder_exports.h"

#include <memory>

namespace Wayfinder
{
    class EngineContext;
    class RenderDevice;

    /**
     * @brief AppSubsystem wrapping the GPU RenderDevice.
     *
     * Owns the RenderDevice instance and manages its lifecycle. Requires
     * the Rendering capability for activation and depends on WindowSubsystem
     * (the swapchain needs a window surface for initialisation).
     */
    class WAYFINDER_API RenderDeviceSubsystem : public AppSubsystem
    {
    public:
        RenderDeviceSubsystem() = default;
        ~RenderDeviceSubsystem() override;

        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
        void Shutdown() override;

        /// @pre Valid only after Initialise() and before Shutdown().
        [[nodiscard]] auto GetDevice() -> RenderDevice&;

    private:
        std::unique_ptr<RenderDevice> m_device;
    };

} // namespace Wayfinder
