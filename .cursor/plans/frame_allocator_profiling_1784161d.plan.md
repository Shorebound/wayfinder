---
name: Frame Allocator Profiling
overview: Wire Tracy into the Wayfinder build system, create engine profiling macros, and instrument FrameAllocator with Tracy zones, plots, and per-frame validation. This is the first Tracy integration in the codebase.
todos:
  - id: cmake-tracy
    content: "Wire Tracy into CMake: WAYFINDER_ENABLE_PROFILING option, TRACY_ENABLE control, TracyClient link, compile definition"
    status: completed
  - id: profiling-header
    content: Create core/Profiling.h with wrapper macros (zones, plots, frame marks) and add to CMakeLists source list
    status: completed
  - id: annotate-allocator
    content: Add Tracy zones and plots to FrameAllocator::Allocate() and Reset(), add m_allocationCount member
    status: completed
  - id: frame-mark
    content: Add WAYFINDER_PROFILE_FRAME_MARK() in Application::Loop()
    status: completed
  - id: presets
    content: Enable profiling in dev preset, leave off for shipping/ci
    status: completed
  - id: build-verify
    content: Build and verify no compilation errors with profiling both ON and OFF
    status: completed
isProject: false
---

# Frame Allocator Profiling and Validation (Issue #52)

This is the first Tracy integration in the codebase. Tracy is already declared as a CPM dependency (`v0.13.1`) in [`cmake/WayfinderDependencies.cmake`](cmake/WayfinderDependencies.cmake) but is not linked, included, or used anywhere. The `TracyClient` CMake target already builds successfully.

## 1. CMake: Wire Tracy into the build

**Add `WAYFINDER_ENABLE_PROFILING` option** in [`CMakeLists.txt`](CMakeLists.txt) following the pattern of `WAYFINDER_ENABLE_LOGGING` (line 96):

```cmake
option(WAYFINDER_ENABLE_PROFILING "Enable Tracy profiling instrumentation" OFF)
```

**Set `TRACY_ENABLE` before Tracy is fetched.** In [`cmake/WayfinderDependencies.cmake`](cmake/WayfinderDependencies.cmake), add before the Tracy CPMAddPackage block:

```cmake
if(WAYFINDER_ENABLE_PROFILING)
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
else()
    set(TRACY_ENABLE OFF CACHE BOOL "" FORCE)
endif()
```

This controls whether Tracy macros compile to real instrumentation or no-ops.

**Add `WAYFINDER_PROFILING_ENABLED` compile definition** in [`cmake/WayfinderCommon.cmake`](cmake/WayfinderCommon.cmake) alongside the existing defines:

```cmake
$<$<BOOL:${WAYFINDER_ENABLE_PROFILING}>:WAYFINDER_PROFILING_ENABLED>
```

**Conditionally link TracyClient** in [`engine/wayfinder/CMakeLists.txt`](engine/wayfinder/CMakeLists.txt):

```cmake
if(WAYFINDER_ENABLE_PROFILING)
    target_link_libraries(${ENGINE_NAME} PUBLIC TracyClient)
endif()
```

**Enable profiling in dev presets** by adding `"WAYFINDER_ENABLE_PROFILING": "ON"` to the `dev` preset in [`CMakePresets.json`](CMakePresets.json). Leave it OFF for `shipping` and `ci` presets.

## 2. Create `core/Profiling.h`

New file: [`engine/wayfinder/src/core/Profiling.h`](engine/wayfinder/src/core/Profiling.h)

Engine-level wrapper macros that decouple application code from Tracy. When `WAYFINDER_PROFILING_ENABLED` is defined, they expand to Tracy calls; otherwise they are empty.

```cpp
#pragma once

#ifdef WAYFINDER_PROFILING_ENABLED
    #include <tracy/Tracy.hpp>

    #define WAYFINDER_PROFILE_SCOPE()               ZoneScoped
    #define WAYFINDER_PROFILE_SCOPE_NAMED(name)     ZoneScopedN(name)
    #define WAYFINDER_PROFILE_FUNCTION()             ZoneScoped

    #define WAYFINDER_PROFILE_FRAME_MARK()           FrameMark
    #define WAYFINDER_PROFILE_PLOT(name, value)      TracyPlot(name, value)

#else
    #define WAYFINDER_PROFILE_SCOPE()               ((void)0)
    #define WAYFINDER_PROFILE_SCOPE_NAMED(name)     ((void)0)
    #define WAYFINDER_PROFILE_FUNCTION()             ((void)0)

    #define WAYFINDER_PROFILE_FRAME_MARK()           ((void)0)
    #define WAYFINDER_PROFILE_PLOT(name, value)      ((void)0)
#endif
```

