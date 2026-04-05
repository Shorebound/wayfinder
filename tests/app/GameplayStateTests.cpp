#include "app/EngineContext.h"
#include "app/IApplicationState.h"
#include "app/StateSubsystem.h"
#include "app/SubsystemManifest.h"
#include "app/SubsystemRegistry.h"
#include "gameplay/Simulation.h"

// These headers are from the implementation we will create (Task 2 GREEN phase).
// For now they will cause compile errors -- that is expected in TDD RED.
#include "gameplay/GameplayState.h"
#include "gameplay/SimulationConfig.h"

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    /// Helper: create an EngineContext with a state-subsystem manifest containing Simulation.
    struct GameplayStateTestFixture
    {
        SubsystemRegistry<StateSubsystem> stateRegistry;
        std::optional<SubsystemManifest<StateSubsystem>> stateManifest;
        EngineContext context;

        GameplayStateTestFixture()
        {
            stateRegistry.Register<Simulation>();
            auto result = stateRegistry.Finalise();
            REQUIRE(result.has_value());
            stateManifest.emplace(std::move(*result));

            // Initialise subsystems (Simulation)
            CapabilitySet caps;
            auto initResult = stateManifest->Initialise(context, caps);
            REQUIRE(initResult.has_value());

            // Wire into context
            context.SetStateSubsystems(&*stateManifest);
        }

        ~GameplayStateTestFixture()
        {
            stateManifest->Shutdown();
        }
    };

    TEST_SUITE("GameplayState")
    {
        TEST_CASE("GameplayState implements IApplicationState")
        {
            static_assert(std::derived_from<GameplayState, IApplicationState>);
        }

        TEST_CASE("GameplayState returns correct name")
        {
            GameplayState state;
            CHECK(state.GetName() == "GameplayState");
        }

        TEST_CASE("GameplayState enters and creates extractor via Simulation")
        {
            GameplayStateTestFixture fixture;
            GameplayState state;

            auto result = state.OnEnter(fixture.context);
            CHECK(result.has_value());

            // After enter, OnRender should not crash (extractor was created).
            state.OnRender(fixture.context);

            auto exitResult = state.OnExit(fixture.context);
            CHECK(exitResult.has_value());
        }

        TEST_CASE("GameplayState delegates update to Simulation")
        {
            GameplayStateTestFixture fixture;
            GameplayState state;

            auto enterResult = state.OnEnter(fixture.context);
            REQUIRE(enterResult.has_value());

            // Create a flecs system to count ticks
            auto& sim = fixture.context.GetStateSubsystem<Simulation>();
            int tickCount = 0;
            sim.GetWorld()
                .system("TestTickCounter")
                .kind(flecs::OnUpdate)
                .run([&tickCount](flecs::iter& /*it*/)
            {
                ++tickCount;
            });

            state.OnUpdate(fixture.context, 0.016f);
            CHECK(tickCount == 1);

            state.OnUpdate(fixture.context, 0.016f);
            CHECK(tickCount == 2);

            state.OnExit(fixture.context);
        }

        TEST_CASE("GameplayState exits cleanly and releases extractor")
        {
            GameplayStateTestFixture fixture;
            GameplayState state;

            auto enterResult = state.OnEnter(fixture.context);
            REQUIRE(enterResult.has_value());

            auto exitResult = state.OnExit(fixture.context);
            CHECK(exitResult.has_value());

            // After exit, a second exit should not crash (already cleaned up).
            // The state should be in a clean post-exit state.
        }
    }

    TEST_SUITE("SimulationConfig")
    {
        TEST_CASE("SimulationConfig has BootScenePath field")
        {
            SimulationConfig config;
            CHECK(config.BootScenePath.empty());
        }

        TEST_CASE("SimulationConfig has FixedTickRate default")
        {
            SimulationConfig config;
            CHECK(config.FixedTickRate == doctest::Approx(60.0f));
        }
    }

} // namespace Wayfinder::Tests
