#include "gameplay/GameStateMachine.h"
#include "gameplay/GameState.h"
#include "core/InternedString.h"
#include "modules/ModuleRegistry.h"
#include "app/EngineConfig.h"
#include "project/ProjectDescriptor.h"

#include <doctest/doctest.h>

#include <flecs.h>

#include <string>
#include <vector>

namespace Wayfinder::Tests
{
    /// Minimal helpers for headless testing.
    ProjectDescriptor MakeTestProject()
    {
        ProjectDescriptor desc{};
        desc.Name = "StateMachineTest";
        return desc;
    }

    EngineConfig MakeTestConfig()
    {
        return EngineConfig::LoadDefaults();
    }

    /// Set up a world with the ActiveGameState singleton (empty initial).
    void PrepareWorld(flecs::world& world)
    {
        world.set<ActiveGameState>({});
        world.set<ActiveGameplayTags>({});
    }

TEST_SUITE("GameStateMachine")
{
    TEST_CASE("Configure and Setup with no states")
    {
        flecs::world world;
        PrepareWorld(world);

        auto project = MakeTestProject();
        auto config = MakeTestConfig();
        ModuleRegistry registry(project, config);

        GameStateMachine sm;
        sm.Configure(world, &registry);
        CHECK_NOTHROW(sm.Setup());
    }

    TEST_CASE("TransitionTo changes current state")
    {
        flecs::world world;
        PrepareWorld(world);

        auto project = MakeTestProject();
        auto config = MakeTestConfig();
        ModuleRegistry registry(project, config);

        registry.RegisterState({"MainMenu", nullptr, nullptr});
        registry.RegisterState({"InGame", nullptr, nullptr});
        registry.SetInitialState("MainMenu");

        GameStateMachine sm;
        sm.Configure(world, &registry);
        sm.Setup();

        CHECK(sm.GetCurrentState() == "MainMenu");

        sm.TransitionTo("InGame");
        CHECK(sm.GetCurrentState() == "InGame");
    }

    TEST_CASE("TransitionTo calls OnExit and OnEnter callbacks")
    {
        flecs::world world;
        PrepareWorld(world);

        auto project = MakeTestProject();
        auto config = MakeTestConfig();
        ModuleRegistry registry(project, config);

        std::vector<std::string> callLog;

        ModuleRegistry::StateDescriptor menuState;
        menuState.Name = "MainMenu";
        menuState.OnEnter = [&](flecs::world&) { callLog.push_back("MainMenu.OnEnter"); };
        menuState.OnExit = [&](flecs::world&) { callLog.push_back("MainMenu.OnExit"); };

        ModuleRegistry::StateDescriptor gameState;
        gameState.Name = "InGame";
        gameState.OnEnter = [&](flecs::world&) { callLog.push_back("InGame.OnEnter"); };
        gameState.OnExit = [&](flecs::world&) { callLog.push_back("InGame.OnExit"); };

        registry.RegisterState(std::move(menuState));
        registry.RegisterState(std::move(gameState));
        registry.SetInitialState("MainMenu");

        GameStateMachine sm;
        sm.Configure(world, &registry);
        sm.Setup();

        callLog.clear(); // Clear initial transition log

        sm.TransitionTo("InGame");

        REQUIRE(callLog.size() == 2);
        CHECK(callLog[0] == "MainMenu.OnExit");
        CHECK(callLog[1] == "InGame.OnEnter");
    }

    TEST_CASE("TransitionTo same state is a no-op")
    {
        flecs::world world;
        PrepareWorld(world);

        auto project = MakeTestProject();
        auto config = MakeTestConfig();
        ModuleRegistry registry(project, config);

        std::vector<std::string> callLog;

        ModuleRegistry::StateDescriptor menuState;
        menuState.Name = "MainMenu";
        menuState.OnEnter = [&](flecs::world&) { callLog.push_back("MainMenu.OnEnter"); };

        registry.RegisterState(std::move(menuState));
        registry.SetInitialState("MainMenu");

        GameStateMachine sm;
        sm.Configure(world, &registry);
        sm.Setup();

        callLog.clear();

        sm.TransitionTo("MainMenu");
        CHECK(callLog.empty()); // No callbacks should fire
    }

    TEST_CASE("TransitionTo unknown state does not change state")
    {
        flecs::world world;
        PrepareWorld(world);

        auto project = MakeTestProject();
        auto config = MakeTestConfig();
        ModuleRegistry registry(project, config);

        registry.RegisterState({"MainMenu", nullptr, nullptr});
        registry.SetInitialState("MainMenu");

        GameStateMachine sm;
        sm.Configure(world, &registry);
        sm.Setup();

        sm.TransitionTo("NonExistentState");
        CHECK(sm.GetCurrentState() == "MainMenu");
    }

    TEST_CASE("ActiveGameState singleton tracks previous state")
    {
        flecs::world world;
        PrepareWorld(world);

        auto project = MakeTestProject();
        auto config = MakeTestConfig();
        ModuleRegistry registry(project, config);

        registry.RegisterState({"MainMenu", nullptr, nullptr});
        registry.RegisterState({"InGame", nullptr, nullptr});
        registry.SetInitialState("MainMenu");

        GameStateMachine sm;
        sm.Configure(world, &registry);
        sm.Setup();

        sm.TransitionTo("InGame");

        const auto& state = world.get<ActiveGameState>();
        CHECK(state.Current.GetString() == "InGame");
        CHECK(state.Previous.GetString() == "MainMenu");
    }

    TEST_CASE("Update is a no-op when not dirty")
    {
        flecs::world world;
        PrepareWorld(world);

        auto project = MakeTestProject();
        auto config = MakeTestConfig();
        ModuleRegistry registry(project, config);

        GameStateMachine sm;
        sm.Configure(world, &registry);
        sm.Setup();

        // Should not crash or do anything
        CHECK_NOTHROW(sm.Update());
    }

    TEST_CASE("MarkDirty causes re-evaluation on next Update")
    {
        flecs::world world;
        PrepareWorld(world);

        auto project = MakeTestProject();
        auto config = MakeTestConfig();
        ModuleRegistry registry(project, config);

        GameStateMachine sm;
        sm.Configure(world, &registry);
        sm.Setup();

        sm.MarkDirty();
        CHECK_NOTHROW(sm.Update()); // Should process dirty flag
    }

    // ── Run Conditions ──────────────────────────────────────

    TEST_CASE("InState run condition")
    {
        flecs::world world;
        PrepareWorld(world);

        auto condition = InState("MainMenu");

        // Before any state is set
        CHECK_FALSE(condition(world));

        // Set state to MainMenu
        world.get_mut<ActiveGameState>().Current = InternedString::Intern("MainMenu");
        CHECK(condition(world));

        // Set state to something else
        world.get_mut<ActiveGameState>().Current = InternedString::Intern("InGame");
        CHECK_FALSE(condition(world));
    }

    TEST_CASE("NotInState run condition")
    {
        flecs::world world;
        PrepareWorld(world);

        auto condition = NotInState("MainMenu");

        // Before any state is set (empty string != "MainMenu")
        CHECK(condition(world));

        world.get_mut<ActiveGameState>().Current = InternedString::Intern("MainMenu");
        CHECK_FALSE(condition(world));

        world.get_mut<ActiveGameState>().Current = InternedString::Intern("InGame");
        CHECK(condition(world));
    }

    TEST_CASE("HasTag run condition")
    {
        flecs::world world;
        PrepareWorld(world);

        auto tag = GameplayTag::FromName("Status.Burning");
        auto condition = HasTag(tag);

        CHECK_FALSE(condition(world));

        world.get_mut<ActiveGameplayTags>().Tags.AddTag(tag);
        CHECK(condition(world));
    }

    TEST_CASE("HasAnyTag run condition")
    {
        flecs::world world;
        PrepareWorld(world);

        auto burning = GameplayTag::FromName("Status.Burning");
        auto frozen = GameplayTag::FromName("Status.Frozen");
        auto condition = HasAnyTag({burning, frozen});

        CHECK_FALSE(condition(world));

        world.get_mut<ActiveGameplayTags>().Tags.AddTag(frozen);
        CHECK(condition(world));
    }

    TEST_CASE("AllOf composite run condition")
    {
        flecs::world world;
        PrepareWorld(world);

        world.get_mut<ActiveGameState>().Current = InternedString::Intern("InGame");
        auto burning = GameplayTag::FromName("Status.Burning");

        auto condition = AllOf({InState("InGame"), HasTag(burning)});

        CHECK_FALSE(condition(world)); // Tag missing

        world.get_mut<ActiveGameplayTags>().Tags.AddTag(burning);
        CHECK(condition(world)); // Both true
    }

    TEST_CASE("AnyOf composite run condition")
    {
        flecs::world world;
        PrepareWorld(world);

        auto condition = AnyOf({InState("MainMenu"), InState("InGame")});

        CHECK_FALSE(condition(world));

        world.get_mut<ActiveGameState>().Current = InternedString::Intern("InGame");
        CHECK(condition(world));
    }
}
} // namespace Wayfinder::Tests
