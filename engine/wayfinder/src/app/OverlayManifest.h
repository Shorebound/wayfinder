#pragma once

#include "OrchestrationTypes.h"

#include <functional>
#include <memory>
#include <typeindex>
#include <vector>

namespace Wayfinder
{
    class IOverlay;

    /**
     * @brief Processed output from overlay registration, stored in AppDescriptor.
     *
     * Consumed by Application to build overlay instances and the OverlayStack.
     * Produced by AppBuilder::Finalise() from accumulated RegisterOverlay calls.
     */
    struct OverlayManifest
    {
        struct OverlayEntry
        {
            std::type_index Type;
            std::function<std::unique_ptr<IOverlay>()> Factory;
            OverlayDescriptor Descriptor;
        };

        std::vector<OverlayEntry> Overlays;
    };

} // namespace Wayfinder
