#include "TestHelpers.h"
#include "scene/Components.h"
#include "scene/RuntimeComponentRegistry.h"
#include "scene/Scene.h"
#include "scene/entity/Entity.h"

#include <doctest/doctest.h>

#include "ecs/Flecs.h"
#include <nlohmann/json.hpp>

namespace Wayfinder::Tests
{
    using Helpers::MakeTestRegistry;

    namespace
    {
        /// Helper: create a scene with a single entity, apply component data, then serialise.
        /// Returns the serialised JSON object containing component data.
        nlohmann::json RoundTrip(const std::string& componentKey, const nlohmann::json& inputData)
        {
            flecs::world world;
            auto registry = MakeTestRegistry();
            registry.RegisterComponents(world);
            Scene::RegisterCoreComponents(world);
            Scene scene(world, registry, "SerialiseTest");

            auto entity = scene.CreateEntity("TestEntity");

            // Apply components from the input data
            nlohmann::json componentTables = nlohmann::json::object();
            componentTables.emplace(componentKey, inputData);
            registry.ApplyComponents(componentTables, entity);

            // Serialise the entity's components back out
            nlohmann::json output = nlohmann::json::object();
            registry.SerialiseComponents(entity, output);

            return output;
        }
    } // namespace

    TEST_SUITE("Component Serialisation")
    {
        TEST_CASE("Transform round-trip preserves values")
        {
            const nlohmann::json input = {{"position", {1.0, 2.0, 3.0}}, {"rotation", {10.0, 20.0, 30.0}}, {"scale", {2.0, 2.0, 2.0}}};

            auto output = RoundTrip("transform", input);

            REQUIRE(output.contains("transform"));
            const auto& xform = output.at("transform");
            REQUIRE(xform.is_object());

            REQUIRE(xform.contains("position"));
            const auto& pos = xform.at("position");
            REQUIRE(pos.is_array());
            REQUIRE(pos.size() == 3);
            CHECK(pos.at(0).get<double>() == doctest::Approx(1.0));
            CHECK(pos.at(1).get<double>() == doctest::Approx(2.0));
            CHECK(pos.at(2).get<double>() == doctest::Approx(3.0));

            const auto& scale = xform.at("scale");
            REQUIRE(scale.is_array());
            CHECK(scale.at(0).get<double>() == doctest::Approx(2.0));

            const auto& rot = xform.at("rotation");
            REQUIRE(rot.is_array());
            REQUIRE(rot.size() == 3);
            CHECK(rot.at(0).get<double>() == doctest::Approx(10.0));
            CHECK(rot.at(1).get<double>() == doctest::Approx(20.0));
            CHECK(rot.at(2).get<double>() == doctest::Approx(30.0));
        }

        TEST_CASE("Transform default values round-trip")
        {
            const nlohmann::json input = nlohmann::json::object(); // No explicit values — should use defaults

            auto output = RoundTrip("transform", input);

            // Even with empty input, the component should be serialised with default values
            REQUIRE(output.contains("transform"));
            const auto& xform = output.at("transform");
            REQUIRE(xform.is_object());

            const auto& pos = xform.at("position");
            REQUIRE(pos.is_array());
            CHECK(pos.at(0).get<double>() == doctest::Approx(0.0));
            CHECK(pos.at(1).get<double>() == doctest::Approx(0.0));
            CHECK(pos.at(2).get<double>() == doctest::Approx(0.0));
        }

        TEST_CASE("Mesh round-trip preserves values")
        {
            const nlohmann::json input = {{"primitive", "cube"}, {"dimensions", {2.0, 3.0, 4.0}}};

            auto output = RoundTrip("mesh", input);

            REQUIRE(output.contains("mesh"));
            const auto& mesh = output.at("mesh");
            REQUIRE(mesh.is_object());

            CHECK(mesh.at("primitive").get<std::string>() == "cube");

            const auto& dims = mesh.at("dimensions");
            REQUIRE(dims.is_array());
            CHECK(dims.at(0).get<double>() == doctest::Approx(2.0));
        }

        TEST_CASE("Mesh round-trip preserves mesh_id")
        {
            const nlohmann::json input = {{"primitive", "cube"}, {"dimensions", {1.0, 1.0, 1.0}}, {"mesh_id", "a0000000-0000-0000-0000-000000000099"}};

            auto output = RoundTrip("mesh", input);

            REQUIRE(output.contains("mesh"));
            const auto& mesh = output.at("mesh");
            REQUIRE(mesh.contains("mesh_id"));
            CHECK(mesh.at("mesh_id").get<std::string>() == "a0000000-0000-0000-0000-000000000099");
        }

        TEST_CASE("Camera round-trip preserves values")
        {
            const nlohmann::json input = {{"primary", true}, {"fov", 90.0}, {"projection", "orthographic"}, {"target", {1.0, 2.0, 3.0}}, {"up", {0.0, 1.0, 0.0}}};

            auto output = RoundTrip("camera", input);

            REQUIRE(output.contains("camera"));
            const auto& cam = output.at("camera");
            REQUIRE(cam.is_object());

            CHECK(cam.at("primary").get<bool>() == true);
            CHECK(cam.at("projection").get<std::string>() == "orthographic");
        }

        TEST_CASE("Light round-trip preserves values")
        {
            const nlohmann::json input = {{"type", "directional"}, {"intensity", 2.5}, {"range", 15.0}};

            auto output = RoundTrip("light", input);

            REQUIRE(output.contains("light"));
            const auto& light = output.at("light");
            REQUIRE(light.is_object());

            CHECK(light.at("type").get<std::string>() == "directional");
            CHECK(light.at("intensity").get<double>() == doctest::Approx(2.5));
        }

        TEST_CASE("Renderable round-trip preserves values")
        {
            const nlohmann::json input = {{"visible", false}, {"sort_priority", 200}};

            auto output = RoundTrip("renderable", input);

            REQUIRE(output.contains("renderable"));
            const auto& renderable = output.at("renderable");
            REQUIRE(renderable.is_object());

            CHECK(renderable.at("visible").get<bool>() == false);
            CHECK(renderable.at("sort_priority").get<int64_t>() == 200);
        }
    }
}
