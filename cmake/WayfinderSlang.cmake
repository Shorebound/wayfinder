# cmake/WayfinderSlang.cmake — Download prebuilt Slang SDK (slangc) via FetchContent
#
# Optional: set SLANG_SDK_CACHE_DIR to redirect the download to a stable
# directory outside the build tree (useful for CI caching).
include(FetchContent)

set(SLANG_VERSION "2026.5.1" CACHE STRING "Slang SDK version")

if(WIN32)
    set(_SLANG_PLATFORM "windows-x86_64")
    set(_SLANG_EXT "zip")
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_SLANG_PLATFORM "macos-aarch64")
    else()
        set(_SLANG_PLATFORM "macos-x86_64")
    endif()
    set(_SLANG_EXT "tar.gz")
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(_SLANG_PLATFORM "linux-aarch64")
    else()
        set(_SLANG_PLATFORM "linux-x86_64")
    endif()
    set(_SLANG_EXT "tar.gz")
endif()

set(_SLANG_FILENAME "slang-${SLANG_VERSION}-${_SLANG_PLATFORM}.${_SLANG_EXT}")
set(_SLANG_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${_SLANG_FILENAME}")

message(STATUS "Fetching Slang SDK ${SLANG_VERSION} for ${_SLANG_PLATFORM}")

# Redirect FetchContent to a cacheable directory when SLANG_SDK_CACHE_DIR is set.
if(SLANG_SDK_CACHE_DIR)
    set(_SLANG_PREV_FC_BASE_DIR "${FETCHCONTENT_BASE_DIR}")
    set(FETCHCONTENT_BASE_DIR "${SLANG_SDK_CACHE_DIR}")
endif()

FetchContent_Declare(slang_sdk
    URL      "${_SLANG_URL}"
    URL_HASH SHA256=4bbf0f1338f13cb0baf2cc10da62ea92d186c9ea3219c73d6d3516bf1bf3cd49
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(slang_sdk)

if(SLANG_SDK_CACHE_DIR)
    set(FETCHCONTENT_BASE_DIR "${_SLANG_PREV_FC_BASE_DIR}")
endif()

if(WIN32)
    set(SLANGC_EXECUTABLE "${slang_sdk_SOURCE_DIR}/bin/slangc.exe" CACHE FILEPATH "Path to slangc" FORCE)
else()
    set(SLANGC_EXECUTABLE "${slang_sdk_SOURCE_DIR}/bin/slangc" CACHE FILEPATH "Path to slangc" FORCE)
endif()

set(SLANG_INCLUDE_DIR "${slang_sdk_SOURCE_DIR}/include" CACHE PATH "Slang headers" FORCE)
set(SLANG_LIB_DIR "${slang_sdk_SOURCE_DIR}/lib" CACHE PATH "Slang libraries" FORCE)

if(NOT EXISTS "${SLANGC_EXECUTABLE}")
    message(FATAL_ERROR "slangc not found at ${SLANGC_EXECUTABLE} — check SLANG_VERSION and platform")
endif()
message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")
