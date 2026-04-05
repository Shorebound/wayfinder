if(WAYFINDER_BUILD_SHARED_LIBS)
  set(ENGINE_LIB_TYPE SHARED)
else()
  set(ENGINE_LIB_TYPE STATIC)
endif()

set(ENGINE_EXPORT_NAME WAYFINDER)

add_library(${ENGINE_NAME} ${ENGINE_LIB_TYPE})
wayfinder_set_output_directories(${ENGINE_NAME})
message(STATUS "Engine: Configuring target ${ENGINE_NAME} (${ENGINE_LIB_TYPE})")

# Apply clang-tidy to engine sources only (not third-party).
if(WAYFINDER_CLANG_TIDY_COMMAND)
    set_target_properties(${ENGINE_NAME} PROPERTIES CXX_CLANG_TIDY "${WAYFINDER_CLANG_TIDY_COMMAND}")
endif()

target_compile_definitions(${ENGINE_NAME}
  PRIVATE # Only needed when building the engine itself
    # Platform-specific DLL export control (only if building DLL)
    $<IF:$<BOOL:${WAYFINDER_BUILD_SHARED_LIBS}>,$<IF:$<PLATFORM_ID:Windows>,WAYFINDER_BUILD_DLL,>,>
    # Add any other definitions ONLY needed privately by the engine
)

include(GenerateExportHeader)
if(WIN32 AND WAYFINDER_BUILD_SHARED_LIBS)
  # Define WAYFINDER_EXPORTS *only* when building this DLL itself
  target_compile_definitions(${ENGINE_NAME} PRIVATE WAYFINDER_EXPORTS)
  message(STATUS "Engine: Enabling DLL export definitions for ${ENGINE_NAME}")
endif()

# Generate wayfinder_exports.h in the build directory's include folder
# This header defines the WAYFINDER_API macro (__declspec(dllexport/dllimport) or nothing)
generate_export_header(${ENGINE_NAME}
    BASE_NAME ${ENGINE_EXPORT_NAME}
    EXPORT_MACRO_NAME ${ENGINE_EXPORT_NAME}_API
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/wayfinder_exports.h
)

