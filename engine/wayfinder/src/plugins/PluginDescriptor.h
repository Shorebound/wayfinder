#pragma once

#include "core/InternedString.h"

#include <typeindex>
#include <vector>

namespace Wayfinder
{
    /**
     * @brief Plugin metadata returned by IPlugin::Describe().
     *
     * Name is an InternedString for O(1) duplicate detection.
     * DependsOn declares other plugins (by type_index) that must
     * be built before this one.
     */
    struct PluginDescriptor
    {
        InternedString Name;
        std::vector<std::type_index> DependsOn;
    };

} // namespace Wayfinder
