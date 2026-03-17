#pragma once

#include "../GraphicsContext.h"

namespace Wayfinder
{
    class NullGraphicsContext final : public IGraphicsContext
    {
    public:
        NullGraphicsContext() = default;
        ~NullGraphicsContext() override = default;

        void Initialize() override {}
        void Shutdown() override {}
        const RenderBackendCapabilities& GetCapabilities() const override { return m_capabilities; }
        void BeginFrame() override {}
        void EndFrame() override {}
        void Clear(float, float, float, float) override {}

    private:
        RenderBackendCapabilities m_capabilities{"Null", 1, true, true, false, true, true};
    };
}