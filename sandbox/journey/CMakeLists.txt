# journey/CMakeLists.txt

# Define the executable target
add_executable(${SANDBOX_NAME})
wayfinder_set_output_directories(${SANDBOX_NAME})
message(STATUS "Sandbox: Configuring target ${SANDBOX_NAME}")

# --- Sources ---
target_sources(${SANDBOX_NAME} 
    PRIVATE
        src/JourneyGame.cpp 
)

# --- Include Directories ---
target_include_directories(${SANDBOX_NAME} 
    PRIVATE
        src
)

# --- Link Libraries ---
target_link_libraries(${SANDBOX_NAME} 
    PRIVATE
        ${ENGINE_NAME}
)

add_custom_command(TARGET ${SANDBOX_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:${SANDBOX_NAME}>/assets"
  COMMENT "Copying sandbox assets for ${SANDBOX_NAME}"
  VERBATIM
)

add_custom_command(TARGET ${SANDBOX_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/project.wayfinder"
    "$<TARGET_FILE_DIR:${SANDBOX_NAME}>/project.wayfinder"
  COMMENT "Copying project.wayfinder for ${SANDBOX_NAME}"
  VERBATIM
)

add_custom_command(TARGET ${SANDBOX_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/config"
    "$<TARGET_FILE_DIR:${SANDBOX_NAME}>/config"
  COMMENT "Copying config directory for ${SANDBOX_NAME}"
  VERBATIM
)

# --- Shader Compilation ---
include(WayfinderShaders)
set(_SHADER_ROOT "${CMAKE_SOURCE_DIR}/engine/wayfinder/shaders")
file(GLOB SHADER_PROGRAMS CONFIGURE_DEPENDS "${_SHADER_ROOT}/*.slang")
file(GLOB SHADER_MODULES CONFIGURE_DEPENDS "${_SHADER_ROOT}/modules/*.slang")
wayfinder_compile_shaders(
    TARGET ${SANDBOX_NAME}
    PROGRAMS ${SHADER_PROGRAMS}
    MODULES ${SHADER_MODULES}
    MODULE_DIR "${_SHADER_ROOT}"
    OUTPUT_DIR "$<TARGET_FILE_DIR:${SANDBOX_NAME}>/assets/shaders"
)

# --- Copy Slang Runtime DLLs (Development/Debug only) ---
# Only the DLLs needed for runtime compilation are copied; the full SDK
# contains gfx.dll, slang-llvm.dll, etc. that we don't use (~110 MB saved).
# The $<IF:$<CONFIG:Shipping>,true,copy_if_different> trick makes the command
# a no-op in Shipping (CMake's 'true' command succeeds without side effects).
if(WIN32 AND DEFINED SLANG_BIN_DIR AND EXISTS "${SLANG_BIN_DIR}")
    set(_SLANG_DLLS
        "${SLANG_BIN_DIR}/slang.dll"
        "${SLANG_BIN_DIR}/slang-rt.dll"
        "${SLANG_BIN_DIR}/slang-compiler.dll"
        "${SLANG_BIN_DIR}/slang-glslang.dll"
    )
    foreach(_DLL ${_SLANG_DLLS})
        if(EXISTS "${_DLL}")
            add_custom_command(TARGET ${SANDBOX_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Shipping>,true,copy_if_different>
                    "${_DLL}" "$<TARGET_FILE_DIR:${SANDBOX_NAME}>"
                COMMENT "Copying Slang DLL: ${_DLL}"
                VERBATIM
            )
        endif()
    endforeach()
endif()

# --- Platform Specifics for THIS Executable ---
# Web Configurations (using standard CMake variable)
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "Sandbox: Configuring ${SANDBOX_NAME} for Web (Emscripten)")
    set_target_properties(${SANDBOX_NAME} PROPERTIES SUFFIX ".html")
    # Use target_link_options (CMake >= 3.13) for cleaner linker flags
    target_link_options(${SANDBOX_NAME} PRIVATE
        "-s USE_GLFW=3"
        "-s ASSERTIONS=1"
        "-s WASM=1"
        "-s ASYNCIFY"
        "-s GL_ENABLE_GET_PROC_ADDRESS=1"
        # Add other Emscripten flags needed by the sandbox/game
    )
endif()

# Apple Framework Linking
if (APPLE)
    message(STATUS "Sandbox: Linking Apple Frameworks for ${SANDBOX_NAME}")
    target_link_libraries(${SANDBOX_NAME} PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
        # Add other frameworks needed by the sandbox/game
    )
endif()