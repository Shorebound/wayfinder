#pragma once

#include "app/AppSubsystem.h"
#include "core/Assert.h"

#include <memory>
#include <typeindex>
#include <unordered_map>

namespace Wayfinder
{

    /**
     * @brief AppSubsystem providing address-stable, typed config storage at runtime.
     *
     * Config structs are stored during the AppBuilder build phase and
     * retrievable at runtime via Get<T>() and TryGet<T>().
     */
    class ConfigService : public AppSubsystem
    {
    public:
        /// Retrieve a stored config by type. Asserts if not present.
        template<typename T>
        [[nodiscard]] auto Get() const -> const T&
        {
            auto it = m_configs.find(std::type_index(typeid(T)));
            WAYFINDER_ASSERT(it != m_configs.end(), "ConfigService::Get<{}> - config not stored", typeid(T).name());
            return *static_cast<const T*>(it->second.get());
        }

        /// Retrieve a stored config by type, or nullptr if not present.
        template<typename T>
        [[nodiscard]] auto TryGet() const -> const T*
        {
            auto it = m_configs.find(std::type_index(typeid(T)));
            if (it == m_configs.end())
            {
                return nullptr;
            }
            return static_cast<const T*>(it->second.get());
        }

        /// Check if a config type is stored.
        template<typename T>
        [[nodiscard]] auto Has() const -> bool
        {
            return m_configs.contains(std::type_index(typeid(T)));
        }

        /// Store a config value. Called during AppBuilder setup (before Initialise).
        template<typename T>
        void Store(T config)
        {
            auto* raw = new T(std::move(config));
            auto deleter = ConfigDeleter{[](void* p)
            {
                delete static_cast<T*>(p);
            }};
            m_configs[std::type_index(typeid(T))] = std::unique_ptr<void, ConfigDeleter>(raw, deleter);
        }

        [[nodiscard]] auto Initialise(EngineContext& context) -> Result<void> override;
        void Shutdown() override;

    private:
        struct ConfigDeleter
        {
            void (*Destroy)(void*) = nullptr;
            void operator()(void* ptr) const
            {
                if (Destroy and ptr)
                {
                    Destroy(ptr);
                }
            }
        };

        std::unordered_map<std::type_index, std::unique_ptr<void, ConfigDeleter>> m_configs;
    };

} // namespace Wayfinder
