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

    # Clean the staging dir at configure time so stale .spv files from
    # removed/renamed shaders don't persist across rebuilds.
    set(STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders")
    file(REMOVE_RECURSE "${STAGING_DIR}")
    file(MAKE_DIRECTORY "${STAGING_DIR}")

    # ── Module precompilation ──────────────────────────────────────
    # Compile .slang modules to .slang-module IR binaries so program
    # compilation skips re-parsing module source on every invocation.
    set(MODULE_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/precompiled_modules")
    file(REMOVE_RECURSE "${MODULE_CACHE_DIR}")
    file(MAKE_DIRECTORY "${MODULE_CACHE_DIR}/modules")

    set(_PRECOMPILED_MODULES "")
    foreach(MODULE_SOURCE ${ARG_MODULES})
        get_filename_component(MODULE_STEM "${MODULE_SOURCE}" NAME_WE)
        set(MODULE_BIN "${MODULE_CACHE_DIR}/modules/${MODULE_STEM}.slang-module")

        add_custom_command(
            OUTPUT "${MODULE_BIN}"
            COMMAND ${SLANGC_EXECUTABLE} "${MODULE_SOURCE}"
                -I "${ARG_MODULE_DIR}"
                -o "${MODULE_BIN}"
            DEPENDS "${MODULE_SOURCE}"
            COMMENT "Precompiling Slang module ${MODULE_STEM}"
            VERBATIM
        )
        list(APPEND _PRECOMPILED_MODULES "${MODULE_BIN}")
    endforeach()

    # ── Program compilation ────────────────────────────────────────
    # -I MODULE_CACHE_DIR first so slangc picks up .slang-module over .slang source.
    # -fvk-use-entrypoint-name: keep VSMain/PSMain in SPIR-V (Slang defaults to "main").
    set(SPV_OUTPUTS "")
    foreach(PROGRAM_SOURCE ${ARG_PROGRAMS})
        get_filename_component(PROGRAM_STEM "${PROGRAM_SOURCE}" NAME_WE)
        set(VERT_SPV "${STAGING_DIR}/${PROGRAM_STEM}.vert.spv")
        set(FRAG_SPV "${STAGING_DIR}/${PROGRAM_STEM}.frag.spv")

        add_custom_command(
            OUTPUT ${VERT_SPV} ${FRAG_SPV}
            COMMAND ${SLANGC_EXECUTABLE} "${PROGRAM_SOURCE}"
                -target spirv
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                -stage vertex
                -entry VSMain
                -o "${VERT_SPV}"
                -I "${MODULE_CACHE_DIR}"
                -I "${ARG_MODULE_DIR}"
            COMMAND ${SLANGC_EXECUTABLE} "${PROGRAM_SOURCE}"
                -target spirv
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                -stage fragment
                -entry PSMain
                -o "${FRAG_SPV}"
                -I "${MODULE_CACHE_DIR}"
                -I "${ARG_MODULE_DIR}"
            DEPENDS ${PROGRAM_SOURCE} ${_PRECOMPILED_MODULES}
            COMMENT "Compiling Slang ${PROGRAM_STEM} -> SPIR-V"
            VERBATIM
        )
        list(APPEND SPV_OUTPUTS ${VERT_SPV} ${FRAG_SPV})
    endforeach()

    if(SPV_OUTPUTS)
        set(_SHADER_STAMP "${CMAKE_CURRENT_BINARY_DIR}/shader_sync.stamp")
        add_custom_command(
            OUTPUT "${_SHADER_STAMP}"
            DEPENDS ${SPV_OUTPUTS}
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${ARG_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${ARG_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${STAGING_DIR}" "${ARG_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E touch "${_SHADER_STAMP}"
            COMMENT "Syncing compiled shaders to output directory"
            VERBATIM
        )
        add_custom_target(${ARG_TARGET}_shaders ALL DEPENDS "${_SHADER_STAMP}")
        add_dependencies(${ARG_TARGET} ${ARG_TARGET}_shaders)
    endif()
endfunction()
