#include "app/EntryPoint.h"
#include "plugins/Plugin.h"
#include "plugins/PluginRegistry.h"
#include "scene/plugins/CameraPlugin.h"
#include "scene/plugins/TransformPlugin.h"

class WaystoneGame : public Wayfinder::Plugin
{
    void Build(Wayfinder::PluginRegistry& registry) override
    {
        registry.AddPlugin<Wayfinder::TransformPlugin>();
        registry.AddPlugin<Wayfinder::CameraPlugin>();
    }
};

std::unique_ptr<Wayfinder::Plugin> Wayfinder::CreateGamePlugin()
{
    return std::make_unique<WaystoneGame>();
}