# --- Sources (explicit list — no GLOB_RECURSE) ---
set(ENGINE_ALL_SOURCES
  # Root
  src/WayfinderPCH.cpp
  src/WayfinderPCH.h

  # App
  src/app/Application.cpp
  src/app/Application.h
  src/app/AppBuilder.cpp
  src/app/AppBuilder.h
  src/app/AppDescriptor.h
  src/app/ApplicationStateMachine.cpp
  src/app/ApplicationStateMachine.h
  src/app/AppSubsystem.h
  src/app/ConfigRegistrar.cpp
  src/app/ConfigRegistrar.h
  src/app/ConfigService.cpp
  src/app/ConfigService.h
  src/app/EngineConfig.cpp
  src/app/EngineConfig.h
  src/app/EngineContext.cpp
  src/app/EngineContext.h
  src/app/EngineRuntime.cpp
  src/app/EngineRuntime.h
  src/app/FpsOverlayLayer.cpp
  src/app/FpsOverlayLayer.h
  src/app/IApplicationState.h
  src/app/IOverlay.h
  src/app/OrchestrationTypes.h
  src/app/OverlayManifest.h
  src/app/OverlayStack.cpp
  src/app/OverlayStack.h
  src/app/Layer.h
  src/app/LayerStack.cpp
  src/app/LayerStack.h
  src/app/StateManifest.h
  src/app/StateSubsystem.h
  src/app/Subsystem.h
  src/app/SubsystemManifest.h
  src/app/SubsystemRegistry.h

  # Assets
  src/assets/AssetCache.h
  src/assets/AssetLoader.h
  src/assets/AssetRegistry.cpp
  src/assets/AssetRegistry.h
  src/assets/AssetSchemaRegistry.cpp
  src/assets/AssetSchemaRegistry.h
  src/assets/AssetService.cpp
  src/assets/AssetService.h
  src/assets/MeshAsset.cpp
  src/assets/MeshAsset.h
  src/assets/MeshFormat.cpp
  src/assets/MeshFormat.h
  src/assets/TextureAsset.cpp
  src/assets/TextureAsset.h

  # Core
  src/core/Assert.h
  src/core/Core.h
  src/core/Handle.h
  src/core/Identifiers.cpp
  src/core/Identifiers.h
  src/core/TransparentStringHash.h
  src/core/InternedString.cpp
  src/core/InternedString.h
  src/core/Log.cpp
  src/core/Log.h
  src/core/Profiling.h
  src/core/ResourcePool.h
  src/core/Result.h
  src/core/TopologicalSort.h
  src/core/Types.h
  src/core/ValidationResult.h

  # Core — Events
  src/core/events/ApplicationEvent.h
  src/core/events/Event.h
  src/core/events/EventQueue.cpp
  src/core/events/EventQueue.h
  src/core/events/KeyEvent.h
  src/core/events/MouseEvent.h
  src/core/events/TypedEventBuffer.h

  # Core — Logging
  src/core/logging/ILogOutput.h
  src/core/logging/ILogger.h
  src/core/logging/LogTypes.h
  src/core/logging/spdlog/SpdLogManager.h
  src/core/logging/spdlog/SpdLogOutput.cpp
  src/core/logging/spdlog/SpdLogOutput.h
  src/core/logging/spdlog/SpdLogger.h

  # Gameplay
  src/gameplay/Capability.h
  src/gameplay/EngineContextRef.h
  src/gameplay/Game.cpp
  src/gameplay/Game.h
  src/gameplay/GameContext.h
  src/gameplay/GameState.cpp
  src/gameplay/GameState.h
  src/gameplay/GameStateMachine.cpp
  src/gameplay/GameStateMachine.h
  src/gameplay/Simulation.cpp
  src/gameplay/Simulation.h
  src/gameplay/StateMachine.h
  src/gameplay/Tag.cpp
  src/gameplay/Tag.h
  src/gameplay/NativeTag.h
  src/gameplay/TagRegistry.cpp
  src/gameplay/TagRegistry.h

  # Maths
  src/maths/Bounds.cpp
  src/maths/Bounds.h
  src/maths/Frustum.cpp
  src/maths/Frustum.h
  src/maths/Maths.cpp
  src/maths/Maths.h

  # Plugins
  src/plugins/IPlugin.h
  src/plugins/IRegistrar.h
  src/plugins/IStateUI.h
  src/plugins/LifecycleHooks.h
  src/plugins/PluginConcepts.h
  src/plugins/PluginDescriptor.h
  src/plugins/Plugin.h
  src/plugins/PluginRegistry.cpp
  src/plugins/PluginRegistry.h
  src/plugins/EngineContextServiceProvider.h
  src/plugins/ServiceProvider.h
  src/plugins/registrars/StateRegistrar.cpp
  src/plugins/registrars/StateRegistrar.h
  src/plugins/registrars/SystemRegistrar.cpp
  src/plugins/registrars/SystemRegistrar.h
  src/plugins/registrars/TagRegistrar.cpp
  src/plugins/registrars/TagRegistrar.h

  # Physics
  src/physics/PhysicsComponents.h
  src/physics/PhysicsPlugin.cpp
  src/physics/PhysicsPlugin.h
  src/physics/PhysicsSubsystem.cpp
  src/physics/PhysicsSubsystem.h
  src/physics/PhysicsWorld.cpp
  src/physics/PhysicsWorld.h

  # Platform
  src/platform/BackendConfig.h
  src/platform/Input.h
  src/platform/Paths.h
  src/platform/KeyCodes.h
  src/platform/MouseCodes.h
  src/platform/Time.h
  src/platform/Window.h
  src/platform/null/NullInput.h
  src/platform/null/NullTime.h
  src/platform/null/NullWindow.h
  src/platform/sdl3/SDL3Input.cpp
  src/platform/sdl3/SDL3Input.h
  src/platform/sdl3/SDL3Time.cpp
  src/platform/sdl3/SDL3Time.h
  src/platform/sdl3/SDL3Window.cpp
  src/platform/sdl3/SDL3Window.h

  # Project
  src/project/ProjectDescriptor.cpp
  src/project/ProjectDescriptor.h
  src/project/ProjectResolver.cpp
  src/project/ProjectResolver.h

  # Rendering
  src/rendering/ArenaFunction.h
  src/rendering/FrameAllocator.cpp
  src/rendering/FrameAllocator.h
  src/rendering/RenderTypes.h

  # Rendering — Backend
  src/rendering/backend/GPUBuffer.cpp
  src/rendering/backend/GPUBuffer.h
  src/rendering/backend/GPUHandles.h
  src/rendering/backend/RenderDevice.h
  src/rendering/backend/VertexFormats.h
  src/rendering/backend/null/NullDevice.cpp
  src/rendering/backend/null/NullDevice.h
  src/rendering/backend/sdl_gpu/SDLGPUDevice.cpp
  src/rendering/backend/sdl_gpu/SDLGPUDevice.h
  src/rendering/backend/sdl_gpu/MipGenerator.h
  src/rendering/backend/sdl_gpu/BlitMipGenerator.h

  # Rendering — Graph
  src/rendering/graph/Canvas.cpp
  src/rendering/graph/Canvas.h
  src/rendering/graph/RenderFrame.h
  src/rendering/graph/RenderFrameUtils.cpp
  src/rendering/graph/RenderFrameUtils.h
  src/rendering/graph/RenderFeature.h
  src/rendering/graph/RenderCapabilities.h
  src/rendering/graph/RenderGraph.cpp
  src/rendering/graph/RenderGraph.h
  src/rendering/graph/RenderIntent.h
  src/rendering/graph/SortKey.h

  # Rendering — Materials
  src/rendering/materials/Material.cpp
  src/rendering/materials/Material.h
  src/rendering/materials/MaterialParameter.cpp
  src/rendering/materials/MaterialParameter.h
  src/rendering/materials/RenderingEffects.cpp
  src/rendering/materials/RenderingEffects.h
  src/rendering/materials/ShaderManager.cpp
  src/rendering/materials/ShaderManager.h
  src/rendering/materials/ShaderProgram.cpp
  src/rendering/materials/ShaderProgram.h
  src/rendering/materials/SlangCompiler.cpp
  src/rendering/materials/SlangCompiler.h

  # Rendering — Mesh
  src/rendering/mesh/Mesh.cpp
  src/rendering/mesh/Mesh.h

  # Rendering -- Pipeline
  src/rendering/pipeline/PipelineCache.cpp
  src/rendering/pipeline/PipelineCache.h
  src/rendering/pipeline/RenderServices.cpp
  src/rendering/pipeline/RenderServices.h
  src/rendering/pipeline/BuiltInUBOs.h
  src/rendering/pipeline/DefaultFeatures.cpp
  src/rendering/pipeline/DefaultFeatures.h
  src/rendering/pipeline/PrepareFrame.cpp
  src/rendering/pipeline/PrepareFrame.h
  src/rendering/pipeline/RenderOrchestrator.cpp
  src/rendering/pipeline/RenderOrchestrator.h
  src/rendering/pipeline/Renderer.cpp
  src/rendering/pipeline/Renderer.h
  src/rendering/pipeline/SceneRenderExtractor.cpp
  src/rendering/pipeline/SceneRenderExtractor.h

  # Rendering — Passes
  src/rendering/passes/ChromaticAberrationFeature.cpp
  src/rendering/passes/ChromaticAberrationFeature.h
  src/rendering/passes/ColourGradingFeature.cpp
  src/rendering/passes/ColourGradingFeature.h
  src/rendering/passes/CompositionPass.cpp
  src/rendering/passes/CompositionPass.h
  src/rendering/passes/DebugPass.cpp
  src/rendering/passes/DebugPass.h
  src/rendering/passes/SceneOpaquePass.cpp
  src/rendering/passes/SceneOpaquePass.h
  src/rendering/passes/VignetteFeature.cpp
  src/rendering/passes/VignetteFeature.h
  src/rendering/passes/SubmissionDrawing.cpp
  src/rendering/passes/SubmissionDrawing.h

  # Rendering — Resources
  src/rendering/resources/RenderResourceCache.cpp
  src/rendering/resources/RenderResourceCache.h
  src/rendering/resources/MeshManager.cpp
  src/rendering/resources/MeshManager.h
  src/rendering/resources/TextureManager.cpp
  src/rendering/resources/TextureManager.h
  src/rendering/resources/TransientBufferAllocator.cpp
  src/rendering/resources/TransientBufferAllocator.h
  src/rendering/resources/TransientResourcePool.cpp
  src/rendering/resources/TransientResourcePool.h

  # Volumes
  src/volumes/Override.h
  src/volumes/OverrideReflection.cpp
  src/volumes/OverrideReflection.h
  src/volumes/BlendableEffect.cpp
  src/volumes/BlendableEffect.h
  src/volumes/BlendableEffectRegistry.cpp
  src/volumes/BlendableEffectRegistry.h
  src/volumes/BlendableEffectRegistry.inl
  src/volumes/BlendableEffectTraits.h
  src/volumes/BlendableEffectUtils.h

  # Scene
  src/scene/ComponentRegistry.cpp
  src/scene/ComponentRegistry.h
  src/scene/Components.h
  src/scene/RuntimeComponentRegistry.cpp
  src/scene/RuntimeComponentRegistry.h
  src/scene/Scene.cpp
  src/scene/Scene.h
  src/scene/SceneWorldBootstrap.cpp
  src/scene/SceneWorldBootstrap.h
  src/scene/plugins/CameraPlugin.cpp
  src/scene/plugins/CameraPlugin.h
  src/scene/plugins/TransformPlugin.cpp
  src/scene/plugins/TransformPlugin.h
  src/scene/SceneDocument.cpp
  src/scene/SceneDocument.h
  src/scene/SceneSettings.h
  src/scene/entity/Entity.cpp
  src/scene/entity/Entity.h
)

