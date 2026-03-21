#pragma once

#include "core/Assert.h"
#include "rendering/FrameAllocator.h"

#include <type_traits>
#include <utility>

namespace Wayfinder
{
    /**
     * @brief A type-erased callable whose captured state lives in a FrameAllocator.
     *
     * Zero heap allocations — the callable's captured state is placement-new'd into
     * the arena. The arena's destructor registry handles cleanup on Reset().
     *
     * Non-copyable, move-only. The moved-from instance becomes empty.
     * Destroying an ArenaFunction is a no-op; the arena owns the storage.
     *
     * @tparam TSignature  Function signature, e.g. void(RenderDevice&, const RenderGraphResources&).
     */
    template <typename TSignature>
    class ArenaFunction;

    template <typename TReturn, typename... TArgs>
    class ArenaFunction<TReturn(TArgs...)>
    {
    public:
        ArenaFunction() = default;

        /**
         * @brief Construct from a callable, placing its storage in the given allocator.
         *
         * The callable is moved/copied into arena memory. If it has a non-trivial
         * destructor, that destructor is registered with the allocator and called
         * during FrameAllocator::Reset().
         */
        template <typename TCallable>
            requires(!std::is_same_v<std::decay_t<TCallable>, ArenaFunction>)
        ArenaFunction(FrameAllocator& allocator, TCallable&& callable)
        {
            using Decayed = std::decay_t<TCallable>;

            auto* storage = allocator.Create<Decayed>(std::forward<TCallable>(callable));
            m_data = storage;
            m_invoke = [](void* data, TArgs... args) -> TReturn
            {
                return (*static_cast<Decayed*>(data))(std::forward<TArgs>(args)...);
            };
        }

        // Non-copyable
        ArenaFunction(const ArenaFunction&) = delete;
        ArenaFunction& operator=(const ArenaFunction&) = delete;

        // Movable
        ArenaFunction(ArenaFunction&& other) noexcept
            : m_invoke(other.m_invoke), m_data(other.m_data)
        {
            other.m_invoke = nullptr;
            other.m_data = nullptr;
        }

        ArenaFunction& operator=(ArenaFunction&& other) noexcept
        {
            if (this != &other)
            {
                m_invoke = other.m_invoke;
                m_data = other.m_data;
                other.m_invoke = nullptr;
                other.m_data = nullptr;
            }
            return *this;
        }

        /// Intentionally empty — the arena owns the storage and handles destruction.
        ~ArenaFunction() = default;

        TReturn operator()(TArgs... args) const
        {
            WAYFINDER_ASSERT(m_invoke != nullptr, "ArenaFunction::operator(): called on empty function");
            return m_invoke(m_data, std::forward<TArgs>(args)...);
        }

        explicit operator bool() const { return m_invoke != nullptr; }

    private:
        using InvokeFn = TReturn (*)(void*, TArgs...);

        InvokeFn m_invoke = nullptr;
        void* m_data = nullptr;
    };

} // namespace Wayfinder
