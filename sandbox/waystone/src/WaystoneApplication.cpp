#include "app/EntryPoint.h"
#include "plugins/Plugin.h"
#include "plugins/PluginRegistry.h"
#include "scene/SceneWorldBootstrap.h"

class WaystoneGame : public Wayfinder::Plugins::Plugin
{
    void Build(Wayfinder::Plugins::PluginRegistry& registry) override
    {
        Wayfinder::PopulateDefaultScenePlugins(registry);
    }
};

namespace Wayfinder::Plugins
{
    std::unique_ptr<Plugin> CreateGamePlugin()
    {
        return std::make_unique<WaystoneGame>();
    }
}
