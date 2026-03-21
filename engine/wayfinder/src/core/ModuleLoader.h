#pragma once

#include "Module.h"
#include "Result.h"
#include "wayfinder_exports.h"

#include <filesystem>
#include <memory>

namespace Wayfinder
{

    /// RAII wrapper for a dynamically loaded game module.
    /// Owns both the Module instance (via paired create/destroy from the DLL)
    /// and the native library handle. Destruction order: Module first, then DLL.
    struct WAYFINDER_API LoadedModule
    {
        LoadedModule() = default;
        ~LoadedModule();

        LoadedModule(LoadedModule&& other) noexcept;
        LoadedModule& operator=(LoadedModule&& other) noexcept;

        LoadedModule(const LoadedModule&) = delete;
        LoadedModule& operator=(const LoadedModule&) = delete;

        /// The game module instance. Null if loading failed.
        Module* Instance = nullptr;

    private:
        friend class ModuleLoader;

        using DestroyFn = void (*)(Module*);
        DestroyFn m_destroyFn = nullptr;
        void* m_libraryHandle = nullptr;
    };

    /// Loads game module shared libraries at runtime.
    class WAYFINDER_API ModuleLoader
    {
    public:
        /**
         * @brief Load a game module shared library from the given path.
         * @param libraryPath  Absolute path to the shared library (.dll / .so / .dylib).
         * @return On success a LoadedModule owning the module instance and
         *         library handle.  On failure an Error describing the cause
         *         (missing library, missing exports, null instance, etc.).
         *         Failures are also logged via the engine logger.
         */
        static Result<LoadedModule> Load(const std::filesystem::path& libraryPath);
    };

} // namespace Wayfinder
