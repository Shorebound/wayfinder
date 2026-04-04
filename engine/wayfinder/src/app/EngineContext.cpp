#include "EngineContext.h"

namespace Wayfinder
{
    void EngineContext::RequestPop()
    {
        WAYFINDER_ASSERT(false, "RequestPop not yet implemented (Phase 4)");
    }

    void EngineContext::ActivateOverlay(std::type_index /*overlayType*/)
    {
        WAYFINDER_ASSERT(false, "ActivateOverlay not yet implemented (Phase 4)");
    }

    void EngineContext::DeactivateOverlay(std::type_index /*overlayType*/)
    {
        WAYFINDER_ASSERT(false, "DeactivateOverlay not yet implemented (Phase 4)");
    }

    void EngineContext::RequestStop()
    {
        m_stopRequested = true;
    }

    auto EngineContext::IsStopRequested() const -> bool
    {
        return m_stopRequested;
    }

    void EngineContext::SetAppSubsystems(SubsystemRegistry<AppSubsystem>* registry)
    {
        m_appSubsystems = registry;
    }

    void EngineContext::SetStateSubsystems(SubsystemRegistry<StateSubsystem>* registry)
    {
        m_stateSubsystems = registry;
    }

} // namespace Wayfinder
