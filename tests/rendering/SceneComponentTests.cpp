#include "rendering/SceneRenderExtractor.h"
#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/entity/Entity.h"

#include <doctest/doctest.h>
#include <flecs.h>

// ── MeshComponent with optional AssetId ──────────────────

TEST_CASE("MeshComponent supports primitive fallback and optional asset id")
{
    flecs::world world;
    Wayfinder::Scene::RegisterCoreECS(world);
    Wayfinder::RuntimeComponentRegistry registry;
    registry.AddCoreEntries();
    registry.RegisterComponents(world);
    Wayfinder::Scene scene(world, registry, "MeshAssetId Test");

    SUBCASE("Default MeshComponent has no asset id")
    {
        Wayfinder::Entity entity = scene.CreateEntity("DefaultMesh");
        entity.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});

        const auto& mesh = entity.GetComponent<Wayfinder::MeshComponent>();
        CHECK(mesh.Primitive == Wayfinder::MeshPrimitive::Cube);
        CHECK_FALSE(mesh.MeshAssetId.has_value());
    }

    SUBCASE("MeshComponent can hold an AssetId")
    {
        Wayfinder::MeshComponent mesh;
        mesh.MeshAssetId = Wayfinder::AssetId::Generate();
        Wayfinder::Entity entity = scene.CreateEntity("AssetMesh");
        entity.AddComponent<Wayfinder::MeshComponent>(mesh);

        const auto& stored = entity.GetComponent<Wayfinder::MeshComponent>();
        CHECK(stored.MeshAssetId.has_value());
        CHECK(stored.MeshAssetId == mesh.MeshAssetId);
    }

    scene.Shutdown();
}

// ── MaterialComponent has no render-state fields ─────────

TEST_CASE("MaterialComponent has no wireframe field")
{
    Wayfinder::MaterialComponent material;

    // MaterialComponent should only have material-related fields
    CHECK(material.BaseColor == Wayfinder::Color::White());
    CHECK_FALSE(material.HasBaseColorOverride);
    CHECK_FALSE(material.MaterialAssetId.has_value());
}

// ── RenderOverrideComponent ──────────────────────────────

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
        CHECK(entity.GetComponent<Wayfinder::RenderOverrideComponent>().Wireframe == true);
    }

    scene.Shutdown();
}

// ── Serialisation round-trip ─────────────────────────────

TEST_CASE("RenderOverrideComponent serialisation round-trip")
{
    flecs::world world;
    Wayfinder::Scene::RegisterCoreECS(world);
    Wayfinder::RuntimeComponentRegistry registry;
    registry.AddCoreEntries();
    registry.RegisterComponents(world);
    Wayfinder::Scene scene(world, registry, "Serialise RenderOverride");

    Wayfinder::Entity entity = scene.CreateEntity("WireframeEntity");
    entity.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{});

    Wayfinder::RenderOverrideComponent renderOverride;
    renderOverride.Wireframe = true;
    entity.AddComponent<Wayfinder::RenderOverrideComponent>(renderOverride);

    // Serialize
    toml::table componentTables;
    registry.SerializeComponents(entity, componentTables);

    CHECK(componentTables.contains("render_override"));
    const toml::table* overrideTable = componentTables["render_override"].as_table();
    REQUIRE(overrideTable != nullptr);
    CHECK(overrideTable->at("wireframe").value_or(false) == true);

    // Deserialize into a new entity
    Wayfinder::Entity entity2 = scene.CreateEntity("Deserialized");
    registry.ApplyComponents(componentTables, entity2);

    CHECK(entity2.HasComponent<Wayfinder::RenderOverrideComponent>());
    CHECK(entity2.GetComponent<Wayfinder::RenderOverrideComponent>().Wireframe == true);

    scene.Shutdown();
}

