#include "gameplay/Simulation.h"

#include "app/EngineContext.h"
#include "app/StateSubsystem.h"
#include "core/InternedString.h"
#include "ecs/Flecs.h"
#include "gameplay/GameState.h"
#include "gameplay/StateMachine.h"
#include "plugins/EngineContextServiceProvider.h"
#include "plugins/ServiceProvider.h"

#include <doctest/doctest.h>

namespace Wayfinder::Tests
{

    TEST_SUITE("Simulation")
    {
        TEST_CASE("Simulation is a StateSubsystem")
        {
            static_assert(std::derived_from<Simulation, StateSubsystem>);
        }

        TEST_CASE("Default-constructs with a valid flecs world")
        {
            Simulation sim;
            auto& world = sim.GetWorld();
            // The world should be valid - entity creation should not crash.
            auto entity = world.entity("test");
            CHECK(entity.is_valid());
        }

        TEST_CASE("Initialises successfully with EngineContext")
        {
            Simulation sim;
            EngineContext ctx;
            auto result = sim.Initialise(ctx);
            CHECK(result.has_value());
        }

        TEST_CASE("Update ticks the flecs world")
        {
            Simulation sim;
            int tickCount = 0;

            sim.GetWorld()
                .system("TickCounter")
                .kind(flecs::OnUpdate)
                .run([&tickCount](flecs::iter& /*it*/)
            {
                ++tickCount;
            });

            sim.Update(0.016f);
            CHECK(tickCount == 1);

            sim.Update(0.016f);
            CHECK(tickCount == 2);
        }

        TEST_CASE("GetCurrentScene returns nullptr initially")
        {
            Simulation sim;
            CHECK(sim.GetCurrentScene() == nullptr);
        }

        TEST_CASE("UnloadCurrentScene is safe when no scene loaded")
        {
            Simulation sim;
            sim.UnloadCurrentScene(); // Should not crash.
            CHECK(sim.GetCurrentScene() == nullptr);
        }

        TEST_CASE("Shutdown resets scene")
        {
            Simulation sim;
            EngineContext ctx;
            auto result = sim.Initialise(ctx);
            REQUIRE(result.has_value());
            sim.Shutdown();
            CHECK(sim.GetCurrentScene() == nullptr);
        }
    }

    TEST_SUITE("EngineContextServiceProvider")
    {
        TEST_CASE("Satisfies ServiceProvider concept")
        {
            static_assert(ServiceProvider<EngineContextServiceProvider>);
        }
    }

    TEST_SUITE("Sub-state pattern")
    {
        TEST_CASE("StateMachine can be used as member for sub-states")
        {
            // Demonstrates SIM-06: sub-states are state-internal.
            // An ApplicationState creates its own StateMachine in OnEnter.
            StateMachine<std::string> sm;
            sm.AddState({.Id = "Exploration",
                .OnEnter =
                    []
            {
            },
                .OnExit =
                    []
            {
            },
                .AllowedTransitions = {"Combat"}});
            sm.AddState({.Id = "Combat",
                .OnEnter =
                    []
            {
            },
                .OnExit =
                    []
            {
            },
                .AllowedTransitions = {"Exploration"}});

            auto result = sm.Finalise("Exploration");
            REQUIRE(result.has_value());

            sm.Start();
            CHECK(sm.GetCurrentState() == "Exploration");

            auto transition = sm.TransitionTo("Combat");
            CHECK(transition.has_value());
            sm.ProcessPending();
            CHECK(sm.GetCurrentState() == "Combat");
        }
    }

    TEST_SUITE("ActiveGameState")
    {
        TEST_CASE("ActiveGameState singleton can be set and read on flecs world")
        {
            // Demonstrates SIM-07: game code sets ActiveGameState via world.set.
            Simulation sim;
            auto& world = sim.GetWorld();

            world.set<ActiveGameState>({
                .Current = InternedString::Intern("Playing"),
                .Previous = InternedString::Intern("MainMenu"),
            });

            const auto& state = world.get<ActiveGameState>();
            CHECK(state.Current == InternedString::Intern("Playing"));
            CHECK(state.Previous == InternedString::Intern("MainMenu"));
        }
    }

} // namespace Wayfinder::Tests
