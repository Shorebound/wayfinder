#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/entity/Entity.h"
#include "TestHelpers.h"

#include <doctest/doctest.h>

#include <flecs.h>
#include <toml++/toml.hpp>

using namespace Wayfinder;
using TestHelpers::MakeTestRegistry;

namespace
{
    /// Helper: create a scene with a single entity, apply component data, then serialise.
    /// Returns the serialised TOML table containing component data.
    toml::table RoundTrip(const std::string& componentKey, const toml::table& inputData)
    {
        flecs::world world;
        auto registry = MakeTestRegistry();
        registry.RegisterComponents(world);
        Scene::RegisterCoreECS(world);
        Scene scene(world, registry, "SerializeTest");

        auto entity = scene.CreateEntity("TestEntity");

        // Apply components from the input data
        toml::table componentTables;
        componentTables.insert_or_assign(componentKey, inputData);
        registry.ApplyComponents(componentTables, entity);

        // Serialise the entity's components back out
        toml::table output;
        registry.SerializeComponents(entity, output);

        return output;
    }
}

TEST_SUITE("Component Serialization")
{
    TEST_CASE("Transform round-trip preserves values")
    {
        toml::table input;
        input.insert_or_assign("position", toml::array{1.0, 2.0, 3.0});
        input.insert_or_assign("rotation", toml::array{10.0, 20.0, 30.0});
        input.insert_or_assign("scale", toml::array{2.0, 2.0, 2.0});

        auto output = RoundTrip("transform", input);

        REQUIRE(output.contains("transform"));
        const auto* xform = output["transform"].as_table();
        REQUIRE(xform);

        const auto* pos = (*xform)["position"].as_array();
        REQUIRE(pos);
        REQUIRE(pos->size() == 3);
        CHECK(pos->get(0)->value_or(0.0) == doctest::Approx(1.0));
        CHECK(pos->get(1)->value_or(0.0) == doctest::Approx(2.0));
        CHECK(pos->get(2)->value_or(0.0) == doctest::Approx(3.0));

        const auto* scale = (*xform)["scale"].as_array();
        REQUIRE(scale);
        CHECK(scale->get(0)->value_or(0.0) == doctest::Approx(2.0));

        const auto* rot = (*xform)["rotation"].as_array();
        REQUIRE(rot);
        REQUIRE(rot->size() == 3);
        CHECK(rot->get(0)->value_or(0.0) == doctest::Approx(10.0));
        CHECK(rot->get(1)->value_or(0.0) == doctest::Approx(20.0));
        CHECK(rot->get(2)->value_or(0.0) == doctest::Approx(30.0));
    }

    TEST_CASE("Transform default values round-trip")
    {
        toml::table input; // No explicit values — should use defaults

        auto output = RoundTrip("transform", input);

        // Even with empty input, the component should be serialised with default values
        REQUIRE(output.contains("transform"));
        const auto* xform = output["transform"].as_table();
        REQUIRE(xform);

        const auto* pos = (*xform)["position"].as_array();
        REQUIRE(pos);
        CHECK(pos->get(0)->value_or(999.0) == doctest::Approx(0.0));
        CHECK(pos->get(1)->value_or(999.0) == doctest::Approx(0.0));
        CHECK(pos->get(2)->value_or(999.0) == doctest::Approx(0.0));
    }

    TEST_CASE("Mesh round-trip preserves values")
    {
        toml::table input;
        input.insert_or_assign("primitive", "cube");
        input.insert_or_assign("dimensions", toml::array{2.0, 3.0, 4.0});

        auto output = RoundTrip("mesh", input);

        REQUIRE(output.contains("mesh"));
        const auto* mesh = output["mesh"].as_table();
        REQUIRE(mesh);

        CHECK((*mesh)["primitive"].value_or("") == std::string("cube"));

        const auto* dims = (*mesh)["dimensions"].as_array();
        REQUIRE(dims);
        CHECK(dims->get(0)->value_or(0.0) == doctest::Approx(2.0));
    }

    TEST_CASE("Camera round-trip preserves values")
    {
        toml::table input;
        input.insert_or_assign("primary", true);
        input.insert_or_assign("fov", 90.0);
        input.insert_or_assign("projection", "orthographic");
        input.insert_or_assign("target", toml::array{1.0, 2.0, 3.0});
        input.insert_or_assign("up", toml::array{0.0, 1.0, 0.0});

        auto output = RoundTrip("camera", input);

        REQUIRE(output.contains("camera"));
        const auto* cam = output["camera"].as_table();
        REQUIRE(cam);

        CHECK((*cam)["primary"].value_or(false) == true);
        CHECK((*cam)["projection"].value_or("") == std::string("orthographic"));
    }

    TEST_CASE("Light round-trip preserves values")
    {
        toml::table input;
        input.insert_or_assign("type", "directional");
        input.insert_or_assign("intensity", 2.5);
        input.insert_or_assign("range", 15.0);

        auto output = RoundTrip("light", input);

        REQUIRE(output.contains("light"));
        const auto* light = output["light"].as_table();
        REQUIRE(light);

        CHECK((*light)["type"].value_or("") == std::string("directional"));
        CHECK((*light)["intensity"].value_or(0.0) == doctest::Approx(2.5));
    }

    TEST_CASE("Renderable round-trip preserves values")
    {
        toml::table input;
        input.insert_or_assign("visible", false);
        input.insert_or_assign("sort_priority", static_cast<int64_t>(200));

        auto output = RoundTrip("renderable", input);

        REQUIRE(output.contains("renderable"));
        const auto* renderable = output["renderable"].as_table();
        REQUIRE(renderable);

        CHECK((*renderable)["visible"].value_or(true) == false);
        CHECK((*renderable)["sort_priority"].value_or(static_cast<int64_t>(0)) == 200);
    }
}
