#pragma once

#include "core/Result.h"
#include "volumes/BlendableEffectRegistry.h"

#include <memory>

namespace Wayfinder
{
    class AssetService;
    class Input;
    class RenderDevice;
    class Renderer;
    class SceneRenderExtractor;
    class Scene;
    class Time;
    class Window;
    class EngineContext;
    struct EngineConfig;
    struct ProjectDescriptor;

    /**
     * @brief Owns platform and rendering services.
     *
     * EngineRuntime bundles Window, Input, Time, RenderDevice, Renderer, and
     * SceneRenderExtractor under a single lifecycle.  It can be created
     * without Application — enabling headless integration tests, the editor,
     * and any other host that needs engine services without the full
     * application shell.
     */
    class WAYFINDER_API EngineRuntime
    {
    public:
        /**
         * @brief Constructs an EngineRuntime.
         * @param config  Engine configuration. The caller must keep this alive
         *                for the lifetime of the EngineRuntime.
         * @param project Project descriptor. The caller must keep this alive
         *                for the lifetime of the EngineRuntime.
         */
        EngineRuntime(const EngineConfig& config, const ProjectDescriptor& project);
        ~EngineRuntime();

        // ── Lifecycle ────────────────────────────────────────
        /**
         * @brief Create and initialise platform and rendering services:
         *        Input, Time, Window, RenderDevice, Renderer, and
         *        SceneRenderExtractor.
         * @return A successful Result on success, or an Error describing the
         *         first subsystem that failed to initialise (e.g. window
         *         creation, device or renderer initialisation).
         */
        Result<void> Initialise();
        void Shutdown();

        // ── Per-frame ────────────────────────────────────────
        void BeginFrame();
        void EndFrame();

        // ── Rendering convenience ────────────────────────────
        void RenderScene(const Scene& scene);
        void SetAssetService(const std::shared_ptr<AssetService>& assetService);

        // ── Queries ──────────────────────────────────────────
        bool ShouldClose() const;
        float GetDeltaTime() const;

        // ── Non-owning accessors ─────────────────────────────
        /// @pre Valid only after Initialise() and before Shutdown().
        Window& GetWindow();
        /// @pre Valid only after Initialise() and before Shutdown().
        Input& GetInput();
        /// @pre Valid only after Initialise() and before Shutdown().
        Time& GetTime();
        /// @pre Valid only after Initialise() and before Shutdown().
        RenderDevice& GetDevice();
        /// @pre Valid only after Initialise() and before Shutdown().
        Renderer& GetRenderer();

        /**
         * @brief Returns the runtime's `BlendableEffectRegistry` (`m_blendableEffectRegistry`).
         * @pre Valid only after Initialise() and before Shutdown().
         */
        BlendableEffectRegistry& GetBlendableEffectRegistry()
        {
            return m_blendableEffectRegistry;
        }
        /**
         * @brief Returns the runtime's `BlendableEffectRegistry` (`m_blendableEffectRegistry`).
         * @pre Valid only after Initialise() and before Shutdown().
         */
        const BlendableEffectRegistry& GetBlendableEffectRegistry() const
        {
            return m_blendableEffectRegistry;
        }

    private:
        const EngineConfig& m_config;

        std::unique_ptr<Input> m_input;
        std::unique_ptr<Time> m_time;
        std::unique_ptr<Window> m_window;
        std::unique_ptr<RenderDevice> m_device;
        std::unique_ptr<Renderer> m_renderer;
        std::unique_ptr<SceneRenderExtractor> m_extractor;

        BlendableEffectRegistry m_blendableEffectRegistry;
    };

} // namespace Wayfinder
