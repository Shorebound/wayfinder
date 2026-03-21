#include "rendering/pipeline/SceneRenderExtractor.h"
#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/entity/Entity.h"

#include <doctest/doctest.h>
#include <flecs.h>
#include <nlohmann/json.hpp>

/// ── MeshComponent ────────────────────────────────────────
namespace Wayfinder::Tests
{

    TEST_CASE("MeshComponent stores primitive and dimensions")
    {
        Wayfinder::MeshComponent mesh;
        CHECK(mesh.Primitive == Wayfinder::MeshPrimitive::Cube);
        CHECK(mesh.Dimensions.x == doctest::Approx(1.0f));
        CHECK(mesh.Dimensions.y == doctest::Approx(1.0f));
        CHECK(mesh.Dimensions.z == doctest::Approx(1.0f));
    }

    /// ── MaterialComponent has no render-state fields ─────────

    TEST_CASE("MaterialComponent has no wireframe field")
    {
        Wayfinder::MaterialComponent material;

        /// MaterialComponent should only have material-related fields.
        CHECK(material.BaseColour.r == Wayfinder::Colour::White().r);
        CHECK(material.BaseColour.g == Wayfinder::Colour::White().g);
        CHECK(material.BaseColour.b == Wayfinder::Colour::White().b);
        CHECK(material.BaseColour.a == Wayfinder::Colour::White().a);
        CHECK_FALSE(material.HasBaseColourOverride);
        CHECK_FALSE(material.MaterialAssetId.has_value());
    }

    /// ── RenderOverrideComponent ──────────────────────────────

    TEST_CASE("RenderOverrideComponent defaults to no overrides")
    {
        Wayfinder::RenderOverrideComponent renderOverride;
        CHECK_FALSE(renderOverride.Wireframe.has_value());
    }

