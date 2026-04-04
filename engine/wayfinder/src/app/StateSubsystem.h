#pragma once

#include "Subsystem.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    /**
     * @brief Subsystem whose lifetime is tied to the current ApplicationState.
     *
     * Created when an ApplicationState enters and destroyed when it exits.
     * Use SubsystemCollection<StateSubsystem> for the state-scoped collection.
     */
    class WAYFINDER_API StateSubsystem : public Subsystem {};

} // namespace Wayfinder
