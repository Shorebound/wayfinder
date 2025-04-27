cmake_minimum_required(VERSION 3.26.0)


project(wayfinder_project VERSION 0.1.0 LANGUAGES C CXX)

set(ENGINE_NAME wayfinder)
set(RUNTIME_NAME beacon)
set(EDITOR_NAME compass)
set(SANDBOX_NAME waystone)
set(TOOLS_NAME navigator)
set(ASSET_PROCESSOR_NAME cartographer)
set(SAMPLES_NAME journey)
set(TESTS_NAME surveyor)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WAYFINDER_BUILD_EDITOR "Build the Wayfinder Editor" ON)
option(WAYFINDER_BUILD_RUNTIME "Build the Wayfinder Runtime" OFF)
option(WAYFINDER_BUILD_SANDBOX "Build the Wayfinder Sandbox" ON)
option(WAYFINDER_BUILD_TOOLS "Build standalone tools" OFF)
option(WAYFINDER_BUILD_SAMPLES "Build sample projects" OFF)
option(WAYFINDER_BUILD_TESTS "Build tests" OFF) 
option(WAYFINDER_BUILD_ASSETPROCESSOR "Build the Wayfinder Asset Processor" OFF)
option(WAYFINDER_BUILD_SHARED_LIBS "Build engine modules as shared libraries" ON)

message(STATUS "Wayfinder Project Configuration:")
message(STATUS "  Build Sandbox (Waystone): ${WAYFINDER_BUILD_SANDBOX}")
message(STATUS "  Build Editor (Compass): ${WAYFINDER_BUILD_EDITOR}")
message(STATUS "  Build Runtime (Beacon): ${WAYFINDER_BUILD_RUNTIME}")
message(STATUS "  Build Tools (Navigator): ${WAYFINDER_BUILD_TOOLS}")
message(STATUS "  Build Samples (Journey): ${WAYFINDER_BUILD_SAMPLES}")
message(STATUS "  Build Tests (Surveyor): ${WAYFINDER_BUILD_TESTS}")
message(STATUS "  Shared Libs: ${WAYFINDER_BUILD_SHARED_LIBS}")

add_subdirectory(${ENGINE_NAME})

if(WAYFINDER_BUILD_SANDBOX)
    add_subdirectory(${SANDBOX_NAME}) 
endif()

if(WAYFINDER_BUILD_RUNTIME)
    #add_subdirectory(${RUNTIME_NAME})
endif()

if(WAYFINDER_BUILD_EDITOR)
    #add_subdirectory(${EDITOR_NAME})
endif()

if(WAYFINDER_BUILD_RUNTIME)
    #add_subdirectory(${RUNTIME_NAME})
endif()

if(WAYFINDER_BUILD_TOOLS)
    #add_subdirectory(${TOOLS_NAME})
endif()

if(WAYFINDER_BUILD_ASSETPROCESSOR)
    #add_subdirectory(${ASSET_PROCESSOR_NAME})
endif()

if(WAYFINDER_BUILD_SAMPLES)
    #add_subdirectory(${SAMPLES_NAME})
endif()

if(WAYFINDER_BUILD_TESTS)
    #enable_testing()
    #add_subdirectory(${TESTS_NAME})
endif()

message(STATUS "Configuration complete.")