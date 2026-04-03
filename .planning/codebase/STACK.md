# Technology Stack

**Last mapped:** 2026-04-03
**Focus areas:** core, app, plugins, gameplay/simulation

---

## Language & Standard

| Component | Details |
|-----------|---------|
| C++ Standard | **C++23** (required, `-std=c++23` enforced) |
| C Standard | C (third-party only: MikkTSpace) |
| Shader Language | **Slang** (compiled to SPIR-V) |
| Scripting | None (plugin-based extensibility via C++) |

---

## Build System

| Aspect | Details |
|--------|---------|
| CMake | **4.0.1** minimum (`CMakeLists.txt` L1) |
| Package Manager | **CPM** (C++ Package Manager), cached in `thirdparty/` |
| Primary Generator | Ninja Multi-Config (Clang + LLD) |
| IDE Generator | Visual Studio 17 2022 |
| Compilers | Clang/Clang++ (primary), MSVC cl.exe (validation), GCC (CI) |

### Configurations

| Config | LTO | Profiling | Notes |
|--------|-----|-----------|-------|
| Debug | No | Optional | Full debug info |
| Development | Yes | Yes (Tracy) | Optimised + instrumented |
| Shipping | Yes | No | Final build |

### Presets (`CMakePresets.json`)

9 presets: `dev`, `dev-all`, `dev-msvc`, `dev-vs`, `dev-tidy`, `shipping`, `ci-linux`, `ci-windows`, `ci-emscripten` (planned).

```powershell
cmake --preset dev              # Ninja + Clang (sandbox + tools + tests)
cmake --build --preset debug    # Debug build
ctest --preset test             # Run all tests
```

### Key Build Flags

- Interprocedural Optimisation (LTO) for Development & Shipping
- Position Independent Code (PIE) globally
- Export compile commands for tooling
- Dynamic MSVC runtime (/MD, /MDd) for DLL boundary safety

### CMake Options

| Option | Default | Purpose |
|--------|---------|---------|
| `WAYFINDER_BUILD_SANDBOX` | ON | Journey sandbox executable |
| `WAYFINDER_BUILD_TOOLS` | OFF | Waypoint, Surveyor, etc. |
| `WAYFINDER_BUILD_TESTS` | OFF | doctest test suites |
| `WAYFINDER_BUILD_EDITOR` | OFF | Cartographer editor (not yet implemented) |
| `WAYFINDER_BUILD_RUNTIME` | OFF | Waystone runtime (not yet implemented) |
| `WAYFINDER_ENABLE_PROFILING` | OFF (dev: ON) | Tracy instrumentation |
| `WAYFINDER_ENABLE_LOGGING` | ON | spdlog + structured logging |
| `WAYFINDER_PHYSICS` | ON | Jolt physics subsystem |
| `WAYFINDER_ENABLE_EXCEPTIONS` | ON | C++ exceptions |
| `WAYFINDER_ENABLE_RTTI` | ON | RTTI support |
| `WAYFINDER_WARNINGS_AS_ERRORS` | OFF (CI: ON) | Strict warnings |

---

## Dependencies

### Core Runtime

| Dependency | Version | Source | Purpose |
|------------|---------|--------|---------|
| SDL3 | `main` (fork) | `Shorebound/sdl` | Window, input, platform abstraction |
| SDL3_image | `main` (fork) | `Shorebound/sdl-image` | Image format loading |
| SDL3 GPU | (in SDL3) | - | Graphics backend abstraction (Vulkan/D3D12/Metal) |
| GLM | 1.0.3 | `g-truc/glm` | Maths: vectors, matrices, quaternions |
| spdlog | 1.17.0 | `gabime/spdlog` | Structured logging with `std::format` |
| tomlplusplus | 3.4.0 | `marzer/tomlplusplus` | TOML config parsing |
| nlohmann/json | 3.12.0 | `nlohmann/json` | JSON parsing & serialisation |
| Flecs | `master` (fork) | `Shorebound/flecs` | Entity Component System |
| Jolt Physics | `master` (fork) | `Shorebound/jolt` | 3D physics (primary) |
| Box2D | 3.1.1 | `erincatto/box2d` | 2D physics (optional) |
| Dear ImGui | 1.92.6 | `ocornut/imgui` | Debug UI, editor panels |
| Tracy | 0.13.1 | `wolfpld/tracy` | CPU/GPU profiling (optional) |
| Slang SDK | 2026.5.1 | `shader-slang/slang` | Shader compiler + runtime reflection |

