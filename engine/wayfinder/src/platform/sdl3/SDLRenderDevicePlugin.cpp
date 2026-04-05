#include "SDLRenderDevicePlugin.h"

#include "SDLRenderDeviceSubsystem.h"
#include "SDLWindowSubsystem.h"
#include "app/AppBuilder.h"
#include "app/SubsystemManifest.h"
#include "gameplay/Capability.h"

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

    void SDLRenderDevicePlugin::Build(AppBuilder& builder)
    {
        builder.RegisterAppSubsystem<SDLRenderDeviceSubsystem>({
            .RequiredCapabilities = MakeCapabilitySet(Capability::Rendering),
            .DependsOn = Deps<SDLWindowSubsystem>(),
        });
    }

} // namespace Wayfinder
