#include "app/EngineConfig.h"
#include "gameplay/GameState.h"
#include "gameplay/GameplayTag.h"
#include "modules/ModuleRegistry.h"
#include "modules/Plugin.h"
#include "modules/registrars/StateRegistrar.h"
#include "modules/registrars/SystemRegistrar.h"
#include "modules/registrars/TagRegistrar.h"
#include "project/ProjectDescriptor.h"

#include "ecs/Flecs.h"
#include <doctest/doctest.h>
#include <string>
#include <vector>

namespace Wayfinder::Tests
{
    static bool g_TestPluginBuildCalled = false;

    static ProjectDescriptor MakeTestProject()
    {
        ProjectDescriptor desc{};
        desc.Name = "ModuleRegistryTest";
        return desc;
    }

    static EngineConfig MakeTestConfig()
    {
        return EngineConfig::LoadDefaults();
    }

    // ── StateRegistrar ──────────────────────────────────────

    TEST_SUITE("StateRegistrar")
    {
        TEST_CASE("Register adds a state descriptor")
        {
            StateRegistrar registrar;
            registrar.Register({"MainMenu", nullptr, nullptr});

            CHECK(registrar.GetDescriptors().size() == 1);
            CHECK(registrar.GetDescriptors()[0].Name == "MainMenu");
        }

        TEST_CASE("Duplicate state registration is rejected")
        {
            StateRegistrar registrar;
            registrar.Register({"MainMenu", nullptr, nullptr});
            registrar.Register({"MainMenu", nullptr, nullptr}); // duplicate

            CHECK(registrar.GetDescriptors().size() == 1);
        }

        TEST_CASE("SetInitial sets the initial state name")
        {
            StateRegistrar registrar;
            registrar.Register({"MainMenu", nullptr, nullptr});
            registrar.SetInitial("MainMenu");

            CHECK(registrar.GetInitial() == "MainMenu");
        }

        TEST_CASE("SetInitial with unregistered state is rejected")
        {
            StateRegistrar registrar;
            registrar.SetInitial("NonExistent");

            CHECK(registrar.GetInitial().empty());
        }

        TEST_CASE("GetInitial is empty by default")
        {
            StateRegistrar registrar;
            CHECK(registrar.GetInitial().empty());
        }
    }

    // ── SystemRegistrar ─────────────────────────────────────

