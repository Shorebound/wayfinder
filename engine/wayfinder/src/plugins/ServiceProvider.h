#pragma once

#include <cassert>
#include <typeindex>
#include <unordered_map>

namespace Wayfinder
{
    /**
     * @brief Concept for types that provide type-safe service access.
     *
     * A ServiceProvider must support Get<T>() returning a reference
     * and TryGet<T>() returning a pointer. These are checked against
     * int as a representative type.
     */
    template<typename T>
    concept ServiceProvider = requires(T provider) {
        { provider.template Get<int>() } -> std::same_as<int&>;
        { provider.template TryGet<int>() } -> std::same_as<int*>;
    };

    /**
     * @brief Standalone service provider for headless tests and tools.
     *
     * Provides simple type-erased service registration and retrieval.
     * Services are stored as void* keyed by std::type_index.
     * The provider does not own the services -- callers must ensure
     * service lifetimes exceed the provider's usage.
     */
    class StandaloneServiceProvider
    {
    public:
        /// Register a service instance. Overwrites any previous registration of the same type.
        template<typename T>
        void Register(T& service)
        {
            m_services[std::type_index(typeid(T))] = &service;
        }

        /// Retrieve a registered service. Asserts if not found.
        template<typename T>
        [[nodiscard]] auto Get() -> T&
        {
            auto it = m_services.find(std::type_index(typeid(T)));
            assert(it != m_services.end() && "Service not registered");
            return *static_cast<T*>(it->second);
        }

        /// Retrieve a registered service, or nullptr if not found.
        template<typename T>
        [[nodiscard]] auto TryGet() -> T*
        {
            auto it = m_services.find(std::type_index(typeid(T)));
            if (it == m_services.end())
            {
                return nullptr;
            }
            return static_cast<T*>(it->second);
        }

    private:
        std::unordered_map<std::type_index, void*> m_services;
    };

    static_assert(ServiceProvider<StandaloneServiceProvider>);

} // namespace Wayfinder
