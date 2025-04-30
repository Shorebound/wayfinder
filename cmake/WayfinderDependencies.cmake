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

message(STATUS "Dependency configuration complete.")
