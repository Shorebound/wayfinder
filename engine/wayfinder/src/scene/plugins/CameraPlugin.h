#pragma once

#include "plugins/Plugin.h"

namespace Wayfinder
{
    /**
     * @brief Registers the active primary camera extraction system (OnUpdate).
     */
    class WAYFINDER_API CameraPlugin : public Plugins::Plugin
    {
    public:
        void Build(Plugins::PluginRegistry& registry) override;
    };

} // namespace Wayfinder
