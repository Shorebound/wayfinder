#pragma once

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
    struct EngineConfig;
    struct EngineContext;
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
        EngineRuntime(const EngineConfig& config, const ProjectDescriptor& project);
        ~EngineRuntime();

        // ── Lifecycle ────────────────────────────────────────
        bool Initialize();
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
        Window& GetWindow();
        Input& GetInput();
        Time& GetTime();
        RenderDevice& GetDevice();
        Renderer& GetRenderer();

        // ── Context bundle for external consumers (editor) ───
        EngineContext BuildContext() const;

    private:
        const EngineConfig& m_config;
        const ProjectDescriptor& m_project;

        std::unique_ptr<Input> m_input;
        std::unique_ptr<Time> m_time;
        std::unique_ptr<Window> m_window;
        std::unique_ptr<RenderDevice> m_device;
        std::unique_ptr<Renderer> m_renderer;
        std::unique_ptr<SceneRenderExtractor> m_extractor;
    };

} // namespace Wayfinder
