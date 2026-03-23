#include "app/EntryPoint.h"
#include "plugins/Plugin.h"
#include "plugins/PluginRegistry.h"

class WaystoneGame : public Wayfinder::Plugin
{
    void Build(Wayfinder::PluginRegistry& /*registry*/) override {}
};

std::unique_ptr<Wayfinder::Plugin> Wayfinder::CreateGamePlugin()
{
    return std::make_unique<WaystoneGame>();
}
