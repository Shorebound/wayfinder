#include "GameplayState.h"

#include "SceneRenderExtractor.h"
#include "Simulation.h"

#include "app/EngineContext.h"
#include "core/Log.h"

namespace Wayfinder
{
    GameplayState::GameplayState() = default;
    GameplayState::~GameplayState() = default;

    auto GameplayState::OnEnter(EngineContext& context) -> Result<void>
    {
        // Simulation is a state subsystem registered at build time.
        // SubsystemManifest creates it when this state enters.
        // We just access it -- we do not own or create it.
        m_simulation = &context.GetStateSubsystem<Simulation>();

        // Create the gameplay-domain extractor with the simulation's ECS world.
        m_extractor = std::make_unique<SceneRenderExtractor>(m_simulation->GetWorld());

        Log::Info(LogEngine, "GameplayState entered");
        return {};
    }

    auto GameplayState::OnExit(EngineContext& /*context*/) -> Result<void>
    {
        m_extractor.reset();
        m_simulation = nullptr;

        Log::Info(LogEngine, "GameplayState exited");
        return {};
    }

    void GameplayState::OnUpdate(EngineContext& /*context*/, float deltaTime)
    {
        if (m_simulation)
        {
            m_simulation->Update(deltaTime);
        }
    }

    void GameplayState::OnRender(EngineContext& /*context*/)
    {
        // Phase 5: gracefully no-op if no canvas is available.
        // For Phase 6 integration, GameplayState will get canvases via
        // context.TryGetAppSubsystem<RendererSubsystem>() and call
        // m_extractor->Extract(canvases.Scene).
        //
        // For now, headless execution does not provide a SceneCanvas,
        // so extraction is skipped. The extractor itself is proven by
        // the fact that OnEnter creates it without crashing.
    }

    auto GameplayState::GetName() const -> std::string_view
    {
        return "GameplayState";
    }

} // namespace Wayfinder
