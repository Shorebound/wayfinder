# cmake/Dependencies.cmake
include(FetchContent)

message(STATUS "Configuring dependencies...")

# --- Raylib ---
set(RAYLIB_VERSION 5.5) 
find_package(raylib ${RAYLIB_VERSION} QUIET)
if (NOT raylib_FOUND)
  message(STATUS "Dependency: raylib not found locally, fetching...")
  FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED)
    set(FETCHCONTENT_QUIET NO)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
    FetchContent_MakeAvailable(raylib)
    message(STATUS "Dependency: raylib fetched and configured.")
  endif()
else()
    message(STATUS "Dependency: Found raylib ${raylib_VERSION}")
endif()

# --- spdlog ---
set(SPDLOG_VERSION 1.15.2) 
find_package(spdlog ${SPDLOG_VERSION} QUIET)
if (NOT spdlog_FOUND)
    message(STATUS "Dependency: spdlog not found locally, fetching...")
    FetchContent_Declare(
        spdlog
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/gabime/spdlog/archive/refs/tags/v${SPDLOG_VERSION}.tar.gz
    )
    # spdlog is header-only by default, but MakeAvailable creates target
    FetchContent_MakeAvailable(spdlog)
    message(STATUS "Dependency: spdlog fetched and configured.")
else()
    message(STATUS "Dependency: Found spdlog ${spdlog_VERSION}")
endif()

# --- toml ---
set(TOMLPLUSPLUS_VERSION 3.4.0)
find_package(tomlplusplus ${TOMLPLUSPLUS_VERSION} QUIET)
if (NOT tomlplusplus_FOUND)
    message(STATUS "Dependency: tomlplusplus not found locally, fetching...")
    FetchContent_Declare(
        tomlplusplus
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v${TOMLPLUSPLUS_VERSION}.tar.gz
    )
    FetchContent_MakeAvailable(tomlplusplus)
    message(STATUS "Dependency: tomlplusplus fetched and configured.")
else()
    message(STATUS "Dependency: Found tomlplusplus ${tomlplusplus_VERSION}")
endif()

# --- json ---
set(JSON_VERSION 3.12.0)
find_package(json ${JSON_VERSION} QUIET)
if (NOT json_FOUND)
    message(STATUS "Dependency: json not found locally, fetching...")
    FetchContent_Declare(
        json
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/nlohmann/json/archive/refs/tags/v${JSON_VERSION}.tar.gz
    )
    FetchContent_MakeAvailable(json)
    message(STATUS "Dependency: json fetched and configured.")
else()
    message(STATUS "Dependency: Found json ${json_VERSION}")
endif()

# --- tracy ---
set(TRACY_VERSION 0.11.1)
find_package(tracy ${TRACY_VERSION} QUIET)
if (NOT tracy_FOUND)
    message(STATUS "Dependency: tracy not found locally, fetching...")
    FetchContent_Declare(
        tracy
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/wolfpld/tracy/archive/refs/tags/v${TRACY_VERSION}.tar.gz
    )
    FetchContent_MakeAvailable(tracy)
    message(STATUS "Dependency: tracy fetched and configured.")
else()
    message(STATUS "Dependency: Found tracy ${tracy_VERSION}")
endif()


# --- Box2D ---
set(BOX2D_VERSION 3.1.0)
find_package(box2d ${BOX2D_VERSION} QUIET)
if (NOT box2d_FOUND)
    message(STATUS "Dependency: box2d not found locally, fetching...")
    FetchContent_Declare(
        box2d
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/erincatto/box2d/archive/refs/tags/v${BOX2D_VERSION}.tar.gz
    )
    # Configure Box2D build options
    set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "Build Box2D testbed" FORCE)
    FetchContent_MakeAvailable(box2d)
    message(STATUS "Dependency: box2d fetched and configured.")
else()
    message(STATUS "Dependency: Found box2d ${box2d_VERSION}")
endif()

# --- Jolt Physics ---
set(JOLT_PHYSICS_VERSION 5.5.0)
find_package(JoltPhysics ${JOLT_PHYSICS_VERSION} QUIET)
if (NOT JoltPhysics_FOUND)
    message(STATUS "Dependency: JoltPhysics not found locally, fetching...")
    FetchContent_Declare(
        JoltPhysics
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v${JOLT_PHYSICS_VERSION}.tar.gz
    )
    # Configure Jolt build options
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Jolt as shared library" FORCE)
    set(DOUBLE_PRECISION OFF CACHE BOOL "Use double precision" FORCE)
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "Build unit tests" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "Build samples" FORCE)
    FetchContent_MakeAvailable(JoltPhysics)
    message(STATUS "Dependency: JoltPhysics fetched and configured.")
else()
    message(STATUS "Dependency: Found JoltPhysics ${JoltPhysics_VERSION}")
endif()

# --- SoLoud ---


# --- WWise ---


# --- Flecs ---
set(FLECS_VERSION 4.1.5)
find_package(flecs ${FLECS_VERSION} QUIET)
if (NOT flecs_FOUND)
    message(STATUS "Dependency: flecs not found locally, fetching...")
    FetchContent_Declare(
        flecs
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/SanderMertens/flecs/archive/refs/tags/v${FLECS_VERSION}.tar.gz
    )
    # Flecs build options
    set(FLECS_SHARED OFF CACHE BOOL "Build Flecs as shared library" FORCE)
    set(FLECS_STATIC ON CACHE BOOL "Build Flecs as static library" FORCE)
    set(FLECS_CPP ON CACHE BOOL "Build Flecs C++ API" FORCE)
    FetchContent_MakeAvailable(flecs)
    message(STATUS "Dependency: flecs fetched and configured.")
else()
    message(STATUS "Dependency: Found flecs ${flecs_VERSION}")
endif()

# --- Dear ImGui ---
set(IMGUI_VERSION "1.91.9b")

find_package(imgui 1.91.9 QUIET)
if (NOT imgui_FOUND)
    message(STATUS "Dependency: imgui not found locally, fetching...")
    FetchContent_Declare(
        imgui
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/ocornut/imgui/archive/refs/tags/v${IMGUI_VERSION}.tar.gz
    )
    FetchContent_GetProperties(imgui)
    message(STATUS "Dependency: imgui fetched and configured.")
else()
    message(STATUS "Dependency: Found imgui ${imgui_VERSION}")
endif()

message(STATUS "Dependency configuration complete.")
