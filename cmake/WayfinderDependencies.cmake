# cmake/WayfinderDependencies.cmake
include(GetCPM)

message(STATUS "Configuring dependencies...")

# --- SDL3 ---
CPMAddPackage(
    NAME SDL3
    GITHUB_REPOSITORY Shorebound/sdl
    GIT_TAG main
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
)

# --- SDL3_image ---
CPMAddPackage(
    NAME SDL3_image
    GITHUB_REPOSITORY Shorebound/sdl-image
    GIT_TAG main
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "SDLIMAGE_SAMPLES OFF"
        "SDLIMAGE_TESTS OFF"
        "SDLIMAGE_VENDORED ON"
        "SDLIMAGE_DEPS_SHARED OFF"
        "SDLIMAGE_AVIF OFF"
        "SDLIMAGE_JXL OFF"
        "SDLIMAGE_TIF OFF"
        "SDLIMAGE_WEBP OFF"
        "SDLIMAGE_INSTALL OFF"
)
unset(BUILD_SHARED_LIBS CACHE)

# --- GLM ---
CPMAddPackage(
    NAME glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
)

# --- spdlog ---
set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "Use std::format instead of bundled fmt" FORCE)
CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.17.0
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
)

# --- tomlplusplus ---
CPMAddPackage(
    NAME tomlplusplus
    GITHUB_REPOSITORY marzer/tomlplusplus
    VERSION 3.4.0
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
)

# --- nlohmann/json ---
CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.12.0
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
)

# --- Tracy ---
if(WAYFINDER_ENABLE_PROFILING)
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
else()
    set(TRACY_ENABLE OFF CACHE BOOL "" FORCE)
endif()

CPMAddPackage(
    NAME tracy
    GITHUB_REPOSITORY wolfpld/tracy
    VERSION 0.13.1
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
)

# --- Box2D ---
CPMAddPackage(
    NAME box2d
    GITHUB_REPOSITORY erincatto/box2d
    VERSION 3.1.1
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BOX2D_BUILD_TESTBED OFF"
)

# --- Jolt Physics ---
CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY Shorebound/jolt
    GIT_TAG master
    SOURCE_SUBDIR "Build"
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "DOUBLE_PRECISION OFF"
        "TARGET_UNIT_TESTS OFF"
        "TARGET_SAMPLES OFF"
        "USE_STATIC_MSVC_RUNTIME_LIBRARY OFF"
        "OVERRIDE_CXX_FLAGS OFF"
        "INTERPROCEDURAL_OPTIMIZATION OFF"
)

# MSVC C5045: informational note about Spectre mitigation inserts. It is often promoted to an
# error (C2220) when warnings-as-errors or strict security switches are in play. Jolt hits this
# in several translation units; suppress only for the third-party target.
if(MSVC)
    foreach(_wayfinder_jolt_target IN ITEMS Jolt JoltPhysics)
        if(TARGET ${_wayfinder_jolt_target})
            target_compile_options(${_wayfinder_jolt_target} PRIVATE /wd5045)
        endif()
    endforeach()
endif()

# --- Flecs ---
CPMAddPackage(
    NAME flecs
    GITHUB_REPOSITORY Shorebound/flecs
    GIT_TAG master
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
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
    SYSTEM TRUE
    EXCLUDE_FROM_ALL YES
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
    target_include_directories(imgui SYSTEM PUBLIC
        "${imgui_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}/backends"
    )
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif()

# --- Tool-only dependencies ---
if(WAYFINDER_BUILD_TOOLS)
    # --- fastgltf (glTF import — Waypoint tool only) ---
    CPMAddPackage(
        NAME fastgltf
        GITHUB_REPOSITORY spnda/fastgltf
        GIT_TAG v0.8.0
        SYSTEM TRUE
        EXCLUDE_FROM_ALL YES
        OPTIONS
            "FASTGLTF_ENABLE_DEPRECATED OFF"
    )

    # --- meshoptimizer (mesh post-processing — Waypoint tool only) ---
    CPMAddPackage(
        NAME meshoptimizer
        GITHUB_REPOSITORY zeux/meshoptimizer
        GIT_TAG v0.22
        SYSTEM TRUE
        EXCLUDE_FROM_ALL YES
    )

    # --- MikkTSpace (tangent generation — Waypoint tool only) ---
    CPMAddPackage(
        NAME MikkTSpace
        GITHUB_REPOSITORY mmikk/MikkTSpace
        GIT_TAG master
        DOWNLOAD_ONLY YES
        SYSTEM TRUE
        EXCLUDE_FROM_ALL YES
    )
    if(MikkTSpace_ADDED)
        add_library(mikktspace STATIC "${MikkTSpace_SOURCE_DIR}/mikktspace.c")
        target_include_directories(mikktspace SYSTEM PUBLIC "${MikkTSpace_SOURCE_DIR}")
        # MikkTSpace is C code — suppress C++ compiler warnings
        set_target_properties(mikktspace PROPERTIES LINKER_LANGUAGE C)
    endif()
endif()

# --- doctest (test framework, header-only) ---
if(WAYFINDER_BUILD_TESTS)
    CPMAddPackage(
        NAME doctest
        GITHUB_REPOSITORY doctest/doctest
        VERSION 2.4.11
        DOWNLOAD_ONLY YES
        SYSTEM TRUE
        EXCLUDE_FROM_ALL YES
    )
    if(doctest_ADDED)
        # Header-only interface (no implementation — use in all test TUs)
        add_library(doctest_headers INTERFACE)
        target_include_directories(doctest_headers SYSTEM INTERFACE "${doctest_SOURCE_DIR}")

        # doctest macros (TEST_CASE etc.) expand __COUNTER__, which Clang ≥ 19
        # flags as a C2y extension under -Wpedantic.  Suppress for consumers.
        # Only add if the compiler actually supports it (Clang < 19 doesn't).
        include(CheckCompilerFlag)
        check_compiler_flag(CXX "-Wno-c2y-extensions" WAYFINDER_HAS_WNO_C2Y_EXTENSIONS)
        if(WAYFINDER_HAS_WNO_C2Y_EXTENSIONS)
            target_compile_options(doctest_headers INTERFACE -Wno-c2y-extensions)
        endif()

        add_library(doctest::doctest ALIAS doctest_headers)

        # Single-TU implementation with main() — link to one test executable
        add_library(doctest_with_main STATIC "${CMAKE_CURRENT_LIST_DIR}/doctest_main.cpp")
        target_link_libraries(doctest_with_main PUBLIC doctest_headers)
        target_compile_definitions(doctest_with_main PRIVATE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
        add_library(doctest::doctest_with_main ALIAS doctest_with_main)
    endif()
endif()

message(STATUS "Dependency configuration complete.")
