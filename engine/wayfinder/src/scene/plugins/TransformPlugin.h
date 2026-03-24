#pragma once

#include "plugins/Plugin.h"

namespace Wayfinder
{
    /**
     * @brief Registers the world-space transform propagation system (PreUpdate).
     */
    class WAYFINDER_API TransformPlugin : public Plugins::Plugin
    {
    public:
        void Build(Plugins::PluginRegistry& registry) override;
    };

} // namespace Wayfinder
