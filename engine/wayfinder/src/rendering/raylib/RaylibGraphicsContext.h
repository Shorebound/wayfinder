#pragma once
#include "../GraphicsContext.h"

namespace Wayfinder
{
    class RaylibGraphicsContext : public IGraphicsContext
    {
    public:
        RaylibGraphicsContext() = default;
        virtual ~RaylibGraphicsContext() = default;

        void Initialize() override;
        void Shutdown() override;
        const RenderBackendCapabilities& GetCapabilities() const override;
        
        void BeginFrame() override;
        void EndFrame() override;
        void Clear(float r, float g, float b, float a = 1.0f) override;

    private:
        RenderBackendCapabilities m_capabilities{
            .BackendName = "Raylib",
            .MaxViewCount = 1,
            .SupportsScenePasses = true,
            .SupportsDebugPasses = true,
            .SupportsRenderTargets = false,
            .SupportsBoxGeometry = true,
            .SupportsDebugLines = true,
        };
    };
}
