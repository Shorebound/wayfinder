# Plan: Runtime Slang Compilation (#142)

## TL;DR

Add runtime Slang compilation to `ShaderManager` so Development/Debug builds compile `.slang` sources to SPIR-V on-demand when pre-compiled `.spv` files aren't found. This eliminates the offline build step during shader iteration. Includes `ReloadShaders()` for cache invalidation. Shipping builds always load pre-compiled `.spv` only. However, make sure there's a comment in the code explaining that a future issue can add explicit opt-in runtime compilation for Shipping (e.g., for modding) without architectural changes.

**Approach:** New `SlangCompiler` class wraps the Slang C++ API (always compiled, no `#ifdef`). `ShaderManager` gains a fallback path: when `.spv` not found and `SlangCompiler` is available, compile from `.slang` source. `RenderServices` creates and owns the `SlangCompiler`, passing it to `ShaderManager`. The Slang shared library (`slang.dll`) is linked to the engine target and auto-copied to output.

---

## Phase 1: Link Slang Runtime Library (CMake)

### Step 1.1: Create an imported target for Slang in `cmake/WayfinderSlang.cmake`

After the existing SDK download logic, add an `IMPORTED SHARED` library target `Slang::slang` that wraps the prebuilt SDK's headers and libraries. This makes linking idiomatic CMake.

- Set `IMPORTED_LOCATION` to `${SLANG_LIB_DIR}/slang.dll` (Windows) or `libslang.so` (Linux) / `libslang.dylib` (macOS).
- Set `IMPORTED_IMPLIB` to `${SLANG_LIB_DIR}/slang.lib` (Windows only).
- Set `INTERFACE_INCLUDE_DIRECTORIES` to `${SLANG_INCLUDE_DIR}`.
- Verify the expected files exist with `if(NOT EXISTS ...)` guards.

### Step 1.2: Link `Slang::slang` to the engine target in `engine/wayfinder/CMakeLists.txt`

Add `Slang::slang` to `target_link_libraries(wayfinder PRIVATE Slang::slang)`. The Slang COM pointer wrapper type used by `SlangCompiler` is forward-declared as an opaque type in the header (via pimpl), and `#include <slang.h>` is confined to `.cpp` files. This avoids leaking Slang headers to engine consumers.

### Step 1.3: Auto-copy `slang.dll` to output directory

Add a post-build step in `engine/wayfinder/CMakeLists.txt` (or the sandbox CMakeLists) that copies `slang.dll` (and any companion DLLs like `slang-llvm.dll` if present) next to the executable. Use a generator expression for the output directory.