    TEST_CASE("RenderOverrideComponent is an opt-in override")
    {
        flecs::world world;
        Wayfinder::Scene::RegisterCoreECS(world);
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        registry.RegisterComponents(world);
        Wayfinder::Scene scene(world, registry, "RenderOverride Test");

        SUBCASE("Entity without RenderOverrideComponent has no wireframe override")
        {
            Wayfinder::Entity entity = scene.CreateEntity("NoOverride");
            entity.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{});
            entity.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});
            entity.AddComponent<Wayfinder::RenderableComponent>(Wayfinder::RenderableComponent{});

            CHECK_FALSE(entity.HasComponent<Wayfinder::RenderOverrideComponent>());
        }

        SUBCASE("Entity with RenderOverrideComponent has wireframe control")
        {
            Wayfinder::Entity entity = scene.CreateEntity("WithOverride");
            entity.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{});
            entity.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});
            entity.AddComponent<Wayfinder::RenderableComponent>(Wayfinder::RenderableComponent{});

            Wayfinder::RenderOverrideComponent renderOverride;
            renderOverride.Wireframe = true;
            entity.AddComponent<Wayfinder::RenderOverrideComponent>(renderOverride);

            CHECK(entity.HasComponent<Wayfinder::RenderOverrideComponent>());
            CHECK(entity.GetComponent<Wayfinder::RenderOverrideComponent>().Wireframe.has_value());
            CHECK(*entity.GetComponent<Wayfinder::RenderOverrideComponent>().Wireframe == true);
        }

        SUBCASE("RenderOverrideComponent with no fields set has no wireframe value")
        {
            Wayfinder::Entity entity = scene.CreateEntity("EmptyOverride");
            entity.AddComponent<Wayfinder::RenderOverrideComponent>(Wayfinder::RenderOverrideComponent{});

            CHECK(entity.HasComponent<Wayfinder::RenderOverrideComponent>());
            CHECK_FALSE(entity.GetComponent<Wayfinder::RenderOverrideComponent>().Wireframe.has_value());
        }

        scene.Shutdown();
    }

    /// ── Serialisation round-trip ─────────────────────────────

    TEST_CASE("RenderOverrideComponent serialisation round-trip with wireframe=true")
    {
        flecs::world world;
        Wayfinder::Scene::RegisterCoreECS(world);
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        registry.RegisterComponents(world);
        Wayfinder::Scene scene(world, registry, "Serialise RenderOverride True");

        Wayfinder::Entity entity = scene.CreateEntity("WireframeEntity");
        entity.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{});

        Wayfinder::RenderOverrideComponent renderOverride;
        renderOverride.Wireframe = true;
        entity.AddComponent<Wayfinder::RenderOverrideComponent>(renderOverride);

        /// Serialise.
        nlohmann::json componentTables = nlohmann::json::object();
        registry.SerialiseComponents(entity, componentTables);

        CHECK(componentTables.contains("render_override"));
        const auto& overrideTable = componentTables["render_override"];
        REQUIRE(overrideTable.is_object());
        CHECK(overrideTable["wireframe"].get<bool>() == true);

        /// Deserialise into a new entity.
        Wayfinder::Entity entity2 = scene.CreateEntity("Deserialised");
        registry.ApplyComponents(componentTables, entity2);

        CHECK(entity2.HasComponent<Wayfinder::RenderOverrideComponent>());
        const auto& restored = entity2.GetComponent<Wayfinder::RenderOverrideComponent>();
        CHECK(restored.Wireframe.has_value());
        CHECK(*restored.Wireframe == true);

        scene.Shutdown();
    }

    TEST_CASE("RenderOverrideComponent serialisation round-trip with wireframe=false")
    {
        flecs::world world;
        Wayfinder::Scene::RegisterCoreECS(world);
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        registry.RegisterComponents(world);
        Wayfinder::Scene scene(world, registry, "Serialise RenderOverride False");

        Wayfinder::Entity entity = scene.CreateEntity("SolidEntity");
        entity.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{});

        Wayfinder::RenderOverrideComponent renderOverride;
        renderOverride.Wireframe = false;
        entity.AddComponent<Wayfinder::RenderOverrideComponent>(renderOverride);

        /// Serialise.
        nlohmann::json componentTables = nlohmann::json::object();
        registry.SerialiseComponents(entity, componentTables);

        CHECK(componentTables.contains("render_override"));
        const auto& overrideTable = componentTables["render_override"];
        REQUIRE(overrideTable.is_object());
        CHECK(overrideTable.contains("wireframe"));
        CHECK(overrideTable["wireframe"].get<bool>() == false);

        /// Deserialise into a new entity.
        Wayfinder::Entity entity2 = scene.CreateEntity("DeserialisedSolid");
        registry.ApplyComponents(componentTables, entity2);

        CHECK(entity2.HasComponent<Wayfinder::RenderOverrideComponent>());
        const auto& restored = entity2.GetComponent<Wayfinder::RenderOverrideComponent>();
        CHECK(restored.Wireframe.has_value());
        CHECK(*restored.Wireframe == false);

        scene.Shutdown();
    }

    TEST_CASE("RenderOverrideComponent serialisation skips empty component")
    {
        flecs::world world;
        Wayfinder::Scene::RegisterCoreECS(world);
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        registry.RegisterComponents(world);
        Wayfinder::Scene scene(world, registry, "Serialise RenderOverride Empty");

        Wayfinder::Entity entity = scene.CreateEntity("EmptyOverrideEntity");
        entity.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{});
        entity.AddComponent<Wayfinder::RenderOverrideComponent>(Wayfinder::RenderOverrideComponent{});

        /// Serialise — component with no fields set should not emit a table.
        nlohmann::json componentTables = nlohmann::json::object();
        registry.SerialiseComponents(entity, componentTables);

        CHECK_FALSE(componentTables.contains("render_override"));

        scene.Shutdown();
    }

    TEST_CASE("MaterialComponent serialisation has no wireframe field")
    {
        flecs::world world;
        Wayfinder::Scene::RegisterCoreECS(world);
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        registry.RegisterComponents(world);
        Wayfinder::Scene scene(world, registry, "Serialise Material");

        Wayfinder::MaterialComponent material;
        material.BaseColour = Wayfinder::Colour::Red();

        Wayfinder::Entity entity = scene.CreateEntity("MaterialEntity");
        entity.AddComponent<Wayfinder::MaterialComponent>(material);

        /// Serialise.
        nlohmann::json componentTables = nlohmann::json::object();
        registry.SerialiseComponents(entity, componentTables);

        CHECK(componentTables.contains("material"));
        const auto& materialTable = componentTables["material"];
        REQUIRE(materialTable.is_object());

        /// Material table should NOT contain wireframe.
        CHECK_FALSE(materialTable.contains("wireframe"));
        CHECK(materialTable.contains("base_colour"));

        scene.Shutdown();
    }

    /// ── Validation rejects malformed authoring ───────────────

    TEST_CASE("Validation rejects non-boolean wireframe in render_override")
    {
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();

        nlohmann::json badTable = nlohmann::json::object();
        badTable["wireframe"] = "yes";

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("render_override", badTable, error));
        CHECK_FALSE(error.empty());
    }

    /// ── Extractor reads RenderOverrideComponent ──────────────

    TEST_CASE("Extractor uses RenderOverrideComponent for wireframe")
    {
        flecs::world world;
        Wayfinder::Scene::RegisterCoreECS(world);
        Wayfinder::RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        registry.RegisterComponents(world);
        Wayfinder::Scene scene(world, registry, "Extractor Override Test");

        Wayfinder::Entity camera = scene.CreateEntity("Camera");
        camera.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{4.0f, 3.0f, 4.0f}});
        Wayfinder::CameraComponent cameraComponent;
        cameraComponent.Primary = true;
        cameraComponent.Target = {0.0f, 0.5f, 0.0f};
        camera.AddComponent<Wayfinder::CameraComponent>(cameraComponent);

        // Entity with wireframe override enabled — should get SolidAndWireframe fill mode
        Wayfinder::Entity wireframeCube = scene.CreateEntity("WireframeCube");
        wireframeCube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{-2.0f, 0.5f, 0.0f}});
        wireframeCube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});
        wireframeCube.AddComponent<Wayfinder::RenderableComponent>(Wayfinder::RenderableComponent{});
        Wayfinder::RenderOverrideComponent wireframeOverride;
        wireframeOverride.Wireframe = true;
        wireframeCube.AddComponent<Wayfinder::RenderOverrideComponent>(wireframeOverride);

        // Entity with wireframe override disabled — should get Solid fill mode
        Wayfinder::Entity solidCube = scene.CreateEntity("SolidCube");
        solidCube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{0.0f, 0.5f, 0.0f}});
        solidCube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});
        solidCube.AddComponent<Wayfinder::RenderableComponent>(Wayfinder::RenderableComponent{});
        Wayfinder::RenderOverrideComponent solidOverride;
        solidOverride.Wireframe = false;
        solidCube.AddComponent<Wayfinder::RenderOverrideComponent>(solidOverride);

        // Entity with RenderOverrideComponent but no Wireframe set — FillMode left unset
        Wayfinder::Entity emptyOverrideCube = scene.CreateEntity("EmptyOverrideCube");
        emptyOverrideCube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{1.0f, 0.5f, 0.0f}});
        emptyOverrideCube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});
        emptyOverrideCube.AddComponent<Wayfinder::RenderableComponent>(Wayfinder::RenderableComponent{});
        emptyOverrideCube.AddComponent<Wayfinder::RenderOverrideComponent>(Wayfinder::RenderOverrideComponent{});

        // Entity without RenderOverrideComponent — FillMode left unset (no override)
        Wayfinder::Entity defaultCube = scene.CreateEntity("DefaultCube");
        defaultCube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{2.0f, 0.5f, 0.0f}});
        defaultCube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});
        defaultCube.AddComponent<Wayfinder::RenderableComponent>(Wayfinder::RenderableComponent{});

        world.progress(0.016f);

        Wayfinder::SceneRenderExtractor extractor;
        const Wayfinder::RenderFrame frame = extractor.Extract(scene);
        const Wayfinder::RenderPass* mainPass = frame.FindPass(Wayfinder::RenderPassIds::MainScene);

        scene.Shutdown();

        REQUIRE(mainPass != nullptr);
        REQUIRE(mainPass->Meshes.size() == 4);

        // Verify: wireframe=true → SolidAndWireframe,
        //         wireframe=false → Solid,
        //         wireframe unset (component present) → FillMode unset,
        //         no component → FillMode unset.
        int countSolid = 0;
        int countSolidAndWireframe = 0;
        int countUnset = 0;
        for (const auto& mesh : mainPass->Meshes)
        {
            if (!mesh.Material.StateOverrides.FillMode.has_value())
            {
                ++countUnset;
            }
            else if (mesh.Material.StateOverrides.FillMode == Wayfinder::RenderFillMode::Solid)
            {
                ++countSolid;
            }
            else if (mesh.Material.StateOverrides.FillMode == Wayfinder::RenderFillMode::SolidAndWireframe)
            {
                ++countSolidAndWireframe;
            }
        }

        CHECK(countSolidAndWireframe == 1);
        CHECK(countSolid == 1);
        CHECK(countUnset == 2);
    }
}
