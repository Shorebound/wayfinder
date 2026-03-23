#pragma once

#include "Plugin.h"

namespace Wayfinder
{
    /// Function signature for creating the game's root plugin from a shared library.
    /// The caller owns the returned pointer. Must be paired with
    /// WayfinderDestroyGamePlugin for proper cleanup across DLL boundaries.
    using WayfinderCreateGamePluginFn = Plugin* (*)();

    /// Function signature for destroying the game's root plugin.
    /// Must be called from the same DLL that created the plugin.
    using WayfinderDestroyGamePluginFn = void (*)(Plugin*);

} // namespace Wayfinder

/// Game plugins export two C-linkage functions resolved by PluginLoader.
/// Use WAYFINDER_IMPLEMENT_GAME_PLUGIN(MyPluginClass) to define both at once.
///
/// @code
///   extern "C" Wayfinder::Plugin* WayfinderCreateGamePlugin();
///   extern "C" void WayfinderDestroyGamePlugin(Wayfinder::Plugin* p);
/// @endcode

#ifdef _WIN32
#define WAYFINDER_PLUGIN_EXPORT __declspec(dllexport)
#else
#define WAYFINDER_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define WAYFINDER_IMPLEMENT_GAME_PLUGIN(PluginClass)                                                                                                                                                                       \
    extern "C" WAYFINDER_PLUGIN_EXPORT Wayfinder::Plugin* WayfinderCreateGamePlugin()                                                                                                                                      \
    {                                                                                                                                                                                                                      \
        return new PluginClass();                                                                                                                                                                                          \
    }                                                                                                                                                                                                                      \
    extern "C" WAYFINDER_PLUGIN_EXPORT void WayfinderDestroyGamePlugin(Wayfinder::Plugin* p)                                                                                                                               \
    {                                                                                                                                                                                                                      \
        delete p;                                                                                                                                                                                                          \
    }
