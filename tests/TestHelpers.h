#pragma once

#include "scene/RuntimeComponentRegistry.h"

#include <filesystem>

namespace Wayfinder::TestHelpers
{
    /// Returns the absolute path to the test fixtures directory.
    /// Works from any test file because the path is computed relative
    /// to this header's location (tests/TestHelpers.h → tests/fixtures/).
    inline std::filesystem::path FixturesDir()
    {
        return std::filesystem::path(__FILE__).parent_path() / "fixtures";
    }

    /// Creates a RuntimeComponentRegistry seeded with core entries.
    inline RuntimeComponentRegistry MakeTestRegistry()
    {
        RuntimeComponentRegistry registry;
        registry.AddCoreEntries();
        return registry;
    }

} // namespace Wayfinder::TestHelpers
