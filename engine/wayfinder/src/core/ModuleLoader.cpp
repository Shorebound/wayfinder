#include "ModuleLoader.h"
#include "ModuleExport.h"
#include "Log.h"
#include "Result.h"

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <Windows.h>
#else
#   include <dlfcn.h>
#endif

namespace Wayfinder
{

    // ---------------------------------------------------------------
    //  LoadedModule
    // ---------------------------------------------------------------

    LoadedModule::~LoadedModule()
    {
        if (Instance && m_destroyFn)
            m_destroyFn(Instance);

        if (m_libraryHandle)
        {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(m_libraryHandle));
#else
            dlclose(m_libraryHandle);
#endif
        }
    }

    LoadedModule::LoadedModule(LoadedModule&& other) noexcept
        : Instance(other.Instance)
        , m_destroyFn(other.m_destroyFn)
        , m_libraryHandle(other.m_libraryHandle)
    {
        other.Instance = nullptr;
        other.m_destroyFn = nullptr;
        other.m_libraryHandle = nullptr;
    }

    LoadedModule& LoadedModule::operator=(LoadedModule&& other) noexcept
    {
        if (this != &other)
        {
            // Destroy current
            if (Instance && m_destroyFn)
                m_destroyFn(Instance);
            if (m_libraryHandle)
            {
#ifdef _WIN32
                FreeLibrary(static_cast<HMODULE>(m_libraryHandle));
#else
                dlclose(m_libraryHandle);
#endif
            }

            Instance = other.Instance;
            m_destroyFn = other.m_destroyFn;
            m_libraryHandle = other.m_libraryHandle;

            other.Instance = nullptr;
            other.m_destroyFn = nullptr;
            other.m_libraryHandle = nullptr;
        }
        return *this;
    }

    // ---------------------------------------------------------------
    //  ModuleLoader
    // ---------------------------------------------------------------

    Result<LoadedModule> ModuleLoader::Load(const std::filesystem::path& libraryPath)
    {
        if (libraryPath.empty())
        {
            WAYFINDER_ERROR(LogEngine, "ModuleLoader: empty library path");
            return MakeError("Empty library path");
        }

#ifdef _WIN32
        HMODULE handle = LoadLibraryW(libraryPath.c_str());
        if (!handle)
        {
            WAYFINDER_ERROR(LogEngine, "ModuleLoader: failed to load '{}' (error {})",
                            libraryPath.string(), GetLastError());
            return MakeError("Failed to load shared library");
        }

        auto createFn = reinterpret_cast<WayfinderCreateModuleFn>(
            GetProcAddress(handle, "WayfinderCreateModule"));
        auto destroyFn = reinterpret_cast<WayfinderDestroyModuleFn>(
            GetProcAddress(handle, "WayfinderDestroyModule"));
#else
        void* handle = dlopen(libraryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle)
        {
            WAYFINDER_ERROR(LogEngine, "ModuleLoader: failed to load '{}': {}",
                            libraryPath.string(), dlerror());
            return MakeError("Failed to load shared library");
        }

        auto createFn = reinterpret_cast<WayfinderCreateModuleFn>(
            dlsym(handle, "WayfinderCreateModule"));
        auto destroyFn = reinterpret_cast<WayfinderDestroyModuleFn>(
            dlsym(handle, "WayfinderDestroyModule"));
#endif

        if (!createFn || !destroyFn)
        {
            WAYFINDER_ERROR(LogEngine,
                "ModuleLoader: '{}' missing required exports (WayfinderCreateModule={}, WayfinderDestroyModule={})",
                libraryPath.string(),
                static_cast<bool>(createFn),
                static_cast<bool>(destroyFn));
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return MakeError("Missing required module exports");
        }

        LoadedModule result;
        result.Instance = createFn();
        result.m_destroyFn = destroyFn;
        result.m_libraryHandle = static_cast<void*>(handle);

        if (!result.Instance)
        {
            WAYFINDER_ERROR(LogEngine, "ModuleLoader: WayfinderCreateModule() returned null for '{}'",
                            libraryPath.string());
            return MakeError("WayfinderCreateModule returned null");
        }

        WAYFINDER_INFO(LogEngine, "ModuleLoader: loaded game module from '{}'",
                        libraryPath.string());
        return result;
    }

} // namespace Wayfinder
