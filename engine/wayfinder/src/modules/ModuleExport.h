#pragma once

#include "Module.h"

namespace Wayfinder
{
    /// Function signature for creating a game module instance.
    /// The caller owns the returned pointer. Must be paired with
    /// WayfinderDestroyModule for proper cleanup across DLL boundaries.
    using WayfinderCreateModuleFn = Module* (*)();

    /// Function signature for destroying a game module instance.
    /// Must be called from the same DLL that created the module.
    using WayfinderDestroyModuleFn = void (*)(Module*);

} // namespace Wayfinder

/// Game modules export two C-linkage functions resolved by ModuleLoader.
/// Use WAYFINDER_IMPLEMENT_MODULE(MyModuleClass) to define both at once.
///
/// @code
///   extern "C" Wayfinder::Module* WayfinderCreateModule();
///   extern "C" void WayfinderDestroyModule(Wayfinder::Module* m);
/// @endcode

#ifdef _WIN32
#   define WAYFINDER_MODULE_API __declspec(dllexport)
#else
#   define WAYFINDER_MODULE_API __attribute__((visibility("default")))
#endif

#define WAYFINDER_IMPLEMENT_MODULE(ModuleClass)                                      \
    extern "C" WAYFINDER_MODULE_API Wayfinder::Module* WayfinderCreateModule()       \
    {                                                                                \
        return new ModuleClass();                                                    \
    }                                                                                \
    extern "C" WAYFINDER_MODULE_API void WayfinderDestroyModule(                     \
        Wayfinder::Module* m)                                                        \
    {                                                                                \
        delete m;                                                                    \
    }