### Asset & Tool Dependencies (Waypoint only)

| Dependency | Version | Purpose |
|------------|---------|---------|
| fastgltf | v0.8.0 | glTF 2.0 import |
| meshoptimizer | v0.22 | Mesh compression, simplification |
| MikkTSpace | master | Tangent/bitangent generation |

### Testing

| Dependency | Version | Purpose |
|------------|---------|---------|
| doctest | 2.4.11 | C++ BDD test framework |

### Dependency Notes

- All third-party targets marked `SYSTEM TRUE` (suppress their warnings)
- SDL3_image uses vendored image codecs
- ImGui wrapped with SDL3/SDL_GPU backend
- MikkTSpace built as C-only
- Jolt has Spectre mitigation flag suppressed
- Several deps are Shorebound forks for stability/patches

---

## Graphics & Shader Pipeline

### GPU Backend

| Component | Details |
|-----------|---------|
| Abstraction | **SDL_GPU** |
| Supported backends | Vulkan, Direct3D 12, Metal, OpenGL (fallback) |
| Shader IR | **SPIR-V** |
| Headless | **Null Device** (no GPU required) |

Key files:
- `engine/wayfinder/src/rendering/backend/RenderDevice.h` - abstract interface
- `engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.cpp` - SDL_GPU impl
- `engine/wayfinder/src/rendering/backend/null/NullDevice.cpp` - headless stub

### Shader Toolchain

| Stage | Tool | Details |
|-------|------|---------|
| Source | Slang | `.slang` files in `engine/wayfinder/shaders/` |
| Compilation | `slangc` | Downloaded via FetchContent (`cmake/WayfinderSlang.cmake`) |
| Target | SPIR-V | `-target spirv -emit-spirv-directly` |
| Build-time | `wayfinder_compile_shaders()` | Module precompilation + program compilation |
| Module caching | `.slang-module` | Precompiled intermediates |
| Artefacts | `.vert.spv`, `.frag.spv`, `shader_manifest.json` | Per-config output |
| Runtime | Slang Runtime | Optional runtime compilation if shader source configured |

Entry points: `VSMain` (vertex), `PSMain` (fragment), with `-fvk-use-entrypoint-name`.

---

## Configuration Formats

| Format | Use Case | Examples |
|--------|----------|---------|
| **TOML** | Hand-authored config | Engine config, input mappings, project descriptors, gameplay tags |
| **JSON** | Generated/structured data | Asset descriptors, scene files, shader manifests |
| **C++ structs** | Runtime configs | `EngineConfig`, `BackendConfig`, `PhysicsConfig` |

---

## Platform Targets

| Platform | Status | Notes |
|----------|--------|-------|
| Windows | **Primary** | Ninja + Clang, Ninja + MSVC, VS 2022 |
| Linux | Supported | Clang with libc++ (CI preset) |
| macOS | Planned | Slang SDK available for x86_64 + ARM64 |
| Web/Emscripten | Planned | CI preset stub exists |

---

## Infrastructure

| Component | Details |
|-----------|---------|
| Precompiled headers | `WayfinderPCH.h` (engine-wide, not third-party) |
| DLL export | Generated `wayfinder_exports.h` for Windows |
| Slang caching | Optional `SLANG_SDK_CACHE_DIR` for CI |
| Data-driven | Config, render passes, asset schemas all file-based |
