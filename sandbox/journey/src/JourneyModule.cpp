#include "core/GameModule.h"
#include "application/EntryPoint.h"

class JourneyModule : public Wayfinder::GameModule
{
    void OnInitialize(const Wayfinder::EngineContext& /*ctx*/) override {}

    void OnUpdate(float /*deltaTime*/) override {}
};

std::unique_ptr<Wayfinder::GameModule> Wayfinder::CreateGameModule()
{
    return std::make_unique<JourneyModule>();
}