target_sources(${ENGINE_NAME} PRIVATE ${ENGINE_ALL_SOURCES})

foreach(FILEPATH ${ENGINE_ALL_SOURCES})
    # Resolve to absolute so source_group and RELATIVE_PATH work correctly
    cmake_path(ABSOLUTE_PATH FILEPATH BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE ABS_PATH)
    # Get the path relative to src/
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src" "${ABS_PATH}")
    # Get the directory part
    get_filename_component(PARENT_DIR "${REL_PATH}" DIRECTORY)
    # Replace / with \\ for Visual Studio filters
    string(REPLACE "/" "\\" GROUP_PATH "${PARENT_DIR}")
    # Create the group (CMake handles duplicates)
    if(GROUP_PATH)
      source_group("${GROUP_PATH}" FILES "${ABS_PATH}")
    else()
      # Files directly under src/
      source_group("Root" FILES "${ABS_PATH}")
    endif()
endforeach()

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Core.h
    DESTINATION include/wayfinder/core
)

# Include directories
target_include_directories(${ENGINE_NAME}
  PUBLIC
    # PUBLIC: Needed by Wayfinder itself AND consumers (like waystone)
    # Allows #include "core/Log.h", #include "app/Application.h", etc.
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # PUBLIC: Needed by consumers to find the generated Wayfinder_export.h
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)

target_link_libraries(${ENGINE_NAME}
    PUBLIC
      wayfinder_common
      SDL3::SDL3
      glm::glm
      flecs::flecs_static
      tomlplusplus::tomlplusplus
      nlohmann_json::nlohmann_json
      Jolt
    PRIVATE
      SDL3_image::SDL3_image-static
      Slang::slang
)

if(WAYFINDER_ENABLE_PROFILING)
    target_link_libraries(${ENGINE_NAME} PUBLIC TracyClient)
endif()

if(WAYFINDER_ENABLE_LOGGING)
    target_link_libraries(${ENGINE_NAME} PUBLIC spdlog::spdlog)
    target_sources(${ENGINE_NAME} 
    PRIVATE 
      src/core/Log.cpp
      src/core/logging/spdlog/SpdLogOutput.cpp
    )
endif()

# Configure precompiled headers
target_precompile_headers(${ENGINE_NAME}
    PRIVATE
        src/WayfinderPCH.h
)

message(STATUS "Engine: Configured precompiled headers for ${ENGINE_NAME}")