    TEST_SUITE("SystemRegistrar")
    {
        TEST_CASE("Register adds a system descriptor")
        {
            SystemRegistrar registrar;
            registrar.Register("TestSystem", [](flecs::world&)
            {
            });

            CHECK(registrar.GetDescriptors().size() == 1);
            CHECK(registrar.GetDescriptors()[0].Name == "TestSystem");
        }

        TEST_CASE("Duplicate system registration is rejected")
        {
            SystemRegistrar registrar;
            registrar.Register("TestSystem", [](flecs::world&)
            {
            });
            registrar.Register("TestSystem", [](flecs::world&)
            {
            }); // duplicate

            CHECK(registrar.GetDescriptors().size() == 1);
        }

        TEST_CASE("Empty factory registration is rejected")
        {
            SystemRegistrar registrar;
            registrar.Register("BadSystem", nullptr);

            CHECK(registrar.GetDescriptors().empty());
        }

        TEST_CASE("ApplyToWorld calls factories in dependency order")
        {
            SystemRegistrar registrar;
            std::vector<std::string> callOrder;

            registrar.Register("SystemA", [&](flecs::world&)
            {
                callOrder.push_back("A");
            });
            registrar.Register("SystemB", [&](flecs::world&)
            {
                callOrder.push_back("B");
            }, {}, {"SystemA"}, {}); // B runs after A
            registrar.Register("SystemC", [&](flecs::world&)
            {
                callOrder.push_back("C");
            }, {}, {"SystemB"}, {}); // C runs after B

            flecs::world world;
            registrar.ApplyToWorld(world);

            REQUIRE(callOrder.size() == 3);
            // A must come before B, B before C
            size_t posA = 0, posB = 0, posC = 0;
            for (size_t i = 0; i < callOrder.size(); ++i)
            {
                if (callOrder[i] == "A")
                {
                    posA = i;
                }
                if (callOrder[i] == "B")
                {
                    posB = i;
                }
                if (callOrder[i] == "C")
                {
                    posC = i;
                }
            }
            CHECK(posA < posB);
            CHECK(posB < posC);
        }

        TEST_CASE("ApplyToWorld handles Before constraint")
        {
            SystemRegistrar registrar;
            std::vector<std::string> callOrder;

            registrar.Register("SystemA", [&](flecs::world&)
            {
                callOrder.push_back("A");
            }, {}, {}, {"SystemB"}); // A runs before B
            registrar.Register("SystemB", [&](flecs::world&)
            {
                callOrder.push_back("B");
            });

            flecs::world world;
            registrar.ApplyToWorld(world);

            REQUIRE(callOrder.size() == 2);
            size_t posA = 0, posB = 0;
            for (size_t i = 0; i < callOrder.size(); ++i)
            {
                if (callOrder[i] == "A")
                {
                    posA = i;
                }
                if (callOrder[i] == "B")
                {
                    posB = i;
                }
            }
            CHECK(posA < posB);
        }

        TEST_CASE("ApplyToWorld skips cyclic systems")
        {
            SystemRegistrar registrar;
            std::vector<std::string> callOrder;

            // A -> B -> A (cycle)
            registrar.Register("SystemA", [&](flecs::world&)
            {
                callOrder.push_back("A");
            }, {}, {"SystemB"}, {}); // A after B
            registrar.Register("SystemB", [&](flecs::world&)
            {
                callOrder.push_back("B");
            }, {}, {"SystemA"}, {}); // B after A

            flecs::world world;
            registrar.ApplyToWorld(world);

            // Both are cyclic, neither should be called
            CHECK(callOrder.empty());
        }

        TEST_CASE("ApplyToWorld initialises non-cyclic systems alongside cycle")
        {
            SystemRegistrar registrar;
            std::vector<std::string> callOrder;

            registrar.Register("Independent", [&](flecs::world&)
            {
                callOrder.push_back("I");
            });
            registrar.Register("SystemA", [&](flecs::world&)
            {
                callOrder.push_back("A");
            }, {}, {"SystemB"}, {});
            registrar.Register("SystemB", [&](flecs::world&)
            {
                callOrder.push_back("B");
            }, {}, {"SystemA"}, {});

            flecs::world world;
            registrar.ApplyToWorld(world);

            CHECK(callOrder.size() == 1);
            CHECK(callOrder[0] == "I");
        }

        TEST_CASE("ApplyToWorld ignores unknown After dependency")
        {
            SystemRegistrar registrar;
            std::vector<std::string> callOrder;

            registrar.Register("SystemA", [&](flecs::world&)
            {
                callOrder.push_back("A");
            }, {}, {"NonExistent"}, {});

            flecs::world world;
            registrar.ApplyToWorld(world);

            CHECK(callOrder.size() == 1);
            CHECK(callOrder[0] == "A");
        }
    }

    // ── TagRegistrar ────────────────────────────────────────

