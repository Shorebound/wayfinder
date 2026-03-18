#pragma once

#include "../RenderDevice.h"

namespace Wayfinder
{
    class NullDevice final : public RenderDevice
    {
    public:
        NullDevice() = default;
        explicit NullDevice(const std::string& backendName) { m_info.BackendName = backendName; }
        ~NullDevice() override = default;

        bool Initialize(Window&) override { return true; }
        void Shutdown() override {}

        bool BeginFrame() override { return true; }
        void EndFrame() override {}

        void BeginRenderPass(const RenderPassDescriptor&) override {}
        void EndRenderPass() override {}

        const RenderDeviceInfo& GetDeviceInfo() const override { return m_info; }

    private:
        RenderDeviceInfo m_info{.BackendName = "Null"};
    };

} // namespace Wayfinder
