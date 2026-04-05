#pragma once

#include "OrchestrationTypes.h"
#include "gameplay/Capability.h"

#include <span>
#include <typeindex>
#include <vector>

namespace Wayfinder
{
    class EngineContext;
    class EventQueue;
    class IOverlay;

    /**
     * @brief Entry in the overlay stack, pairing a non-owning overlay pointer with
     *        its capability requirements, priority, and activation state.
     */
    struct OverlayEntry
    {
        IOverlay* Overlay = nullptr;
        std::type_index Type = std::type_index(typeid(void));
        CapabilitySet RequiredCapabilities;
        int32_t Priority = 0;
        bool CapabilitySatisfied = false;
        bool ManuallyActive = true;

        [[nodiscard]] auto IsActive() const -> bool
        {
            return CapabilitySatisfied and ManuallyActive;
        }
    };

    /**
     * @brief Non-owning, priority-sorted execution view over Application-owned overlays.
     *
     * Overlays persist across state transitions. Their activation is gated by the
     * current state's CapabilitySet: an overlay is capability-satisfied when its
     * RequiredCapabilities are a subset of the effective capabilities (or empty).
     *
     * Execution order: Update and Render iterate low-to-high priority (bottom-up).
     * Event processing iterates high-to-low (top-down, first overlay to consume wins).
     */
    class OverlayStack
    {
    public:
        /// Add a non-owning overlay entry. Sorts by effective priority after insertion.
        void AddOverlay(IOverlay* overlay, std::type_index type, OverlayDescriptor descriptor, int32_t registrationIndex);

        /// Recompute capability satisfaction for all entries. Fires OnAttach/OnDetach on transitions.
        void UpdateCapabilities(const CapabilitySet& effectiveCaps, EngineContext& context);

        /// Process events top-down (highest priority first). Returns true if any overlay consumed.
        [[nodiscard]] auto ProcessEvents(EngineContext& context, EventQueue& events) -> bool;

        /// Tick active overlays in priority order (low to high).
        void Update(EngineContext& context, float deltaTime);

        /// Render active overlays in priority order (low to high, bottom-up).
        void Render(EngineContext& context);

        /// Runtime activation toggle. Calls OnAttach if capabilities already satisfied.
        void Activate(std::type_index overlayType, EngineContext& context);

        /// Runtime deactivation toggle. Calls OnDetach if currently active.
        void Deactivate(std::type_index overlayType, EngineContext& context);

        /// Detach all active overlays (shutdown path). Iterates reverse priority order.
        void DetachAll(EngineContext& context);

        /// Read-only view of entries for inspection.
        [[nodiscard]] auto GetEntries() const -> std::span<const OverlayEntry>;

        /// Number of registered overlays.
        [[nodiscard]] auto GetEntryCount() const -> size_t;

    private:
        std::vector<OverlayEntry> m_entries;
    };

} // namespace Wayfinder
