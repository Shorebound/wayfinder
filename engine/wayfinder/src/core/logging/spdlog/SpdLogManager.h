#pragma once
#include <spdlog/spdlog.h>

namespace Wayfinder
{

    /** @brief Thin wrapper for spdlog global lifecycle. */
    class SpdLogManager
    {
    public:
        static void Initialise()
        {
            spdlog::set_pattern("%^[%T] %n: %v%$");
        }

        static void Shutdown()
        {
            spdlog::shutdown();
        }
    };

} // namespace Wayfinder
