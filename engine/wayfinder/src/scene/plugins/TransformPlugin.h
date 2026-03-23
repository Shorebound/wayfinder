#pragma once

#include "plugins/Plugin.h"

namespace Wayfinder
{
    /**
     * @brief Registers the world-space transform propagation system (PreUpdate).
     */
    class WAYFINDER_API TransformPlugin : public Plugin
    {
    public:
        void Build(PluginRegistry& registry) override;
    };

} // namespace Wayfinder
