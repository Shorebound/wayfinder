set(WAYFINDER_RENDER_TEST_NAME wayfinder_render_tests)

add_executable(${WAYFINDER_RENDER_TEST_NAME}
  rendering/RenderPipelineTests.cpp
  rendering/RenderGraphTests.cpp
  rendering/RenderFeatureTests.cpp
  rendering/RenderContextTests.cpp
)

wayfinder_set_output_directories(${WAYFINDER_RENDER_TEST_NAME})

target_link_libraries(${WAYFINDER_RENDER_TEST_NAME}
  PRIVATE
    ${ENGINE_NAME}
    doctest::doctest_with_main
)

add_test(NAME ${WAYFINDER_RENDER_TEST_NAME} COMMAND ${WAYFINDER_RENDER_TEST_NAME})
set_target_properties(${WAYFINDER_RENDER_TEST_NAME} PROPERTIES FOLDER "${TESTS_DIRECTORY}")

# ── Core Tests ────────────────────────────────────────────

set(WAYFINDER_CORE_TEST_NAME wayfinder_core_tests)

add_executable(${WAYFINDER_CORE_TEST_NAME}
  core/HandleTests.cpp
  core/InternedStringTests.cpp
  core/EngineRuntimeTests.cpp
  core/EventQueueTests.cpp
  core/IdentifierTests.cpp
  core/GameplayTagTests.cpp
  core/GameplayTagRegistryTests.cpp
  core/GameStateMachineTests.cpp
  core/ModuleRegistryTests.cpp
  core/SubsystemTests.cpp
  core/EngineConfigTests.cpp
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
  scene/ECSIntegrationTests.cpp
  scene/SceneLoadTests.cpp
  scene/SceneSaveTests.cpp
  scene/ComponentSerializationTests.cpp
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