TEST_CASE("MeshComponent with AssetId serialisation round-trip")
{
    flecs::world world;
    Wayfinder::Scene::RegisterCoreECS(world);
    Wayfinder::RuntimeComponentRegistry registry;
    registry.AddCoreEntries();
    registry.RegisterComponents(world);
    Wayfinder::Scene scene(world, registry, "Serialise MeshAssetId");

    Wayfinder::MeshComponent mesh;
    mesh.Primitive = Wayfinder::MeshPrimitive::Cube;
    mesh.Dimensions = { 2.0f, 3.0f, 4.0f };
    mesh.MeshAssetId = Wayfinder::AssetId::Generate();

    Wayfinder::Entity entity = scene.CreateEntity("AssetMesh");
    entity.AddComponent<Wayfinder::MeshComponent>(mesh);

    // Serialize
    toml::table componentTables;
    registry.SerializeComponents(entity, componentTables);

    CHECK(componentTables.contains("mesh"));
    const toml::table* meshTable = componentTables["mesh"].as_table();
    REQUIRE(meshTable != nullptr);
    CHECK(meshTable->contains("asset_id"));
    CHECK(meshTable->at("asset_id").value_or(std::string{}) == mesh.MeshAssetId->ToString());

    // Deserialize into a new entity
    Wayfinder::Entity entity2 = scene.CreateEntity("DeserializedMesh");
    registry.ApplyComponents(componentTables, entity2);

    CHECK(entity2.HasComponent<Wayfinder::MeshComponent>());
    const auto& restored = entity2.GetComponent<Wayfinder::MeshComponent>();
    CHECK(restored.MeshAssetId.has_value());
    CHECK(restored.MeshAssetId->ToString() == mesh.MeshAssetId->ToString());
    CHECK(restored.Primitive == Wayfinder::MeshPrimitive::Cube);

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
    material.BaseColor = Wayfinder::Color::Red();

    Wayfinder::Entity entity = scene.CreateEntity("MaterialEntity");
    entity.AddComponent<Wayfinder::MaterialComponent>(material);

    // Serialize
    toml::table componentTables;
    registry.SerializeComponents(entity, componentTables);

    CHECK(componentTables.contains("material"));
    const toml::table* materialTable = componentTables["material"].as_table();
    REQUIRE(materialTable != nullptr);

    // Material table should NOT contain wireframe
    CHECK_FALSE(materialTable->contains("wireframe"));
    CHECK(materialTable->contains("base_color"));

    scene.Shutdown();
}

// ── Extractor reads RenderOverrideComponent ──────────────

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

    // Entity with wireframe override
    Wayfinder::Entity wireframeCube = scene.CreateEntity("WireframeCube");
    wireframeCube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{0.0f, 0.5f, 0.0f}});
    wireframeCube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});
    wireframeCube.AddComponent<Wayfinder::RenderableComponent>(Wayfinder::RenderableComponent{});
    Wayfinder::RenderOverrideComponent renderOverride;
    renderOverride.Wireframe = true;
    wireframeCube.AddComponent<Wayfinder::RenderOverrideComponent>(renderOverride);

    // Entity without wireframe override (should get default SolidAndWireframe from extractor)
    Wayfinder::Entity solidCube = scene.CreateEntity("SolidCube");
    solidCube.AddComponent<Wayfinder::TransformComponent>(Wayfinder::TransformComponent{{2.0f, 0.5f, 0.0f}});
    solidCube.AddComponent<Wayfinder::MeshComponent>(Wayfinder::MeshComponent{});
    solidCube.AddComponent<Wayfinder::RenderableComponent>(Wayfinder::RenderableComponent{});

    world.progress(0.016f);

    Wayfinder::SceneRenderExtractor extractor;
    const Wayfinder::RenderFrame frame = extractor.Extract(scene);
    const Wayfinder::RenderPass* mainPass = frame.FindPass(Wayfinder::RenderPassIds::MainScene);

    scene.Shutdown();

    REQUIRE(mainPass != nullptr);
    REQUIRE(mainPass->Meshes.size() == 2);

    // One should have wireframe override applied, the other should have the default
    bool foundWireframe = false;
    bool foundDefault = false;
    for (const auto& mesh : mainPass->Meshes)
    {
        if (mesh.Material.StateOverrides.FillMode == Wayfinder::RenderFillMode::SolidAndWireframe)
        {
            foundWireframe = true;
        }
        else
        {
            foundDefault = true;
        }
    }

    CHECK(foundWireframe);
    CHECK(foundDefault);
}
