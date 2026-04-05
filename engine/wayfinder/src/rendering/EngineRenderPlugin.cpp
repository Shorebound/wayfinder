#include "EngineRenderPlugin.h"

#include "app/AppBuilder.h"
#include "app/RendererSubsystem.h"
#include "app/SubsystemManifest.h"
#include "gameplay/Capability.h"
#include "gameplay/Tag.h"
#include "platform/sdl3/SDLRenderDeviceSubsystem.h"

namespace Wayfinder
{
    namespace
    {
        auto MakeCapabilitySet(Tag tag) -> CapabilitySet
        {
            CapabilitySet caps;
            caps.AddTag(tag);
            return caps;
        }
    } // namespace

    void EngineRenderPlugin::Build(AppBuilder& builder)
    {
        builder.RegisterAppSubsystem<RendererSubsystem>({
            .RequiredCapabilities = MakeCapabilitySet(Capability::Rendering),
            .DependsOn = Deps<SDLRenderDeviceSubsystem>(),
        });
    }

} // namespace Wayfinder
