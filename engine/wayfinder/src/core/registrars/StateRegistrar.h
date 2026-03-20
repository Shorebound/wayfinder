#pragma once

#include "wayfinder_exports.h"

#include <functional>
#include <string>
#include <vector>

namespace flecs
{
    struct world;
}

namespace Wayfinder
{
    /// Internal storage for game-state descriptors.
    /// Owned by ModuleRegistry — not a subsystem.
    class WAYFINDER_API StateRegistrar
    {
    public:
        struct Descriptor
        {
            std::string Name;
            std::function<void(flecs::world&)> OnEnter;
            std::function<void(flecs::world&)> OnExit;
        };

        /// Register a named state descriptor.
        void Register(Descriptor descriptor);

        /// Set the initial game state name.
        void SetInitial(std::string stateName);

        /// Read-only access to descriptors.
        const std::vector<Descriptor>& GetDescriptors() const { return m_descriptors; }

        /// Returns the initial state name (empty if none was set).
        const std::string& GetInitial() const { return m_initialState; }

    private:
        std::vector<Descriptor> m_descriptors;
        std::string m_initialState;
    };

} // namespace Wayfinder
