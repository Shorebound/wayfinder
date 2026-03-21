#pragma once

#include "core/GameState.h"
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
    /// Internal storage and topological-sort logic for ECS system descriptors.
    /// Owned by ModuleRegistry — not a subsystem.
    class WAYFINDER_API SystemRegistrar
    {
    public:
        struct Descriptor
        {
            std::string Name;
            std::function<void(flecs::world&)> Factory;
            RunCondition Condition;
            std::vector<std::string> After;  ///< Names of systems this must run after.
            std::vector<std::string> Before; ///< Names of systems this must run before.
        };

        /// Register a named system descriptor.  Rejects duplicates.
        void Register(std::string name,
                      std::function<void(flecs::world&)> factory,
                      RunCondition condition = {},
                      std::vector<std::string> after = {},
                      std::vector<std::string> before = {});

        /// Topologically sort and apply all registered factories into the world.
        void ApplyToWorld(flecs::world& world) const;

        /// Read-only access to descriptors.
        const std::vector<Descriptor>& GetDescriptors() const { return m_descriptors; }

    private:
        std::vector<Descriptor> m_descriptors;
    };

} // namespace Wayfinder
