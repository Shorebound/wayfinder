#pragma once

#include "Plugin.h"
#include "core/Result.h"
#include "wayfinder_exports.h"

#include <filesystem>
#include <memory>

namespace Wayfinder::Plugins
{

    /// RAII wrapper for a dynamically loaded game plugin library.
    /// Owns both the Plugin instance (via paired create/destroy from the DLL)
    /// and the native library handle. Destruction order: Plugin first, then DLL.
    struct WAYFINDER_API LoadedPlugin
    {
        LoadedPlugin() = default;
        ~LoadedPlugin();

        LoadedPlugin(LoadedPlugin&& other) noexcept;
        LoadedPlugin& operator=(LoadedPlugin&& other) noexcept;

        LoadedPlugin(const LoadedPlugin&) = delete;
        LoadedPlugin& operator=(const LoadedPlugin&) = delete;

        /// The game plugin instance. Null if loading failed or ownership was transferred.
        Plugin* Instance = nullptr;

    private:
        friend class PluginLoader;

        using DestroyFn = void (*)(Plugin*);
        DestroyFn m_destroyFn = nullptr;
        void* m_libraryHandle = nullptr;
    };

    /// Loads game plugin shared libraries at runtime.
    class WAYFINDER_API PluginLoader
    {
    public:
        /**
         * @brief Load a game plugin shared library from the given path.
         * @param libraryPath  Absolute path to the shared library (.dll / .so / .dylib).
         * @return On success a LoadedPlugin owning the plugin instance and
         *         library handle.  On failure an Error describing the cause
         *         (missing library, missing exports, null instance, etc.).
         *         Failures are also logged via the engine logger.
         */
        static Result<LoadedPlugin> Load(const std::filesystem::path& libraryPath);
    };

} // namespace Wayfinder::Plugins
