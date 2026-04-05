#pragma once

#include "app/AppSubsystem.h"
#include "core/Result.h"
#include "rendering/graph/Canvas.h"
#include "volumes/BlendableEffectRegistry.h"
#include "wayfinder_exports.h"

#include <memory>

namespace Wayfinder
{
    class EngineContext;
    class Renderer;

    /**
     * @brief AppSubsystem wrapping the Renderer, FrameCanvases, and BlendableEffectRegistry.
     *
     * Owns the Renderer, per-frame FrameCanvases, and BlendableEffectRegistry.
     * Requires the Rendering capability for activation and depends on
     * RenderDeviceSubsystem. Per D-07: no global SetActiveInstance pattern --
     * the registry is accessed via EngineContext -> RendererSubsystem -> GetBlendableEffectRegistry().
     */
    class WAYFINDER_API RendererSubsystem : public AppSubsystem
    {
    public:
        RendererSubsystem() = default;
        ~RendererSubsystem() override;

        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
        void Shutdown() override;

        /// @pre Valid only after Initialise() and before Shutdown().
        [[nodiscard]] auto GetRenderer() -> Renderer&;

        /// @pre Valid only after Initialise() and before Shutdown().
        [[nodiscard]] auto GetCanvases() -> FrameCanvases&;

        /// @pre Valid only after Initialise() and before Shutdown().
        [[nodiscard]] auto GetCanvases() const -> const FrameCanvases&;

        [[nodiscard]] auto GetBlendableEffectRegistry() -> BlendableEffectRegistry&;
        [[nodiscard]] auto GetBlendableEffectRegistry() const -> const BlendableEffectRegistry&;

    private:
        std::unique_ptr<Renderer> m_renderer;
        FrameCanvases m_canvases;
        BlendableEffectRegistry m_blendableEffectRegistry;
    };

} // namespace Wayfinder
