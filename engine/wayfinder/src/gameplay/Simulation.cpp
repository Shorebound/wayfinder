#include "Simulation.h"

#include "core/Log.h"
#include "scene/Scene.h"

namespace Wayfinder
{
    Simulation::Simulation() = default;

    Simulation::~Simulation() = default;

    auto Simulation::Initialise(EngineContext& /*context*/) -> Result<void>
    {
        Log::Info(LogEngine, "Simulation initialised");
        return {};
    }

    void Simulation::Shutdown()
    {
        UnloadCurrentScene();
        Log::Info(LogEngine, "Simulation shut down");
    }

    void Simulation::Update(float deltaTime)
    {
        m_world.progress(deltaTime);
    }

    auto Simulation::GetWorld() -> flecs::world&
    {
        return m_world;
    }

    auto Simulation::GetWorld() const -> const flecs::world&
    {
        return m_world;
    }

    auto Simulation::GetCurrentScene() -> Scene*
    {
        return m_currentScene.get();
    }

    auto Simulation::GetCurrentScene() const -> const Scene*
    {
        return m_currentScene.get();
    }

    void Simulation::LoadScene(std::string_view scenePath)
    {
        /// @prototype Full scene loading requires AssetService integration.
        Log::Warn(LogEngine, "Simulation::LoadScene('{}') is a stub - AssetService not yet wired", scenePath);
    }

    void Simulation::UnloadCurrentScene()
    {
        m_currentScene.reset();
    }

} // namespace Wayfinder
