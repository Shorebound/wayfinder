#pragma once

#include "gameplay/Capability.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <variant>

namespace Wayfinder
{
    class IApplicationState;

    // -- Background mode flags ---------------------------------------------

    /**
     * @brief Flags describing which frame-loop phases run for a background (suspended) state.
     *
     * Used on both sides of push/pop negotiation:
     *  - IApplicationState::GetBackgroundPreferences() -- what the suspended state wants.
     *  - IApplicationState::GetSuspensionPolicy() -- what the foreground state allows.
     *
     * The effective policy is the bitwise AND of both sides.
     */
    enum class BackgroundMode : uint8_t
    {
        None = 0,
        Update = 1 << 0,
        Render = 1 << 1,
        All = Update | Render,
    };

    [[nodiscard]] constexpr auto operator|(BackgroundMode lhs, BackgroundMode rhs) -> BackgroundMode
    {
        return static_cast<BackgroundMode>(std::to_underlying(lhs) | std::to_underlying(rhs));
    }

    [[nodiscard]] constexpr auto operator&(BackgroundMode lhs, BackgroundMode rhs) -> BackgroundMode
    {
        return static_cast<BackgroundMode>(std::to_underlying(lhs) & std::to_underlying(rhs));
    }

    [[nodiscard]] constexpr auto operator~(BackgroundMode mode) -> BackgroundMode
    {
        return static_cast<BackgroundMode>(~std::to_underlying(mode));
    }

    constexpr auto operator|=(BackgroundMode& lhs, BackgroundMode rhs) -> BackgroundMode&
    {
        lhs = lhs | rhs;
        return lhs;
    }

    constexpr auto operator&=(BackgroundMode& lhs, BackgroundMode rhs) -> BackgroundMode&
    {
        lhs = lhs & rhs;
        return lhs;
    }

    /// Check whether a specific flag is set in a BackgroundMode value.
    [[nodiscard]] constexpr auto HasFlag(BackgroundMode value, BackgroundMode flag) -> bool
    {
        return (value & flag) != BackgroundMode::None;
    }

    // -- Pending operation variant -----------------------------------------

    /// Replace the current state with a new state.
    struct FlatTransition
    {
        std::type_index Target;
    };

    /// Push a new state on top of the current state.
    struct PushTransition
    {
        std::type_index Target;
    };

    /// Pop the top state, resuming the one beneath.
    struct PopTransition {};

    /// Pending operation to be processed at the next frame boundary.
    using PendingOperation = std::variant<std::monostate, FlatTransition, PushTransition, PopTransition>;

    // -- Registration descriptors ------------------------------------------

    /**
     * @brief Descriptor for registering an application state with the builder.
     *
     * Stored during AppBuilder::AddState<T>() and consumed by Finalise()
     * to produce the StateManifest.
     */
    struct StateRegistrationDescriptor
    {
        std::function<std::unique_ptr<IApplicationState>()> Factory;
        CapabilitySet Capabilities;
        bool Initial = false;
    };

    /**
     * @brief Descriptor for registering an overlay with the builder.
     *
     * Priority -1 means "use registration order index" (assigned during Finalise).
     */
    struct OverlayDescriptor
    {
        CapabilitySet RequiredCapabilities;
        int32_t Priority = -1;
        bool DefaultActive = true;
    };

} // namespace Wayfinder
