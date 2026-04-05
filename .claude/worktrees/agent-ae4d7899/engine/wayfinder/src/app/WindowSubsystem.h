#pragma once

#include "app/AppSubsystem.h"
#include "core/Result.h"
#include "wayfinder_exports.h"

#include <memory>

namespace Wayfinder
{
    class EngineContext;
    class Window;

    /**
     * @brief AppSubsystem wrapping the platform Window.
     *
     * Owns the Window instance and manages its lifecycle. Requires the
     * Presentation capability for activation. Must be initialised before
     * RenderDeviceSubsystem (swapchain needs a window surface).
     */
    class WAYFINDER_API WindowSubsystem : public AppSubsystem
    {
    public:
        WindowSubsystem() = default;
        ~WindowSubsystem() override;

        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
        void Shutdown() override;

        /// @pre Valid only after Initialise() and before Shutdown().
        [[nodiscard]] auto GetWindow() -> Window&;

        /// @pre Valid only after Initialise() and before Shutdown().
        [[nodiscard]] auto ShouldClose() const -> bool;

    private:
        std::unique_ptr<Window> m_window;
    };

} // namespace Wayfinder
