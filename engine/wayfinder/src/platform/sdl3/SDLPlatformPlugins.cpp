#include "SDLPlatformPlugins.h"

#include "SDLInputSubsystem.h"
#include "SDLTimeSubsystem.h"
#include "SDLWindowSubsystem.h"
#include "app/AppBuilder.h"
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

    void SDLWindowPlugin::Build(AppBuilder& builder)
    {
        builder.RegisterAppSubsystem<SDLWindowSubsystem>({
            .RequiredCapabilities = MakeCapabilitySet(Capability::Presentation),
        });
    }

    void SDLInputPlugin::Build(AppBuilder& builder)
    {
        builder.RegisterAppSubsystem<SDLInputSubsystem>();
    }

    void SDLTimePlugin::Build(AppBuilder& builder)
    {
        builder.RegisterAppSubsystem<SDLTimeSubsystem>();
    }

    void SDLPlatformPlugins::Build(AppBuilder& builder)
    {
        builder.AddPlugin<SDLWindowPlugin>();
        builder.AddPlugin<SDLInputPlugin>();
        builder.AddPlugin<SDLTimePlugin>();
    }

} // namespace Wayfinder
