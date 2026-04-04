#pragma once

#include "Subsystem.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    /**
     * @brief Subsystem whose lifetime is tied to the Application.
     *
     * Created during Application startup and destroyed during
     * Application shutdown. Use SubsystemCollection<AppSubsystem>
     * for the application-scoped collection.
     */
    class WAYFINDER_API AppSubsystem : public Subsystem {};

} // namespace Wayfinder