Only copy for non-Shipping configs (Shipping doesn't need the DLL since runtime compilation is disabled). Use `$<NOT:$<CONFIG:Shipping>>` guard.

**Files modified:**
- `cmake/WayfinderSlang.cmake` - add imported target
- `engine/wayfinder/CMakeLists.txt` - link Slang::slang, add DLL copy
- OR `sandbox/journey/CMakeLists.txt` - DLL copy may be better here since sandbox owns the output dir

---

## Phase 2: SlangCompiler Class

### Step 2.1: Create `engine/wayfinder/src/rendering/materials/SlangCompiler.h`

New class in `Wayfinder` namespace. Responsible for:
- Owning a `slang::IGlobalSession` and `slang::ISession` (Slang's compilation state)
- Compiling a `.slang` source file to SPIR-V bytecode for a given entry point and stage
- Configuring search paths for module resolution (so `import "modules/transforms"` works)
- Surfacing compilation errors/warnings through engine logging with file/line info

**Public interface:**

```cpp
class SlangCompiler
{
public:
    SlangCompiler();
    ~SlangCompiler();

    // Non-copyable, non-movable (owns COM pointers)
    SlangCompiler(const SlangCompiler&) = delete;
    SlangCompiler& operator=(const SlangCompiler&) = delete;

    struct InitDesc
    {
        std::string_view sourceDirectory;   // Root directory containing .slang files
        std::span<const std::string_view> searchPaths = {};  // Additional -I paths for module resolution
    };

    Result<void> Initialise(const InitDesc& desc);
    void Shutdown();

    struct CompileResult
    {
        std::vector<uint8_t> bytecode;   // SPIR-V bytecode
    };

    // Compile a .slang source file to SPIR-V for a specific entry point and stage.
    // sourceName: stem name (e.g. "basic_lit") - resolved against sourceDirectory
    // entryPoint: function name (e.g. "VSMain", "PSMain")
    // stage: vertex, fragment, or compute
    Result<CompileResult> Compile(
        std::string_view sourceName,
        std::string_view entryPoint,
        ShaderStage stage) const;

    bool IsInitialised() const;

private:
    // Opaque pointers to Slang COM objects (avoid slang.h in header)
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::string m_sourceDirectory;
};
```

Key design decisions:
- **Pimpl idiom** via `std::unique_ptr<Impl>` to keep `slang.h` out of the header. The Impl struct holds `Slang::ComPtr<slang::IGlobalSession>` and `Slang::ComPtr<slang::ISession>`.
- **`Result<CompileResult>`** return type follows engine convention.
- **`const` on Compile()** - Slang sessions are thread-safe for reads; the global session is created once.
- **`sourceName`** is the stem (e.g. `"basic_lit"`) - the compiler appends `.slang` and resolves against `m_sourceDirectory`.

### Step 2.2: Create `engine/wayfinder/src/rendering/materials/SlangCompiler.cpp`

Implementation using the Slang C++ API. The compilation flow per call to `Compile()`:

1. Build the full path: `m_sourceDirectory / (sourceName + ".slang")`
2. `session->loadModule(path, diagnostics)` - loads and parses the `.slang` file (modules resolved via search paths configured at init)
3. `module->findEntryPointByName(entryPoint, entryPointObj)` - finds the specific entry point
4. `session->createCompositeComponentType({module, entryPoint}, composedProgram, diagnostics)` - composes the program
5. `composedProgram->getEntryPointCode(0, 0, spirvBlob, diagnostics)` - emits SPIR-V
6. Copy `spirvBlob->getBufferPointer()` / `getBufferSize()` into `CompileResult.bytecode`
7. If any step fails, log diagnostics with `WAYFINDER_ERROR(LogRenderer, ...)` including file/line info from the diagnostic blob, and return `MakeError(...)`.

**Session configuration at init time:**

```cpp
slang::SessionDesc sessionDesc = {};
slang::TargetDesc targetDesc = {};
targetDesc.format = SLANG_SPIRV;
targetDesc.profile = globalSession->findProfile("spirv_1_5");

// Compiler options to match offline slangc flags
slang::CompilerOptionEntry options[] = {
    { slang::CompilerOptionName::EmitSpirvDirectly, { .intValue0 = 1 } },
    { slang::CompilerOptionName::VulkanUseEntryPointName, { .intValue0 = 1 } },
};
sessionDesc.compilerOptionEntries = options;
sessionDesc.compilerOptionEntryCount = std::size(options);

sessionDesc.targets = &targetDesc;
sessionDesc.targetCount = 1;

// Search paths for module resolution
// 1. Source directory (for import "modules/transforms")
// 2. Any additional paths
sessionDesc.searchPaths = ...;
sessionDesc.searchPathCount = ...;
```

**Error reporting:** Parse the Slang diagnostic blob as a C-string and split by newlines. Each diagnostic line typically has `filename(line): error/warning: message` format. Log each line via `WAYFINDER_ERROR` or `WAYFINDER_WARN` as appropriate.

**Files created:**
- `engine/wayfinder/src/rendering/materials/SlangCompiler.h`
- `engine/wayfinder/src/rendering/materials/SlangCompiler.cpp`

---

## Phase 3: Integrate with ShaderManager

### Step 3.1: Add `ShaderConfig.SourceDirectory` to `EngineConfig`

Extend `ShaderConfig` in `engine/wayfinder/src/app/EngineConfig.h`:

```cpp
struct ShaderConfig
{
    std::string Directory = "assets/shaders";          // Compiled .spv files
    std::string SourceDirectory;                        // .slang source files (empty = disabled)
};
```

Parse the new field in `EngineConfig.cpp` from `[shaders].source_directory`. Default empty (runtime compilation disabled unless configured).

Update `engine.toml` config to add:
```toml
[shaders]
directory = "assets/shaders"
source_directory = "../../engine/wayfinder/shaders"    # Relative to executable
```

The source directory points to the engine's shader source tree. For development, this is a relative path from `bin/Debug/` back to `engine/wayfinder/shaders/`. The path is resolved the same way as `directory` - via `SDL_GetBasePath()`.

### Step 3.2: Modify `ShaderManager` to accept an optional `SlangCompiler*`

Add a `SlangCompiler*` member (nullable, non-owning) to `ShaderManager`. Modify `Initialise()`:

```cpp
void Initialise(RenderDevice& device, std::string_view shaderDirectory, SlangCompiler* compiler = nullptr);
```

### Step 3.3: Modify `ShaderManager::GetShader()` fallback path

Current flow:
1. Check cache -> return if found
2. Load `.spv` from disk -> create GPU shader -> cache -> return
3. If `.spv` not found -> error, return invalid

New flow:
1. Check cache -> return if found
2. Load `.spv` from disk -> if found, create GPU shader -> cache -> return
3. **If `.spv` not found AND `m_compiler` is not null AND build is not Shipping:**
   a. Call `m_compiler->Compile(name, entryPoint, stage)`
   b. If compilation succeeds, create GPU shader from bytecode -> cache -> return
   c. If compilation fails, error is already logged by SlangCompiler -> return invalid
4. If `.spv` not found and no compiler -> error, return invalid (existing behaviour)

The Shipping guard uses:
```cpp
#if !defined(WAYFINDER_SHIPPING)
    if (m_compiler && m_compiler->IsInitialised())
    {
        // ... runtime compilation fallback
    }
#endif
```

This means the fallback code is completely compiled out of Shipping builds - zero overhead.

### Step 3.4: Add `ReloadShaders()` to `ShaderManager`

```cpp
void ReloadShaders();
```

Implementation:
1. Destroy all cached GPU shader handles via `m_device->DestroyShader(handle)`
2. Clear `m_cache`
3. Log: `"ShaderManager: Shader cache invalidated - shaders will recompile on next use"`

This does NOT immediately recompile. The next call to `GetShader()` will trigger recompilation (either from .spv or from .slang source). This is intentional - lazy recompilation avoids compiling shaders that may not be used.

**Note:** Callers must also invalidate and recreate any GPU pipelines that reference the old shader handles. This means `ShaderProgramRegistry` and `PipelineCache` need their own invalidation. Add:

```cpp
// In ShaderProgramRegistry:
void InvalidateAll();   // Destroys all pipeline handles, clears registry

// In PipelineCache:
void InvalidateAll();   // Destroys all cached pipelines, clears cache
```

### Step 3.5: Add `ReloadShaders()` to `RenderServices`

Orchestrates the full invalidation chain:

```cpp
void RenderServices::ReloadShaders()
{
    m_pipelineCache.InvalidateAll();
    m_programRegistry.InvalidateAll();
    m_shaderManager.ReloadShaders();
    WAYFINDER_INFO(LogRenderer, "RenderServices: All shaders and pipelines invalidated");
}
```

After this, the next frame will lazily recompile all needed shaders and recreate pipelines. The `RenderOrchestrator` re-registers shader programs on its next `Initialise` or the existing lazy pipeline creation in `SubmissionDrawing` handles it.

Actually - `ShaderProgramRegistry::Register()` calls into `ShaderManager::GetShader()` and `PipelineCache` during registration. So after invalidation, the programs need to be re-registered. The simplest approach:

1. `ShaderProgramRegistry::InvalidateAll()` clears all programs + pipelines
2. `RenderOrchestrator::Initialise(services)` is called again to re-register all programs
3. OR `RenderOrchestrator` stores descriptors and has a `ReRegisterPrograms()` method

The cleanest architecture: `RenderOrchestrator` already stores `ShaderProgramDesc` objects for all programs during its `Initialise()`. Add a `RebuildPipelines()` method that re-registers them all. Call it from `RenderServices::ReloadShaders()`.

**Files modified:**
- `engine/wayfinder/src/app/EngineConfig.h` - add SourceDirectory field
- `engine/wayfinder/src/app/EngineConfig.cpp` - parse source_directory from TOML
- `engine/wayfinder/src/rendering/materials/ShaderManager.h` - add compiler ptr, ReloadShaders()
- `engine/wayfinder/src/rendering/materials/ShaderManager.cpp` - fallback compilation, reload
- `engine/wayfinder/src/rendering/pipeline/RenderServices.h` - add ReloadShaders()
- `engine/wayfinder/src/rendering/pipeline/RenderServices.cpp` - implement ReloadShaders()
- `engine/wayfinder/src/rendering/materials/ShaderProgram.h` - add InvalidateAll() to registry
- `engine/wayfinder/src/rendering/materials/ShaderProgram.cpp` - implement InvalidateAll()
- `engine/wayfinder/src/rendering/pipeline/PipelineCache.h` - add InvalidateAll()
- `engine/wayfinder/src/rendering/pipeline/PipelineCache.cpp` - implement InvalidateAll()
- `engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.h` - add RebuildPipelines()
- `engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.cpp` - implement RebuildPipelines()
- `bin/Debug/config/engine.toml` - add source_directory

---

## Phase 4: Wire SlangCompiler into RenderServices

### Step 4.1: Create and own `SlangCompiler` in `RenderServices`

Add a `SlangCompiler m_slangCompiler;` member to `RenderServices`.

In `RenderServices::Initialise()`, after existing setup:

```cpp
// Initialise Slang runtime compiler if source directory is configured
if (!config.Shaders.SourceDirectory.empty())
{
    SlangCompiler::InitDesc compilerDesc;
    compilerDesc.SourceDirectory = ResolveShaderDirectory(config.Shaders.SourceDirectory);
    compilerDesc.SearchPaths = {}; // Source dir already serves as search root

    auto compilerResult = m_slangCompiler.Initialise(compilerDesc);
    if (compilerResult)
    {
        WAYFINDER_INFO(LogRenderer, "Slang runtime compiler initialised - source: {}", compilerDesc.SourceDirectory);
    }
    else
    {
        WAYFINDER_WARN(LogRenderer, "Slang runtime compiler failed to initialise: {}", compilerResult.error().GetMessage());
        // Non-fatal: fall back to pre-compiled .spv only
    }
}
```

Then pass `&m_slangCompiler` (if initialised) to `ShaderManager::Initialise()`.

In `RenderServices::Shutdown()`, add `m_slangCompiler.Shutdown()` before `m_shaderManager.Shutdown()`.

**Note:** The `ResolveShaderDirectory` helper (currently a `ShaderManager` internal) should be factored out to a shared utility so `RenderServices` can use it for the source directory resolution. Move it to a small free function in a shared header, or duplicate the logic (it's ~10 lines).

**Files modified:**
- `engine/wayfinder/src/rendering/pipeline/RenderServices.h` - add SlangCompiler member
- `engine/wayfinder/src/rendering/pipeline/RenderServices.cpp` - init/shutdown compiler, pass to ShaderManager

---

## Phase 5: Update Source Lists and Build

### Step 5.1: Add new source files to `engine/wayfinder/CMakeLists.txt`

Add to the explicit source file list:
- `src/rendering/materials/SlangCompiler.h`
- `src/rendering/materials/SlangCompiler.cpp`

### Step 5.2: DLL copy step

In `sandbox/journey/CMakeLists.txt` (where other post-build copies happen), add:

```cmake
# Copy Slang runtime DLLs for Development/Debug builds
if(WIN32 AND EXISTS "${SLANG_LIB_DIR}")
    # Find all DLLs in the Slang SDK bin directory
    file(GLOB _SLANG_DLLS "${slang_sdk_SOURCE_DIR}/bin/*.dll")
    foreach(_DLL ${_SLANG_DLLS})
        add_custom_command(TARGET ${SANDBOX_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Shipping>,true,copy_if_different>
                "${_DLL}" "$<TARGET_FILE_DIR:${SANDBOX_NAME}>"
            COMMENT "Copying Slang DLL: ${_DLL}"
            VERBATIM
        )
    endforeach()
endif()
```

For non-Windows, `slang.so`/`slang.dylib` would need similar treatment but the Slang SDK structure differs. Start with Windows; add Unix support when needed.

---

## Phase 6: Tests

### Step 6.1: `SlangCompilerTests.cpp` in `tests/rendering/`

Test the `SlangCompiler` class directly. Tests run headless (no GPU), so they validate compilation output only.

**Test fixture setup:**
- Use existing `.slang` files from `engine/wayfinder/shaders/` as test inputs. Reference via relative path from test fixture location, or use a CMake-configured path define.
- Alternatively, create minimal `.slang` test fixture files under `tests/fixtures/shaders/` to avoid coupling to engine shaders.

**Test cases:**
1. **"SlangCompiler initialises with valid source directory"** - construct, init with the fixture shader path, verify `IsInitialised()` returns true.
2. **"SlangCompiler fails gracefully with invalid source directory"** - init with non-existent path, verify returns error Result.
3. **"Compiles a simple vertex shader to SPIR-V"** - compile a minimal `.slang` fixture, verify bytecode is non-empty and starts with SPIR-V magic number (`0x07230203`).
4. **"Compiles a simple fragment shader to SPIR-V"** - same for fragment stage.
5. **"Compilation fails for non-existent source file"** - compile `"doesnt_exist"`, verify error Result with descriptive message.
6. **"Compilation fails for invalid shader code"** - create a fixture `.slang` with syntax errors, verify error Result.
7. **"Compilation fails for non-existent entry point"** - compile with entry point name that doesn't exist, verify error Result.
8. **"Module imports resolve correctly"** - compile a fixture that imports a module (like `transforms`), verify success.
9. **"Multiple compilations from same session succeed"** - compile vertex then fragment from same file, verify both produce valid bytecode.

**Test fixture files** (create under `tests/fixtures/shaders/`):
- `simple.slang` - minimal vertex + fragment with `[shader("vertex")] VSMain` and `[shader("fragment")] PSMain`
- `modules/test_module.slang` - a simple module with an exported struct
- `with_import.slang` - imports `test_module` and uses its types
- `invalid_syntax.slang` - deliberately broken for error testing

### Step 6.2: `ShaderManagerRuntimeCompilationTests.cpp` or extend `RenderServicesTests.cpp`

Test the integration: `ShaderManager` with a `SlangCompiler` attached, using `NullDevice`.

**Test cases:**
1. **"GetShader falls back to runtime compilation when .spv missing"** - configure ShaderManager with a compiler and fixture shaders, call GetShader for a name with no .spv on disk. On NullDevice, CreateShader returns a handle (incrementing counter), so we can verify the path was taken by checking logs or handle validity.
2. **"GetShader prefers .spv when available"** - provide both .spv files and source files, verify .spv is loaded (no compilation triggered). This is hard to verify directly but could check that SlangCompiler::Compile was not called via a mock/spy pattern or simply verify the shader loads successfully.
3. **"ReloadShaders invalidates cache"** - load a shader, verify cached, call ReloadShaders(), verify next GetShader reloads.

### Step 6.3: Add test files to `tests/CMakeLists.txt`

Add `SlangCompilerTests.cpp` to `wayfinder_render_tests` target. Also link `Slang::slang` to the render test target.

**Files created:**
- `tests/rendering/SlangCompilerTests.cpp`
- `tests/fixtures/shaders/simple.slang`
- `tests/fixtures/shaders/modules/test_module.slang`
- `tests/fixtures/shaders/with_import.slang`
- `tests/fixtures/shaders/invalid_syntax.slang`

**Files modified:**
- `tests/CMakeLists.txt` - add new test source, link Slang

---

## Implementation Order & Dependencies

```
Phase 1: CMake (Steps 1.1-1.3)           [no dependencies]
Phase 2: SlangCompiler (Steps 2.1-2.2)   [depends on Phase 1 for Slang headers/lib]
Phase 3: ShaderManager integration        [depends on Phase 2]
  Step 3.1: EngineConfig changes          [no dependencies, parallel with Phase 2]
  Step 3.2-3.3: ShaderManager changes     [depends on Phase 2]
  Step 3.4-3.5: ReloadShaders()           [depends on Step 3.2]
Phase 4: RenderServices wiring            [depends on Phase 2 + 3]
Phase 5: Build integration                [depends on Phase 2]
  Step 5.1: Source list update            [depends on Phase 2]
  Step 5.2: DLL copy                      [depends on Phase 1]
Phase 6: Tests                            [depends on Phase 2 + 3]
  Step 6.1: SlangCompiler tests           [depends on Phase 2]
  Step 6.2: Integration tests             [depends on Phase 3 + 4]
  Step 6.3: CMake test wiring             [depends on Steps 6.1-6.2]
```

**Critical gate:** Phase 2 (SlangCompiler) - if the Slang C++ API doesn't work as expected (COM lifetimes, session configuration, SPIR-V output format), everything downstream is blocked. Build and test SlangCompiler in isolation first.

---

## Relevant Files

### New files
- `engine/wayfinder/src/rendering/materials/SlangCompiler.h` - Slang C++ API wrapper (pimpl)
- `engine/wayfinder/src/rendering/materials/SlangCompiler.cpp` - implementation: session init, compile, error reporting
- `tests/rendering/SlangCompilerTests.cpp` - unit tests for compiler
- `tests/fixtures/shaders/simple.slang` - minimal test shader
- `tests/fixtures/shaders/modules/test_module.slang` - test module
- `tests/fixtures/shaders/with_import.slang` - import test
- `tests/fixtures/shaders/invalid_syntax.slang` - error path test

### Modified files
- `cmake/WayfinderSlang.cmake` - add `Slang::slang` imported target (reuse `SLANG_INCLUDE_DIR`, `SLANG_LIB_DIR`)
- `engine/wayfinder/CMakeLists.txt` - link `Slang::slang`, add `SlangCompiler.h/.cpp` to source list
- `sandbox/journey/CMakeLists.txt` - DLL copy post-build step for non-Shipping
- `engine/wayfinder/src/app/EngineConfig.h` - add `ShaderConfig::SourceDirectory`
- `engine/wayfinder/src/app/EngineConfig.cpp` - parse `[shaders].source_directory` from TOML
- `engine/wayfinder/src/rendering/materials/ShaderManager.h` - add `SlangCompiler*`, `ReloadShaders()`
- `engine/wayfinder/src/rendering/materials/ShaderManager.cpp` - fallback compilation path, reload implementation
- `engine/wayfinder/src/rendering/pipeline/RenderServices.h` - add `SlangCompiler` member, `ReloadShaders()`
- `engine/wayfinder/src/rendering/pipeline/RenderServices.cpp` - init/shutdown compiler, orchestrate reload
- `engine/wayfinder/src/rendering/materials/ShaderProgram.h` - add `InvalidateAll()` to `ShaderProgramRegistry`
- `engine/wayfinder/src/rendering/materials/ShaderProgram.cpp` - implement `InvalidateAll()`
- `engine/wayfinder/src/rendering/pipeline/PipelineCache.h` - add `InvalidateAll()`
- `engine/wayfinder/src/rendering/pipeline/PipelineCache.cpp` - implement `InvalidateAll()`
- `engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.h` - add `RebuildPipelines()`
- `engine/wayfinder/src/rendering/pipeline/RenderOrchestrator.cpp` - re-register all shader programs
- `bin/Debug/config/engine.toml` - add `source_directory` field
- `tests/CMakeLists.txt` - add test source, link Slang

---

## Verification

1. **Configure:** `cmake --preset dev` succeeds - Slang imported target resolves, engine links against `slang.lib`
2. **Build:** `cmake --build --preset debug` - all targets compile including `SlangCompiler`
3. **DLL present:** `bin/Debug/slang.dll` exists (and any companion DLLs)
4. **Tests:** `ctest --preset test` - all new `SlangCompiler*` test cases pass
5. **Runtime (Development):** Delete all `.spv` from `bin/Debug/assets/shaders/`. Launch `journey`. Engine logs show Slang runtime compilation for each shader. Rendering is visually identical to pre-compiled path.
6. **Runtime (pre-compiled):** Restore `.spv` files. Launch `journey`. Engine loads `.spv` as before - no runtime compilation logged.
7. **ReloadShaders:** While `journey` is running with source directory configured, call `ReloadShaders()` (wire to a debug key or ImGui button). All shaders recompile from source. No crash, rendering restores.
8. **Shipping:** `cmake --build --preset shipping` - compiles without Slang fallback code (`#if !defined(WAYFINDER_SHIPPING)` branch excluded). No `slang.dll` copied.
9. **Lint:** `tools/lint.py --changed` + `tools/tidy.py --changed` - clean
10. **Error reporting:** Create a `.slang` file with a syntax error, configure as source directory. Launch in Development. Engine logs show Slang error with file, line, and descriptive message. Shader falls back gracefully (pipeline creation fails but engine doesn't crash).

---

## Decisions

| Decision | Rationale |
|---|---|
| **Pimpl for SlangCompiler** | Keeps `slang.h` (large COM header) out of engine headers. Only `.cpp` files include it. Prevents Slang types from leaking to all engine consumers. |
| **PRIVATE linkage for Slang::slang** | Engine consumers don't need Slang headers - only `SlangCompiler.cpp` uses them directly. |
| **Fallback guarded by `WAYFINDER_SHIPPING`** | Shipping loads .spv only - guaranteed correct, no DLL dependency, no runtime compilation overhead. Future issue can add opt-in Shipping compilation for modding. |
| **SlangCompiler always compiled (no #ifdef)** | The class itself has no config-specific code. Only the ShaderManager fallback path is guarded. Simplifies testing - tests always have the compiler available. |
| **Lazy recompilation on ReloadShaders()** | `ReloadShaders()` only invalidates. Next `GetShader()` call triggers compilation. Avoids compiling unused shaders and keeps reload instant. |
| **Source directory via EngineConfig TOML** | Data-driven, not hardcoded. Development configs point to source tree; Shipping configs omit it. Matches engine principle "if it can be a file on disk, it should be." |
| **Result<CompileResult> return type** | Matches engine-wide error handling convention. Callers get structured success/failure, not just bool. |
| **Entry points VSMain/PSMain hardcoded** | Matches existing ShaderManager convention and all Slang shader files. No need for per-shader entry point configuration yet. |
| **Test fixtures in tests/fixtures/shaders/** | Decouples tests from engine shader source. Tests don't break when engine shaders change. |
| **Pipeline invalidation chain** | ReloadShaders must cascade through ShaderManager -> PipelineCache -> ShaderProgramRegistry -> RenderOrchestrator. Missing any link leaves stale GPU handles. |
| **Slang session created once at init** | `slang::ISession` is expensive to create (profile resolution, search path setup). One session for all compilations. Module caching is automatic within the session. |

---

## Further Considerations

1. **Thread safety of Slang session.** The Slang docs indicate `ISession::loadModule` is not thread-safe. Since all shader loading currently happens on the main thread, this is fine. If future work parallelises shader compilation, the session needs a mutex or per-thread sessions.

2. **Slang module caching within the session.** When `loadModule()` is called multiple times for modules that import the same shared modules (e.g., `transforms`), Slang caches the parsed module internally. No manual caching needed. However, `ReloadShaders()` must create a *new* session to force re-parsing of changed module source. Add session recreation to the reload flow.

3. **Compilation latency.** First shader load in Development will be slower (Slang compilation vs .spv file read). For the 8 current shaders, this is likely <1s total. If it becomes noticeable, a future optimisation could pre-warm the cache on a background thread during loading screens.
