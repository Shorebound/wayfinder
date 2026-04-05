#include "app/Application.h"
#include "plugins/Plugin.h"
#include "plugins/PluginRegistry.h"
#include "scene/SceneWorldBootstrap.h"

namespace
{
    class WaystoneGame : public Wayfinder::Plugins::Plugin
    {
    public:
        void Build(Wayfinder::Plugins::PluginRegistry& registry) override
        {
            Wayfinder::PopulateDefaultScenePlugins(registry);
        }
    };
} // namespace

int main(int argc, char* argv[])
{
    auto gamePlugin = std::make_unique<WaystoneGame>();
    Wayfinder::Application app(std::move(gamePlugin), {argc, argv});
    app.Run();
    return 0;
}
