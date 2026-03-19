# cmake/WayfinderDependencies.cmake
include(GetCPM)

message(STATUS "Configuring dependencies...")

# --- SDL3 ---
# Uses GIT_REPOSITORY because SDL3 uses branch-style release tags
CPMAddPackage(
    NAME SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.30
    OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
)

# --- GLM ---
CPMAddPackage(
    NAME glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

# --- spdlog ---
CPMAddPackage("gh:gabime/spdlog@1.15.2")

# --- tomlplusplus ---
CPMAddPackage("gh:marzer/tomlplusplus@3.4.0")

# --- nlohmann/json ---
CPMAddPackage("gh:nlohmann/json@3.12.0")

# --- Tracy ---
CPMAddPackage("gh:wolfpld/tracy@0.11.1")

# --- Box2D ---
CPMAddPackage(
    NAME box2d
    GITHUB_REPOSITORY erincatto/box2d
    VERSION 3.1.0
    OPTIONS
        "BOX2D_BUILD_TESTBED OFF"
)

# --- Jolt Physics ---
CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY jrouwe/JoltPhysics
    VERSION 5.5.0
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "DOUBLE_PRECISION OFF"
        "TARGET_UNIT_TESTS OFF"
        "TARGET_SAMPLES OFF"
)

# --- Flecs ---
CPMAddPackage(
    NAME flecs
    GITHUB_REPOSITORY SanderMertens/flecs
    VERSION 4.1.5
    OPTIONS
        "FLECS_SHARED OFF"
        "FLECS_STATIC ON"
        "FLECS_CPP ON"
)

# --- Dear ImGui ---
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    VERSION 1.91.9b
)

# --- doctest (test framework, header-only) ---
if(WAYFINDER_BUILD_TESTS)
    CPMAddPackage(
        NAME doctest
        GITHUB_REPOSITORY doctest/doctest
        VERSION 2.4.11
        DOWNLOAD_ONLY YES
    )
    if(doctest_ADDED)
        add_library(doctest_with_main INTERFACE)
        target_include_directories(doctest_with_main INTERFACE "${doctest_SOURCE_DIR}")
        target_compile_definitions(doctest_with_main INTERFACE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
        add_library(doctest::doctest_with_main ALIAS doctest_with_main)
    endif()
endif()

message(STATUS "Dependency configuration complete.")
