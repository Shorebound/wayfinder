#pragma once

#include <flecs.h>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class WAYFINDER_API SceneModuleRegistry
    {
    public:
        static const SceneModuleRegistry& Get();

        void RegisterModules(flecs::world& world) const;
    };
} // namespace Wayfinder