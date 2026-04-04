#pragma once

#include "Subsystem.h"
#include "core/Result.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class EngineContext;

    /**
     * @brief Subsystem whose lifetime is tied to the Application.
     *
     * Created during Application startup and destroyed during
     * Application shutdown. Use SubsystemCollection<AppSubsystem>
     * for the v1 collection, or SubsystemRegistry<AppSubsystem>
     * for dependency-ordered, capability-gated v2 management.
     */
    class WAYFINDER_API AppSubsystem : public Subsystem
    {
    public:
        /// V2 initialisation receiving the engine context. Returns Result<void> for error propagation.
        [[nodiscard]] virtual auto Initialise(EngineContext& context) -> Result<void>
        {
            return {};
        }
    };

} // namespace Wayfinder
