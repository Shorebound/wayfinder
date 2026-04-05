#pragma once

#include <string>

namespace Wayfinder
{

    /**
     * @brief Configuration struct for Simulation settings.
     *
     * Loaded via ConfigService during gameplay plugin build phase:
     *   builder.LoadConfig<SimulationConfig>("simulation")
     *
     * TOML source: config/simulation.toml
     * Fallback: ProjectDescriptor's boot scene path when no TOML override exists.
     */
    struct SimulationConfig
    {
        /// Path to the boot scene, loaded on Simulation::Initialise.
        /// Loaded from config/simulation.toml, fallback to ProjectDescriptor.
        std::string BootScenePath;

        /// @prototype Future fixed-step support.
        float FixedTickRate = 60.0f;
    };

} // namespace Wayfinder
