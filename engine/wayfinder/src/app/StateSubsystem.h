#pragma once

#include "Subsystem.h"
#include "core/Result.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class EngineContext;

    /**
     * @brief Subsystem whose lifetime is tied to the current ApplicationState.
     *
     * Created when an ApplicationState enters and destroyed when it exits.
     * Use SubsystemCollection<StateSubsystem> for the v1 collection, or
     * SubsystemRegistry<StateSubsystem> for dependency-ordered, capability-gated
     * v2 management.
     */
    class WAYFINDER_API StateSubsystem : public Subsystem
    {
    public:
        /// @todo Phase 7 (#171): Remove this using-declaration when SubsystemCollection is deleted.
        using Subsystem::Initialise;

        /// V2 initialisation receiving the engine context. Returns Result<void> for error propagation.
        [[nodiscard]] virtual auto Initialise([[maybe_unused]] EngineContext& context) -> Result<void>
        {
            return {};
        }

        /// @prototype Called when configuration files are reloaded.
        /// Override to respond to config changes. Wired to file-watcher in a future phase.
        virtual void OnConfigReloaded() {}
    };

} // namespace Wayfinder
