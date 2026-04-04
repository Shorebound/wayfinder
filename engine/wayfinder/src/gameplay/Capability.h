#pragma once

#include "NativeTag.h"
#include "Tag.h"

namespace Wayfinder
{
    /// CapabilitySet is a TagContainer used for capability-based activation control.
    using CapabilitySet = TagContainer;

    /**
     * @brief Well-known capability tags for subsystem and overlay activation.
     *
     * These constants self-register into the NativeTag linked list and become
     * valid Tag values after NativeTag::RegisterAll(registry) is called during
     * engine startup.
     */
    namespace Capability
    {
        inline NativeTag Simulation{"Capability.Simulation", "Enables simulation subsystems"};
        inline NativeTag Rendering{"Capability.Rendering", "Enables rendering subsystems"};
        inline NativeTag Presentation{"Capability.Presentation", "Enables presentation subsystems"};
        inline NativeTag Editing{"Capability.Editing", "Enables editing subsystems"};
    } // namespace Capability

} // namespace Wayfinder
