set(WAYFINDER_RENDER_TEST_NAME wayfinder_render_tests)

add_executable(${WAYFINDER_RENDER_TEST_NAME}
  rendering/RenderOrchestratorTests.cpp
  rendering/RenderGraphTests.cpp
  volumes/BlendableEffectRegistryTests.cpp
  rendering/RenderFeatureTests.cpp
  rendering/SubmissionDrawingTests.cpp
  rendering/SceneOpaquePassTests.cpp
  rendering/PostProcessFeatureTests.cpp
  rendering/DebugPassTests.cpp
  rendering/RenderServicesTests.cpp
  rendering/BlendStateTests.cpp
  rendering/FrameAllocatorTests.cpp
  rendering/TextureManagerTests.cpp
  rendering/MeshManagerTests.cpp
  rendering/SlangCompilerTests.cpp
  rendering/SlangReflectionTests.cpp
)

wayfinder_set_output_directories(${WAYFINDER_RENDER_TEST_NAME})

target_link_libraries(${WAYFINDER_RENDER_TEST_NAME}
  PRIVATE
    ${ENGINE_NAME}
    doctest::doctest_with_main
    Slang::slang
)

target_include_directories(${WAYFINDER_RENDER_TEST_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME ${WAYFINDER_RENDER_TEST_NAME} COMMAND ${WAYFINDER_RENDER_TEST_NAME})
set_target_properties(${WAYFINDER_RENDER_TEST_NAME} PROPERTIES FOLDER "${TESTS_DIRECTORY}")

# Copy Slang runtime DLLs needed for runtime compilation next to render test executable
if(WIN32 AND DEFINED SLANG_BIN_DIR)
    set(_SLANG_TEST_DLLS
        "${SLANG_BIN_DIR}/slang.dll"
        "${SLANG_BIN_DIR}/slang-rt.dll"
        "${SLANG_BIN_DIR}/slang-compiler.dll"
        "${SLANG_BIN_DIR}/slang-glslang.dll"
    )
    foreach(_DLL ${_SLANG_TEST_DLLS})
        if(EXISTS "${_DLL}")
            add_custom_command(TARGET ${WAYFINDER_RENDER_TEST_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_DLL}" "$<TARGET_FILE_DIR:${WAYFINDER_RENDER_TEST_NAME}>"
                VERBATIM
            )
        endif()
    endforeach()
endif()

# ── Core Tests ────────────────────────────────────────────

set(WAYFINDER_CORE_TEST_NAME wayfinder_core_tests)

add_executable(${WAYFINDER_CORE_TEST_NAME}
  core/HandleTests.cpp
  core/InternedStringTests.cpp
  core/EventQueueTests.cpp
  core/IdentifierTests.cpp
  core/ResultTests.cpp
  core/MeshFormatTests.cpp
  core/AssetPipelineTests.cpp
  core/FrustumTests.cpp
  app/EngineRuntimeTests.cpp
  app/EngineConfigTests.cpp
  app/SubsystemTests.cpp
  gameplay/TagTests.cpp
  gameplay/TagRegistryTests.cpp
  gameplay/GameStateMachineTests.cpp
  plugins/PluginRegistryTests.cpp
)

wayfinder_set_output_directories(${WAYFINDER_CORE_TEST_NAME})

target_link_libraries(${WAYFINDER_CORE_TEST_NAME}
  PRIVATE
    ${ENGINE_NAME}
    doctest::doctest_with_main
)

target_include_directories(${WAYFINDER_CORE_TEST_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME ${WAYFINDER_CORE_TEST_NAME} COMMAND ${WAYFINDER_CORE_TEST_NAME})
set_target_properties(${WAYFINDER_CORE_TEST_NAME} PROPERTIES FOLDER "${TESTS_DIRECTORY}")

# ── Scene Tests ───────────────────────────────────────────

set(WAYFINDER_SCENE_TEST_NAME wayfinder_scene_tests)

add_executable(${WAYFINDER_SCENE_TEST_NAME}
  scene/SceneComponentTests.cpp
  scene/ECSIntegrationTests.cpp
  scene/SceneLoadTests.cpp
  scene/SceneSaveTests.cpp
  scene/ComponentSerialisationTests.cpp
  scene/ComponentValidationTests.cpp
)

wayfinder_set_output_directories(${WAYFINDER_SCENE_TEST_NAME})

target_link_libraries(${WAYFINDER_SCENE_TEST_NAME}
  PRIVATE
    ${ENGINE_NAME}
    doctest::doctest_with_main
)

target_include_directories(${WAYFINDER_SCENE_TEST_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME ${WAYFINDER_SCENE_TEST_NAME} COMMAND ${WAYFINDER_SCENE_TEST_NAME})
set_target_properties(${WAYFINDER_SCENE_TEST_NAME} PROPERTIES FOLDER "${TESTS_DIRECTORY}")

# ── Physics Tests ─────────────────────────────────────────

set(WAYFINDER_PHYSICS_TEST_NAME wayfinder_physics_tests)

add_executable(${WAYFINDER_PHYSICS_TEST_NAME}
  physics/PhysicsTests.cpp
  physics/PhysicsIntegrationTests.cpp
)

wayfinder_set_output_directories(${WAYFINDER_PHYSICS_TEST_NAME})

target_link_libraries(${WAYFINDER_PHYSICS_TEST_NAME}
  PRIVATE
    ${ENGINE_NAME}
    doctest::doctest_with_main
)

target_include_directories(${WAYFINDER_PHYSICS_TEST_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME ${WAYFINDER_PHYSICS_TEST_NAME} COMMAND ${WAYFINDER_PHYSICS_TEST_NAME})
set_target_properties(${WAYFINDER_PHYSICS_TEST_NAME} PROPERTIES FOLDER "${TESTS_DIRECTORY}")
