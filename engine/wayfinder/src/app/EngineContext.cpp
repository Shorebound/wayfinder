#include "EngineContext.h"

#include "OverlayStack.h"

namespace Wayfinder
{
    void EngineContext::RequestPop()
    {
        WAYFINDER_ASSERT(m_stateMachine, "ApplicationStateMachine not set");
        m_stateMachine->RequestPop();
    }

    void EngineContext::ActivateOverlay(std::type_index overlayType)
    {
        WAYFINDER_ASSERT(m_overlayStack, "OverlayStack not set");
        m_overlayStack->Activate(overlayType, *this);
    }

    void EngineContext::DeactivateOverlay(std::type_index overlayType)
    {
        WAYFINDER_ASSERT(m_overlayStack, "OverlayStack not set");
        m_overlayStack->Deactivate(overlayType, *this);
    }

    void EngineContext::RequestStop()
    {
        m_stopSource.request_stop();
    }

    auto EngineContext::IsStopRequested() const -> bool
    {
        return m_stopSource.stop_requested();
    }

    auto EngineContext::GetStopToken() const -> std::stop_token
    {
        return m_stopSource.get_token();
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

    void EngineContext::SetStateMachine(ApplicationStateMachine* stateMachine)
    {
        m_stateMachine = stateMachine;
    }

    void EngineContext::SetOverlayStack(OverlayStack* overlayStack)
    {
        m_overlayStack = overlayStack;
    }

} // namespace Wayfinder
