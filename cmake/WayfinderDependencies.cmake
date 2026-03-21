# cmake/WayfinderDependencies.cmake
include(GetCPM)

message(STATUS "Configuring dependencies...")

# --- SDL3 ---
# Uses GIT_REPOSITORY because SDL3 uses branch-style release tags
CPMAddPackage(
    NAME SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.2
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
    GIT_TAG 1.0.3
)

# --- spdlog ---
set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "Use std::format instead of bundled fmt" FORCE)
CPMAddPackage("gh:gabime/spdlog@1.17.0")

# --- tomlplusplus ---
CPMAddPackage("gh:marzer/tomlplusplus@3.4.0")

# --- nlohmann/json ---
CPMAddPackage("gh:nlohmann/json@3.12.0")

# --- Tracy ---
CPMAddPackage("gh:wolfpld/tracy@0.13.1")

# --- Box2D ---
CPMAddPackage(
    NAME box2d
    GITHUB_REPOSITORY erincatto/box2d
    VERSION 3.1.1
    OPTIONS
        "BOX2D_BUILD_TESTBED OFF"
)

# --- Jolt Physics ---
CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY jrouwe/JoltPhysics
    VERSION 5.5.0
    SOURCE_SUBDIR "Build"
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "DOUBLE_PRECISION OFF"
        "TARGET_UNIT_TESTS OFF"
        "TARGET_SAMPLES OFF"
        "USE_STATIC_MSVC_RUNTIME_LIBRARY OFF"
        "OVERRIDE_CXX_FLAGS OFF"
        "INTERPROCEDURAL_OPTIMIZATION OFF"
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
    VERSION 1.92.6
    DOWNLOAD_ONLY YES
)

if(imgui_ADDED)
    # Build ImGui core + SDL3/SDL_GPU backend as a static library.
    add_library(imgui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/imgui_demo.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp"
    )
    target_include_directories(imgui PUBLIC
        "${imgui_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}/backends"
    )
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif()

# --- doctest (test framework, header-only) ---
if(WAYFINDER_BUILD_TESTS)
    CPMAddPackage(
        NAME doctest
        GITHUB_REPOSITORY doctest/doctest
        VERSION 2.4.11
        DOWNLOAD_ONLY YES
    )
    if(doctest_ADDED)
        # Header-only interface (no implementation — use in all test TUs)
        add_library(doctest_headers INTERFACE)
        target_include_directories(doctest_headers INTERFACE "${doctest_SOURCE_DIR}")
        add_library(doctest::doctest ALIAS doctest_headers)

        # Single-TU implementation with main() — link to one test executable
        add_library(doctest_with_main STATIC "${CMAKE_CURRENT_LIST_DIR}/doctest_main.cpp")
        target_link_libraries(doctest_with_main PUBLIC doctest_headers)
        target_compile_definitions(doctest_with_main PRIVATE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
        add_library(doctest::doctest_with_main ALIAS doctest_with_main)
    endif()
endif()

message(STATUS "Dependency configuration complete.")