    TEST_SUITE("TagRegistrar")
    {
        TEST_CASE("Register adds a tag descriptor")
        {
            TagRegistrar registrar;
            registrar.Register({"Status.Burning", "On fire"});

            CHECK(registrar.GetDescriptors().size() == 1);
            CHECK(registrar.GetDescriptors()[0].Name == "Status.Burning");
            CHECK(registrar.GetDescriptors()[0].Comment == "On fire");
        }

        TEST_CASE("Duplicate tag registration is rejected")
        {
            TagRegistrar registrar;
            registrar.Register({"Status.Burning", "first"});
            registrar.Register({"Status.Burning", "second"});

            CHECK(registrar.GetDescriptors().size() == 1);
        }

        TEST_CASE("AddFile registers a tag file path")
        {
            TagRegistrar registrar;
            registrar.AddFile("config/tags/gameplay.toml");

            CHECK(registrar.GetFiles().size() == 1);
            CHECK(registrar.GetFiles()[0] == "config/tags/gameplay.toml");
        }

        TEST_CASE("Duplicate AddFile is skipped")
        {
            TagRegistrar registrar;
            registrar.AddFile("config/tags/gameplay.toml");
            registrar.AddFile("config/tags/gameplay.toml");

            CHECK(registrar.GetFiles().size() == 1);
        }
    }

    // ── ModuleRegistry ──────────────────────────────────────

    TEST_SUITE("ModuleRegistry")
    {
        TEST_CASE("RegisterSystem delegates to SystemRegistrar")
        {
            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            registry.RegisterSystem("TestSystem", [](flecs::world&)
            {
            });

            CHECK(registry.GetSystems().size() == 1);
            CHECK(registry.GetSystems()[0].Name == "TestSystem");
        }

        TEST_CASE("RegisterState delegates to StateRegistrar")
        {
            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            registry.RegisterState({"MainMenu", nullptr, nullptr});
            registry.SetInitialState("MainMenu");

            CHECK(registry.GetStateDescriptors().size() == 1);
            CHECK(registry.GetInitialState() == "MainMenu");
        }

        TEST_CASE("RegisterTag returns a valid tag")
        {
            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            auto tag = registry.RegisterTag("Status.Burning", "On fire");

            CHECK(tag.IsValid());
            CHECK(tag.GetName() == "Status.Burning");
            CHECK(registry.GetRegisteredTags().size() == 1);
        }

        TEST_CASE("RegisterTagFile stores the path")
        {
            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            registry.RegisterTagFile("config/tags/gameplay.toml");

            CHECK(registry.GetTagFiles().size() == 1);
        }

        TEST_CASE("RegisterComponent stores descriptor")
        {
            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            ModuleRegistry::ComponentDescriptor desc;
            desc.Key = "TestComponent";
            registry.RegisterComponent(std::move(desc));

            CHECK(registry.GetComponentDescriptors().size() == 1);
            CHECK(registry.GetComponentDescriptors()[0].Key == "TestComponent");
        }

        TEST_CASE("ApplyToWorld calls global factories")
        {
            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            bool called = false;
            registry.RegisterGlobal("TestGlobal", [&](flecs::world&)
            {
                called = true;
            });

            flecs::world world;
            registry.ApplyToWorld(world);

            CHECK(called);
        }

        TEST_CASE("ApplyToWorld calls system factories")
        {
            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            bool called = false;
            registry.RegisterSystem("TestSystem", [&](flecs::world&)
            {
                called = true;
            });

            flecs::world world;
            registry.ApplyToWorld(world);

            CHECK(called);
        }

        TEST_CASE("GetProject returns the project descriptor")
        {
            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            CHECK(registry.GetProject().Name == "ModuleRegistryTest");
        }

        TEST_CASE("GetConfig returns the engine configuration")
        {
            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            CHECK(registry.GetConfig().Physics.FixedTimestep == doctest::Approx(1.0f / 60.0f));
        }

        // ── Plugin ──────────────────────────────────────────────

        TEST_CASE("AddPlugin calls Build immediately")
        {
            struct TestPlugin : Plugin
            {
                void Build(ModuleRegistry&) override
                {
                    g_TestPluginBuildCalled = true;
                }
            };

            auto project = MakeTestProject();
            auto config = MakeTestConfig();
            ModuleRegistry registry(project, config);

            g_TestPluginBuildCalled = false;
            registry.AddPlugin<TestPlugin>();

            CHECK(g_TestPluginBuildCalled);
        }
    }
}
