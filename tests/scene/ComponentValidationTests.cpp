#include "scene/RuntimeComponentRegistry.h"
#include "TestHelpers.h"

#include <doctest/doctest.h>

#include <string>
#include <toml++/toml.hpp>

using namespace Wayfinder;
using TestHelpers::MakeTestRegistry;

TEST_SUITE("Component Validation")
{
    // ── Transform ───────────────────────────────────────────

    TEST_CASE("Transform validates correctly with valid data")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("position", toml::array{1.0, 2.0, 3.0});
        input.insert_or_assign("rotation", toml::array{0.0, 0.0, 0.0});
        input.insert_or_assign("scale", toml::array{1.0, 1.0, 1.0});

        std::string error;
        CHECK(registry.ValidateComponent("transform", input, error));
        CHECK(error.empty());
    }

    TEST_CASE("Transform rejects non-array position")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("position", "not an array");

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("transform", input, error));
        CHECK_FALSE(error.empty());
    }

    TEST_CASE("Transform rejects position with wrong element count")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("position", toml::array{1.0, 2.0}); // only 2 elements

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("transform", input, error));
    }

    // ── Mesh ────────────────────────────────────────────────

    TEST_CASE("Mesh validates correctly with valid data")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("primitive", "cube");
        input.insert_or_assign("dimensions", toml::array{1.0, 1.0, 1.0});

        std::string error;
        CHECK(registry.ValidateComponent("mesh", input, error));
    }

    TEST_CASE("Mesh rejects unknown primitive type")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("primitive", "sphere"); // not a valid enum value

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("mesh", input, error));
    }

    // ── Camera ──────────────────────────────────────────────

    TEST_CASE("Camera validates correctly with valid data")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("primary", true);
        input.insert_or_assign("projection", "perspective");
        input.insert_or_assign("fov", 45.0);

        std::string error;
        CHECK(registry.ValidateComponent("camera", input, error));
    }

    TEST_CASE("Camera rejects non-boolean primary field")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("primary", "yes"); // should be bool

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("camera", input, error));
    }

    TEST_CASE("Camera rejects invalid projection mode")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("projection", "isometric"); // not a valid value

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("camera", input, error));
    }

    // ── Light ───────────────────────────────────────────────

    TEST_CASE("Light validates correctly with valid data")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("type", "point");
        input.insert_or_assign("intensity", 1.0);
        input.insert_or_assign("range", 10.0);

        std::string error;
        CHECK(registry.ValidateComponent("light", input, error));
    }

    TEST_CASE("Light rejects invalid light type")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("type", "area"); // not valid

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("light", input, error));
    }

    TEST_CASE("Light rejects non-numeric intensity")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("intensity", "bright"); // should be numeric

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("light", input, error));
    }

    // ── Material ────────────────────────────────────────────

    TEST_CASE("Material validates correctly with valid data")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("wireframe", true);

        std::string error;
        CHECK(registry.ValidateComponent("material", input, error));
    }

    TEST_CASE("Material rejects non-boolean wireframe")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("wireframe", "yes");

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("material", input, error));
    }

    // ── Renderable ──────────────────────────────────────────

    TEST_CASE("Renderable validates correctly with valid data")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("visible", true);
        input.insert_or_assign("sort_priority", static_cast<int64_t>(128));

        std::string error;
        CHECK(registry.ValidateComponent("renderable", input, error));
    }

    TEST_CASE("Renderable rejects non-boolean visible")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("visible", "true"); // should be bool

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("renderable", input, error));
    }

    TEST_CASE("Renderable rejects non-integer sort_priority")
    {
        auto registry = MakeTestRegistry();
        toml::table input;
        input.insert_or_assign("sort_priority", 128.5); // should be integer

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("renderable", input, error));
    }

    // ── Unknown Component ───────────────────────────────────

    TEST_CASE("Unknown component key is rejected")
    {
        auto registry = MakeTestRegistry();
        toml::table input;

        std::string error;
        CHECK_FALSE(registry.ValidateComponent("unknown_component", input, error));
        CHECK_FALSE(error.empty());
    }

    // ── RuntimeComponentRegistry ────────────────────────────

    TEST_CASE("IsRegistered returns true for core components")
    {
        auto registry = MakeTestRegistry();

        CHECK(registry.IsRegistered("transform"));
        CHECK(registry.IsRegistered("mesh"));
        CHECK(registry.IsRegistered("camera"));
        CHECK(registry.IsRegistered("light"));
        CHECK(registry.IsRegistered("material"));
        CHECK(registry.IsRegistered("renderable"));
    }

    TEST_CASE("IsRegistered returns false for unknown components")
    {
        auto registry = MakeTestRegistry();
        CHECK_FALSE(registry.IsRegistered("nonexistent"));
    }

    TEST_CASE("Empty table validates for optional-only components")
    {
        auto registry = MakeTestRegistry();
        toml::table emptyInput;

        std::string error;
        // Transform has all optional fields, so empty should be valid
        CHECK(registry.ValidateComponent("transform", emptyInput, error));
    }
}