`ZoneScopedN(name)` is Tracy's built-in macro for named zones -- it handles unique variable naming internally, avoiding the `__LINE__` token-pasting pitfall.

Add to the engine source list in [`engine/wayfinder/CMakeLists.txt`](engine/wayfinder/CMakeLists.txt) under the `# Core` section.

## 3. Annotate FrameAllocator

### [`engine/wayfinder/src/rendering/FrameAllocator.h`](engine/wayfinder/src/rendering/FrameAllocator.h)

- Add `m_allocationCount` (`size_t`, default `0`) member variable
- Add `GetAllocationCount() const` accessor

### [`engine/wayfinder/src/rendering/FrameAllocator.cpp`](engine/wayfinder/src/rendering/FrameAllocator.cpp)

- Include `core/Profiling.h`
- **`Allocate()`**: Add `WAYFINDER_PROFILE_SCOPE()` at the top; increment `m_allocationCount`
- **`Reset()`**: Before clearing state, emit Tracy plots:
  - `WAYFINDER_PROFILE_PLOT("FrameAllocator/UsedBytes", static_cast<int64_t>(GetUsedBytes()))`
  - `WAYFINDER_PROFILE_PLOT("FrameAllocator/AllocationCount", static_cast<int64_t>(m_allocationCount))`
  - Reset `m_allocationCount = 0`

## 4. Add frame mark at the frame boundary

In [`engine/wayfinder/src/app/Application.cpp`](engine/wayfinder/src/app/Application.cpp), add `#include "core/Profiling.h"` and place `WAYFINDER_PROFILE_FRAME_MARK()` at the end of each loop iteration (after `m_runtime->EndFrame()`). This marks the frame boundary for Tracy's frame view.

## 5. Verify Reset() is called exactly once per frame

The current architecture creates a stack-local `RenderGraph` (which owns the `FrameAllocator`) each frame in `Renderer::Render()`. The destructor `~FrameAllocator()` calls `Reset()` when the graph goes out of scope. This guarantees exactly one `Reset()` per frame.

To make this verifiable in Tracy: add a `WAYFINDER_PROFILE_SCOPE_NAMED("FrameAllocator::Reset")` zone to `Reset()`. In the Tracy profiler, this zone should appear exactly once per frame in the timeline view. Additionally, the `FrameAllocator/AllocationCount` plot being non-zero exactly once per frame confirms the one-reset-per-frame invariant.

## 6. Observation for follow-up

The `FrameAllocator` is designed with page retention ("Pages are retained for reuse -- no deallocation occurs") but the current frame loop creates/destroys a new `RenderGraph` (and its `FrameAllocator`) every frame, defeating this design. Making the allocator persistent on the `Renderer` and passing it by reference to `RenderGraph` would eliminate per-frame page allocation/deallocation overhead. This is out of scope for issue #52 but should be filed as a follow-up.

## Files changed

| File | Change |
|------|--------|
| [`CMakeLists.txt`](CMakeLists.txt) | Add `WAYFINDER_ENABLE_PROFILING` option |
| [`cmake/WayfinderCommon.cmake`](cmake/WayfinderCommon.cmake) | Add `WAYFINDER_PROFILING_ENABLED` define |
| [`cmake/WayfinderDependencies.cmake`](cmake/WayfinderDependencies.cmake) | Set `TRACY_ENABLE` before Tracy CPM block |
| [`CMakePresets.json`](CMakePresets.json) | Enable profiling in `dev` preset |
| [`engine/wayfinder/CMakeLists.txt`](engine/wayfinder/CMakeLists.txt) | Link TracyClient, add `Profiling.h` to sources |
| [`engine/wayfinder/src/core/Profiling.h`](engine/wayfinder/src/core/Profiling.h) | **New** -- Tracy wrapper macros |
| [`engine/wayfinder/src/rendering/FrameAllocator.h`](engine/wayfinder/src/rendering/FrameAllocator.h) | Add `m_allocationCount`, `GetAllocationCount()` |
| [`engine/wayfinder/src/rendering/FrameAllocator.cpp`](engine/wayfinder/src/rendering/FrameAllocator.cpp) | Tracy zones, plots, allocation counter |
| [`engine/wayfinder/src/app/Application.cpp`](engine/wayfinder/src/app/Application.cpp) | Frame mark |
