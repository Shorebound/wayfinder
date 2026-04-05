#include "OverlayStack.h"

#include "IOverlay.h"
#include "core/Assert.h"
#include "core/Log.h"
#include "core/events/EventQueue.h"

#include <algorithm>
#include <ranges>

namespace Wayfinder
{

    void OverlayStack::AddOverlay(IOverlay* overlay, std::type_index type, OverlayDescriptor descriptor, int32_t registrationIndex)
    {
        WAYFINDER_ASSERT(overlay != nullptr, "AddOverlay called with null overlay");

        OverlayEntry entry;
        entry.Overlay = overlay;
        entry.Type = type;
        entry.RequiredCapabilities = std::move(descriptor.RequiredCapabilities);
        entry.Priority = (descriptor.Priority == -1) ? registrationIndex : descriptor.Priority;
        entry.ManuallyActive = descriptor.DefaultActive;
        entry.CapabilitySatisfied = false;

        m_entries.push_back(std::move(entry));

        // Stable sort preserves registration order for equal priorities.
        std::ranges::stable_sort(m_entries, {}, &OverlayEntry::Priority);
    }

    void OverlayStack::UpdateCapabilities(const CapabilitySet& effectiveCaps, EngineContext& context)
    {
        for (auto& entry : m_entries)
        {
            const bool wasSatisfied = entry.CapabilitySatisfied;
            entry.CapabilitySatisfied = entry.RequiredCapabilities.IsEmpty() or effectiveCaps.HasAll(entry.RequiredCapabilities);

            const bool wasActive = wasSatisfied and entry.ManuallyActive;
            const bool isActive = entry.IsActive();

            if (not wasActive and isActive)
            {
                auto result = entry.Overlay->OnAttach(context);
                if (not result.has_value())
                {
                    Log::Warn(LogEngine, "Overlay '{}' OnAttach failed: {}", entry.Overlay->GetName(), result.error().GetMessage());
                    entry.ManuallyActive = false;
                }
            }
            else if (wasActive and not isActive)
            {
                auto result = entry.Overlay->OnDetach(context);
                if (not result.has_value())
                {
                    Log::Warn(LogEngine, "Overlay '{}' OnDetach failed: {}", entry.Overlay->GetName(), result.error().GetMessage());
                }
            }
        }
    }

    auto OverlayStack::ProcessEvents(EngineContext& context, EventQueue& events) -> bool
    {
        // Top-down: highest priority first (reverse iteration of sorted entries).
        for (auto& entry : std::ranges::views::reverse(m_entries))
        {
            if (entry.IsActive())
            {
                if (entry.Overlay->OnEvent(context, events))
                {
                    return true;
                }
            }
        }
        return false;
    }

    void OverlayStack::Update(EngineContext& context, float deltaTime)
    {
        for (auto& entry : m_entries)
        {
            if (entry.IsActive())
            {
                entry.Overlay->OnUpdate(context, deltaTime);
            }
        }
    }

    void OverlayStack::Render(EngineContext& context)
    {
        for (auto& entry : m_entries)
        {
            if (entry.IsActive())
            {
                entry.Overlay->OnRender(context);
            }
        }
    }

    void OverlayStack::Activate(std::type_index overlayType, EngineContext& context)
    {
        for (auto& entry : m_entries)
        {
            if (entry.Type == overlayType)
            {
                if (entry.IsActive())
                {
                    return; // Already active, avoid double-attach.
                }

                entry.ManuallyActive = true;
                if (entry.CapabilitySatisfied)
                {
                    auto result = entry.Overlay->OnAttach(context);
                    if (not result.has_value())
                    {
                        Log::Warn(LogEngine, "Overlay '{}' OnAttach failed on Activate: {}", entry.Overlay->GetName(), result.error().GetMessage());
                        entry.ManuallyActive = false;
                    }
                }
                return;
            }
        }
    }

    void OverlayStack::Deactivate(std::type_index overlayType, EngineContext& context)
    {
        for (auto& entry : m_entries)
        {
            if (entry.Type == overlayType)
            {
                const bool wasActive = entry.IsActive();
                entry.ManuallyActive = false;
                if (wasActive)
                {
                    auto result = entry.Overlay->OnDetach(context);
                    if (not result.has_value())
                    {
                        Log::Warn(LogEngine, "Overlay '{}' OnDetach failed on Deactivate: {}", entry.Overlay->GetName(), result.error().GetMessage());
                    }
                }
                return;
            }
        }
    }

    void OverlayStack::DetachAll(EngineContext& context)
    {
        for (auto& entry : std::ranges::views::reverse(m_entries))
        {
            if (entry.IsActive())
            {
                auto result = entry.Overlay->OnDetach(context);
                if (not result.has_value())
                {
                    Log::Warn(LogEngine, "Overlay '{}' OnDetach failed during DetachAll: {}", entry.Overlay->GetName(), result.error().GetMessage());
                }
                entry.ManuallyActive = false;
            }
        }
    }

    auto OverlayStack::GetEntries() const -> std::span<const OverlayEntry>
    {
        return m_entries;
    }

    auto OverlayStack::GetEntryCount() const -> size_t
    {
        return m_entries.size();
    }

} // namespace Wayfinder
