#pragma once

#include "core/InternedString.h"
#include "plugins/IPlugin.h"
#include "plugins/PluginDescriptor.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AppBuilder;

    /**
     * @brief IPlugin registering SDLRenderDeviceSubsystem.
     *
     * Requires the Rendering capability. Depends on SDLWindowSubsystem
     * (the GPU device needs a window surface for swapchain initialisation).
     */
    class WAYFINDER_API SDLRenderDevicePlugin final : public IPlugin
    {
    public:
        void Build(AppBuilder& builder) override;

        [[nodiscard]] auto Describe() const -> PluginDescriptor override
        {
            return {.Name = InternedString::Intern("SDLRenderDevicePlugin")};
        }
    };

} // namespace Wayfinder
