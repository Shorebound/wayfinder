#pragma once

#include "plugins/Plugin.h"

namespace Wayfinder
{
    /**
     * @brief Registers the active primary camera extraction system (OnUpdate).
     */
    class WAYFINDER_API CameraPlugin : public Plugin
    {
    public:
        void Build(PluginRegistry& registry) override;
    };

} // namespace Wayfinder
