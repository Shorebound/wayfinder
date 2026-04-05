#pragma once

#include "core/Assert.h"

#include <memory>
#include <typeindex>
#include <unordered_map>

namespace Wayfinder
{
    class AppBuilder;

    /**
     * @brief Immutable container of type-erased outputs produced by AppBuilder::Finalise().
     *
     * Each registrar's Finalise() produces a typed output. AppBuilder stores
     * them here for later retrieval by the application or engine runtime.
     * Queryable by output type via Get<T>(), TryGet<T>(), and Has<T>().
     */
    class AppDescriptor
    {
    public:
        AppDescriptor() = default;

        AppDescriptor(const AppDescriptor&) = delete;
        auto operator=(const AppDescriptor&) -> AppDescriptor& = delete;
        AppDescriptor(AppDescriptor&&) noexcept = default;
        auto operator=(AppDescriptor&&) noexcept -> AppDescriptor& = default;

        /// Retrieve a processed output by type. Asserts if not found.
        template<typename TOutput>
        [[nodiscard]] auto Get() const -> const TOutput&
        {
            auto it = m_outputs.find(std::type_index(typeid(TOutput)));
            WAYFINDER_ASSERT(it != m_outputs.end(), "AppDescriptor::Get<{}> - output not found", typeid(TOutput).name());
            return *static_cast<const TOutput*>(it->second.get());
        }

        /// Retrieve a processed output by type, or nullptr if not found.
        template<typename TOutput>
        [[nodiscard]] auto TryGet() const -> const TOutput*
        {
            auto it = m_outputs.find(std::type_index(typeid(TOutput)));
            if (it == m_outputs.end())
            {
                return nullptr;
            }
            return static_cast<const TOutput*>(it->second.get());
        }

        /// Check if an output type is present.
        template<typename TOutput>
        [[nodiscard]] auto Has() const -> bool
        {
            return m_outputs.contains(std::type_index(typeid(TOutput)));
        }

        /// Add a processed output. Called by AppBuilder::Finalise().
        /// @todo Restrict to private + friend once AppBuilder exists (Plan 03-03).
        template<typename TOutput>
        void AddOutput(TOutput output)
        {
            auto* ptr = new TOutput(std::move(output));
            m_outputs[std::type_index(typeid(TOutput))] = std::unique_ptr<void, OutputDeleter>(ptr, OutputDeleter{[](void* p)
            {
                delete static_cast<TOutput*>(p);
            }});
        }

    private:
        struct OutputDeleter
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

        std::unordered_map<std::type_index, std::unique_ptr<void, OutputDeleter>> m_outputs;
    };

} // namespace Wayfinder
