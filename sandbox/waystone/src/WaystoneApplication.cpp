#include "app/EntryPoint.h"
#include "plugins/Plugin.h"
#include "plugins/PluginRegistry.h"
#include "scene/plugins/CameraPlugin.h"
#include "scene/plugins/TransformPlugin.h"

class WaystoneGame : public Wayfinder::Plugins::Plugin
{
    void Build(Wayfinder::Plugins::PluginRegistry& registry) override
    {
        registry.AddPlugin<Wayfinder::TransformPlugin>();
        registry.AddPlugin<Wayfinder::CameraPlugin>();
    }
};

namespace Wayfinder::Plugins
{
    std::unique_ptr<Plugin> CreateGamePlugin()
    {
        return std::make_unique<WaystoneGame>();
    }
}
