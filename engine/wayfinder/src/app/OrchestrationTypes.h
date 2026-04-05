#pragma once

#include "gameplay/Capability.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>
#include <variant>

namespace Wayfinder
{
    class IApplicationState;

    // -- Negotiation structs -----------------------------------------------

    /**
     * @brief What a background (suspended) state wants when pushed behind another state.
     *
     * Conservative defaults: a background state neither updates nor renders
     * unless it explicitly opts in. Used during push/pop negotiation.
     */
    struct BackgroundPreferences
    {
        bool WantsBackgroundUpdate = false;
        bool WantsBackgroundRender = false;
    };

    /**
     * @brief What the foreground (pushing) state allows background states to do.
     *
     * Default: allows background render (common pause-over-gameplay case)
     * but not background update. Used during push/pop negotiation.
     */
    struct SuspensionPolicy
    {
        bool AllowBackgroundUpdate = false;
        bool AllowBackgroundRender = true;
    };

    /**
     * @brief Result of AND-intersecting BackgroundPreferences with SuspensionPolicy.
     */
    struct EffectiveBackgroundPolicy
    {
        bool Update;
        bool Render;
    };

    /**
     * @brief Compute effective background behaviour from both sides of a push.
     *
     * Both the background state (what it wants) and the foreground state
     * (what it allows) must agree for each dimension to be active.
     */
    [[nodiscard]] constexpr auto ComputeBackgroundPolicy(const BackgroundPreferences& background, const SuspensionPolicy& foreground) -> EffectiveBackgroundPolicy
    {
        return {
            .Update = background.WantsBackgroundUpdate and foreground.AllowBackgroundUpdate,
            .Render = background.WantsBackgroundRender and foreground.AllowBackgroundRender,
        };
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
