set(WAYFINDER_RENDER_TEST_NAME wayfinder_render_tests)

add_executable(${WAYFINDER_RENDER_TEST_NAME}
  rendering/RenderPipelineTests.cpp
  rendering/RenderGraphTests.cpp
  rendering/RenderFeatureTests.cpp
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
)

wayfinder_set_output_directories(${WAYFINDER_CORE_TEST_NAME})

target_link_libraries(${WAYFINDER_CORE_TEST_NAME}
  PRIVATE
    ${ENGINE_NAME}
    doctest::doctest_with_main
)

add_test(NAME ${WAYFINDER_CORE_TEST_NAME} COMMAND ${WAYFINDER_CORE_TEST_NAME})
set_target_properties(${WAYFINDER_CORE_TEST_NAME} PROPERTIES FOLDER "${TESTS_DIRECTORY}")