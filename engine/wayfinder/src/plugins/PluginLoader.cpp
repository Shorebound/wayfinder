#include "PluginLoader.h"
#include "PluginExport.h"
#include "core/Log.h"
#include "core/Result.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace Wayfinder::Plugins
{

    // ---------------------------------------------------------------
    //  LoadedPlugin
    // ---------------------------------------------------------------

    LoadedPlugin::~LoadedPlugin()
    {
        if (Instance && m_destroyFn)
        {
            m_destroyFn(Instance);
        }

        if (m_libraryHandle)
        {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(m_libraryHandle));
#else
            dlclose(m_libraryHandle);
#endif
        }
    }

    LoadedPlugin::LoadedPlugin(LoadedPlugin&& other) noexcept : Instance(other.Instance), m_destroyFn(other.m_destroyFn), m_libraryHandle(other.m_libraryHandle)
    {
        other.Instance = nullptr;
        other.m_destroyFn = nullptr;
        other.m_libraryHandle = nullptr;
    }

    LoadedPlugin& LoadedPlugin::operator=(LoadedPlugin&& other) noexcept
    {
        if (this != &other)
        {
            // Destroy current
            if (Instance && m_destroyFn)
            {
                m_destroyFn(Instance);
            }
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
    //  PluginLoader
    // ---------------------------------------------------------------

    Result<LoadedPlugin> PluginLoader::Load(const std::filesystem::path& libraryPath)
    {
        if (libraryPath.empty())
        {
            Log::Error(LogEngine, "PluginLoader: empty library path");
            return MakeError("Empty library path");
        }

#ifdef _WIN32
        HMODULE handle = LoadLibraryW(libraryPath.c_str());
        if (!handle)
        {
            Log::Error(LogEngine, "PluginLoader: failed to load '{}' (error {})", libraryPath.string(), GetLastError());
            return MakeError("Failed to load shared library");
        }

        auto versionFn = reinterpret_cast<WayfinderGetPluginAPIVersionFn>(GetProcAddress(handle, "WayfinderGetPluginAPIVersion"));
#else
        void* handle = dlopen(libraryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle)
        {
            Log::Error(LogEngine, "PluginLoader: failed to load '{}': {}", libraryPath.string(), dlerror());
            return MakeError("Failed to load shared library");
        }

        auto versionFn = reinterpret_cast<WayfinderGetPluginAPIVersionFn>(dlsym(handle, "WayfinderGetPluginAPIVersion"));
#endif

        if (!versionFn)
        {
            Log::Error(LogEngine, "PluginLoader: '{}' missing WayfinderGetPluginAPIVersion export", libraryPath.string());
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return MakeError("Plugin ABI mismatch");
        }

        const uint32_t pluginAbi = versionFn();
        if (pluginAbi != WAYFINDER_PLUGIN_ABI_VERSION)
        {
            Log::Error(LogEngine, "PluginLoader: '{}' ABI mismatch (plugin {}, engine {})", libraryPath.string(), pluginAbi, WAYFINDER_PLUGIN_ABI_VERSION);
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return MakeError("Plugin ABI mismatch");
        }

#ifdef _WIN32
        auto createFn = reinterpret_cast<WayfinderCreateGamePluginFn>(GetProcAddress(handle, "WayfinderCreateGamePlugin"));
        auto destroyFn = reinterpret_cast<WayfinderDestroyGamePluginFn>(GetProcAddress(handle, "WayfinderDestroyGamePlugin"));
#else
        auto createFn = reinterpret_cast<WayfinderCreateGamePluginFn>(dlsym(handle, "WayfinderCreateGamePlugin"));
        auto destroyFn = reinterpret_cast<WayfinderDestroyGamePluginFn>(dlsym(handle, "WayfinderDestroyGamePlugin"));
#endif

        if (!createFn || !destroyFn)
        {
            Log::Error(
                LogEngine, "PluginLoader: '{}' missing required exports (WayfinderCreateGamePlugin={}, WayfinderDestroyGamePlugin={})", libraryPath.string(), static_cast<bool>(createFn), static_cast<bool>(destroyFn));
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return MakeError("Missing required plugin exports");
        }

        LoadedPlugin result;
        result.Instance = createFn();
        result.m_destroyFn = destroyFn;
        result.m_libraryHandle = handle;

        if (!result.Instance)
        {
            Log::Error(LogEngine, "PluginLoader: WayfinderCreateGamePlugin() returned null for '{}'", libraryPath.string());
            return MakeError("WayfinderCreateGamePlugin returned null");
        }

        Log::Info(LogEngine, "PluginLoader: loaded game plugin from '{}'", libraryPath.string());
        return result;
    }

} // namespace Wayfinder::Plugins
