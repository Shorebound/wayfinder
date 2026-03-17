#pragma once

#include "../RenderAPI.h"

namespace Wayfinder
{
    class NullRenderAPI final : public IRenderAPI
    {
    public:
        NullRenderAPI() = default;
        ~NullRenderAPI() override = default;

        void Initialize() override {}
        void Shutdown() override {}
        const RenderBackendCapabilities& GetCapabilities() const override { return m_capabilities; }

        void DrawText(const std::string&, int, int, int, const Color&) override {}
        void DrawFPS(int, int) override {}
        void Begin3DMode(const Camera&) override {}
        void End3DMode() override {}
        void DrawGrid(int, float) override {}
        void DrawBox(const Matrix4&, const Float3&, const Color&) override {}
        void DrawBoxWires(const Matrix4&, const Float3&, const Color&) override {}
        void DrawLine3D(const Float3&, const Float3&, const Color&) override {}

    private:
        RenderBackendCapabilities m_capabilities{"Null", 1, true, true, false, true, true};
    };
}