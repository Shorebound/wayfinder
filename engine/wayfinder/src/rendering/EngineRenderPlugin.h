#pragma once

#include "core/InternedString.h"
#include "plugins/IPlugin.h"
#include "plugins/PluginDescriptor.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AppBuilder;

    /**
     * @brief IPlugin composing the rendering pipeline.
     *
     * Registers RendererSubsystem with the Rendering capability and a dependency
     * on SDLRenderDeviceSubsystem. Intended for use as
     * `app.AddPlugin<EngineRenderPlugin>()` in game entry points.
     *
     * @prototype Render feature registration may move here once individual
     *            features become AppBuilder-registered rather than internally
     *            composed by Renderer.
     */
    class WAYFINDER_API EngineRenderPlugin final : public IPlugin
    {
    public:
        void Build(AppBuilder& builder) override;

        [[nodiscard]] auto Describe() const -> PluginDescriptor override
        {
            return {.Name = InternedString::Intern("EngineRenderPlugin")};
        }
    };

} // namespace Wayfinder
