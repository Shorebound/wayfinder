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
        /// Load a game module shared library from the given path.
        /// Returns an Error on failure (logs the error).
        static Result<LoadedModule> Load(const std::filesystem::path& libraryPath);
    };

} // namespace Wayfinder
