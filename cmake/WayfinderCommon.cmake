# cmake/WayfinderCommon.cmake

# Create an interface library to hold common settings
add_library(wayfinder_common INTERFACE)
message(STATUS "Configuring common Wayfinder settings (wayfinder_common)")

target_compile_features(wayfinder_common INTERFACE cxx_std_23)

# --- Common Compile Definitions ---
target_compile_definitions(wayfinder_common INTERFACE
    # Config-specific definitions
    $<$<CONFIG:Debug>:WAYFINDER_DEBUG>
    $<$<CONFIG:Development>:WAYFINDER_DEVELOPMENT>
    $<$<CONFIG:Shipping>:WAYFINDER_SHIPPING>

    # Config-specific logging levels (Assuming these are needed project-wide)
    # If only engine needs them, keep them there.
    $<$<CONFIG:Debug>:WAYFINDER_LOGGING_LEVEL=0>
    $<$<CONFIG:Development>:WAYFINDER_LOGGING_LEVEL=1>
    $<$<CONFIG:Shipping>:WAYFINDER_LOGGING_LEVEL=2>

    # Debug/Feature Flags (controlled by options)
    $<$<NOT:$<CONFIG:Shipping>>:WAYFINDER_ENABLE_ASSERTS>
    $<$<BOOL:${WAYFINDER_ENABLE_LOGGING}>:WAYFINDER_LOGGING_ENABLED>
    $<$<BOOL:${WAYFINDER_PHYSICS}>:WAYFINDER_PHYSICS>

    # Platform-specific (Public controls #ifdef)
    $<$<PLATFORM_ID:Windows>:WAYFINDER_PLATFORM_WINDOWS>
    $<$<PLATFORM_ID:Linux>:WAYFINDER_PLATFORM_LINUX>
    $<$<PLATFORM_ID:Darwin>:WAYFINDER_PLATFORM_MACOS>
    $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Emscripten>:WAYFINDER_PLATFORM_WEB>

    # Compiler-specific (Public controls #ifdef)
    $<$<CXX_COMPILER_ID:MSVC>:WAYFINDER_COMPILER_MSVC>
    $<$<CXX_COMPILER_ID:GNU>:WAYFINDER_COMPILER_GCC>
    $<$<CXX_COMPILER_ID:Clang>:WAYFINDER_COMPILER_CLANG>
    $<$<CXX_COMPILER_ID:AppleClang>:WAYFINDER_COMPILER_CLANG>
    $<$<CXX_COMPILER_ID:Intel>:WAYFINDER_COMPILER_INTEL>

    # Define NDEBUG for non-Debug builds (Common practice)
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

# --- Common Compile Options ---
target_compile_options(wayfinder_common INTERFACE
    # Common flags for GCC/Clang
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
    # Consider adding more specific warnings here if desired project-wide

    # Common flags for MSVC
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# Optional: Treat warnings as errors
if(WAYFINDER_WARNINGS_AS_ERRORS)
    target_compile_options(wayfinder_common INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
    )
    message(STATUS "Common: Treating compiler warnings as errors")
endif()

# RTTI Control
if(NOT WAYFINDER_ENABLE_RTTI)
    message(STATUS "Common: Disabling RTTI")
    target_compile_options(wayfinder_common INTERFACE
        $<$<CXX_COMPILER_ID:MSVC>:/GR->
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-rtti>
    )
endif()

# Exception Control
if(NOT WAYFINDER_ENABLE_EXCEPTIONS)
    message(STATUS "Common: Disabling Exceptions")
    target_compile_options(wayfinder_common INTERFACE
        $<$<CXX_COMPILER_ID:MSVC>:/EHs-c-> # Review MSVC docs for exact flags
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions>
    )
endif()
