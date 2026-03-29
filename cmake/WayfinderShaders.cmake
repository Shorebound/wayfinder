# WayfinderShaders.cmake — compile Slang shaders to SPIR-V at build time using slangc
#
# Usage:
#   wayfinder_compile_shaders(
#       TARGET <cmake-target>
#       PROGRAMS <list of .slang program files>
#       MODULES <list of .slang module files imported by programs>
#       MODULE_DIR <include path for imports (e.g. engine/wayfinder/shaders)>
#       OUTPUT_DIR <dir>
#   )
#
# Requires SLANGC_EXECUTABLE (set by WayfinderSlang.cmake).

function(wayfinder_compile_shaders)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "TARGET;OUTPUT_DIR;MODULE_DIR" "PROGRAMS;MODULES")

    if(NOT SLANGC_EXECUTABLE)
        message(FATAL_ERROR "SLANGC_EXECUTABLE not set — include WayfinderSlang.cmake in the top-level CMakeLists")
    endif()
    if(NOT ARG_MODULE_DIR)
        message(FATAL_ERROR "wayfinder_compile_shaders: MODULE_DIR is required")
    endif()

    set(STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders")
    file(REMOVE_RECURSE "${STAGING_DIR}")
    file(MAKE_DIRECTORY "${STAGING_DIR}")

    set(_ALL_MODULE_DEPS ${ARG_MODULES})

    foreach(PROGRAM_SOURCE ${ARG_PROGRAMS})
        get_filename_component(PROGRAM_STEM "${PROGRAM_SOURCE}" NAME_WE)
        set(VERT_SPV "${STAGING_DIR}/${PROGRAM_STEM}.vert.spv")
        set(FRAG_SPV "${STAGING_DIR}/${PROGRAM_STEM}.frag.spv")

        # One slangc invocation per entry point (current slangc rejects multiple -o for -target spirv).
        # -fvk-use-entrypoint-name: keep VSMain/PSMain in SPIR-V (Slang defaults to "main").
        add_custom_command(
            OUTPUT ${VERT_SPV} ${FRAG_SPV}
            COMMAND ${SLANGC_EXECUTABLE} "${PROGRAM_SOURCE}"
                -target spirv
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                -stage vertex
                -entry VSMain
                -o "${VERT_SPV}"
                -I "${ARG_MODULE_DIR}"
            COMMAND ${SLANGC_EXECUTABLE} "${PROGRAM_SOURCE}"
                -target spirv
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                -stage fragment
                -entry PSMain
                -o "${FRAG_SPV}"
                -I "${ARG_MODULE_DIR}"
            DEPENDS ${PROGRAM_SOURCE} ${_ALL_MODULE_DEPS}
            COMMENT "Compiling Slang ${PROGRAM_STEM} -> SPIR-V"
            VERBATIM
        )
        list(APPEND SPV_OUTPUTS ${VERT_SPV} ${FRAG_SPV})
    endforeach()

    if(SPV_OUTPUTS)
        add_custom_target(${ARG_TARGET}_shaders ALL DEPENDS ${SPV_OUTPUTS})
        add_dependencies(${ARG_TARGET} ${ARG_TARGET}_shaders)

        add_custom_command(TARGET ${ARG_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${STAGING_DIR}"
                "${ARG_OUTPUT_DIR}"
            COMMENT "Copying compiled shaders to output directory"
            VERBATIM
        )
    endif()
endfunction()
