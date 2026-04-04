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

    void EngineContext::SetAppSubsystems(SubsystemManifest<AppSubsystem>* manifest)
    {
        m_appSubsystems = manifest;
    }

    void EngineContext::SetStateSubsystems(SubsystemManifest<StateSubsystem>* manifest)
    {
        m_stateSubsystems = manifest;
    }

    auto EngineContext::GetAppDescriptor() const -> const AppDescriptor&
    {
        WAYFINDER_ASSERT(m_appDescriptor, "AppDescriptor not set");
        return *m_appDescriptor;
    }

    auto EngineContext::TryGetAppDescriptor() const -> const AppDescriptor*
    {
        return m_appDescriptor;
    }

    void EngineContext::SetAppDescriptor(const AppDescriptor* descriptor)
    {
        m_appDescriptor = descriptor;
    }

} // namespace Wayfinder
