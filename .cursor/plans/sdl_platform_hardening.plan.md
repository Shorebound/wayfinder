## Plan: SDL Platform Hardening

Refactor the SDL path by tightening contracts first, then centralising backend-global SDL ownership and frame pumping behind a small platform runtime while preserving the existing Window, Input, Time, and RenderDevice split. This keeps Null and headless support first-class, removes runtime-only void pointer coupling, and moves presentation policy to the render backend where it belongs.

**Steps**
1. Phase 1 - Contract and compatibility hardening. Add explicit backend compatibility rules and a headless query to the existing backend/config layer, allow SDL3 plus Null as a supported windowed no-GPU mode, reject Null plus SDL_GPU before any subsystem is created, and make config parsing accept explicit null platform selection. This blocks all later phases.
2. Phase 1 verification. Extend startup/config tests so valid combinations initialise cleanly and invalid combinations fail before window or device creation. This can run in parallel with implementation note drafting, but it must pass before the larger refactor continues.
3. Phase 2 - Introduce a focused platform runtime coordinator. Add a small backend runtime owned by EngineRuntime that owns backend-global lifecycle and frame pump responsibilities only. For SDL3 this means SDL_Init, SDL_Quit, event pumping, and per-frame backend synchronisation. For Null, it remains a no-op headless coordinator. This depends on step 1.
4. Phase 2 runtime extraction. Remove SDL_Init and SDL_Quit from SDL3Window, keep SDL3Window responsible only for SDL_Window ownership and window properties, and route frame pumping through the new platform runtime instead of Window::Update. This depends on step 3.
5. Phase 3 - Unify SDL event and input ownership. Move SDL event polling and input snapshot ordering under one frame-start contract so scroll, key transitions, and future text input live under one backend-owned flow. Keep the engine EventQueue and query-based Input API unless a failing test proves a public contract change is necessary. This depends on step 4.
6. Phase 3 API cleanup. Remove or reduce Window::Update once the new coordinator is in place, delete the Application-side scroll bridge, and ensure Application only consumes engine events instead of patching backend state. This depends on step 5.
7. Phase 4 - Replace raw native handles with typed surface access. Introduce a sealed platform surface handle or typed surface query, replace Window::GetNativeHandle, and update SDLGPUDevice::Initialise to consume explicit SDL surface data instead of casting a void pointer. This depends on step 1 and can proceed in parallel with step 5 once the platform runtime API is stable.
8. Phase 4 validation. Keep a defensive runtime check at the render boundary so an unexpected surface/backend mismatch still returns a clear Result error even though startup validation should already block it. This depends on step 7.
9. Phase 5 - Move presentation policy to the render side. Split window geometry and title settings from presentation settings, add a surface or presentation config owned by EngineConfig and consumed by RenderDevice::Initialise, and either implement fullscreen honestly via SDL state queries or remove it from the public contract until it is supported. This depends on step 7.
10. Phase 5 renderer integration. Update SDLGPUDevice to apply the new presentation policy, update NullDevice to ignore it explicitly, and remove or deprecate window-level VSync state that does not affect swapchain behaviour. This depends on step 9.
11. Parallel cleanup track - targeted SDL RAII hardening. Convert obvious scoped SDL ownership sites such as TextureAsset surface loading to move-only RAII immediately because they are low-risk correctness wins and independent of the backend contract work. Keep SDL3Window window-handle RAII as an implementation detail once platform-global SDL ownership has moved out of the window layer, but do not keep an SDL init or quit guard inside SDL3Window because that would preserve the wrong ownership model. In SDLGPUDevice, consider small move-only wrappers only for standalone owned resources such as the device-owned depth texture and dedicated or persistent transfer buffers after the lifecycle refactor is stable; keep resource pools and per-frame borrowed pointers on their current explicit teardown path. This can run in parallel with step 1 for TextureAsset and after step 4 for SDL3Window and SDLGPUDevice internals.
12. Phase 6 - Documentation and regression hardening. Update repo guidance with the new SDL ownership and backend compatibility rules, add tests for frame-order coherence and headless behaviour, and run focused app/render validation plus changed-file lint and tidy checks. This depends on step 10 and should also verify the targeted RAII cleanup from step 11.

**Relevant files**
- engine/wayfinder/src/app/EngineRuntime.h - runtime ownership and per-frame ordering; likely home for the new backend runtime member and startup sequencing changes.
- engine/wayfinder/src/app/EngineRuntime.cpp - current subsystem creation order, shutdown order, and BeginFrame flow that must be tightened.
- engine/wayfinder/src/app/Application.cpp - currently bridges scroll events back into Input; should become a consumer only once backend ownership is unified.
- engine/wayfinder/src/app/EngineConfig.h - current window/backend config structures that need a clearer split between window and presentation policy.
- engine/wayfinder/src/app/EngineConfig.cpp - backend parsing and validation entry point, including explicit null-platform parsing.
- engine/wayfinder/src/platform/BackendConfig.h - best place for compatibility helpers and headless queries unless a dedicated validator header is introduced.
- engine/wayfinder/src/platform/Window.h - current surface, VSync, fullscreen, and update contract that needs to be reduced to what the backend actually guarantees.
- engine/wayfinder/src/platform/Input.h - query API to preserve while cleaning up scroll and frame-start ownership.
- engine/wayfinder/src/platform/Time.h - only touch if time updates are folded into the new runtime-owned frame start contract.
- engine/wayfinder/src/platform/sdl3/SDL3Window.h - SDL3 window surface contract and state queries.
- engine/wayfinder/src/platform/sdl3/SDL3Window.cpp - currently owns SDL_Init, SDL_Quit, and SDL event pumping; main extraction target.
- engine/wayfinder/src/platform/sdl3/SDL3Input.h - SDL-specific input contract to preserve while removing cross-layer coupling.
- engine/wayfinder/src/platform/sdl3/SDL3Input.cpp - current keyboard/mouse polling and scroll accumulation logic that should move under a single backend frame contract.
- engine/wayfinder/src/platform/null/NullWindow.h - null surface contract for headless mode.
- engine/wayfinder/src/platform/null/NullInput.h - null input semantics that should stay deterministic after refactor.
- engine/wayfinder/src/platform/null/NullTime.h - headless frame-time behaviour to preserve.
- engine/wayfinder/src/rendering/backend/RenderDevice.h - render-device initialisation contract and the place to move presentation settings into the device side.
- engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.h - typed SDL surface consumption and backend-local presentation behaviour.
- engine/wayfinder/src/rendering/backend/sdl_gpu/SDLGPUDevice.cpp - currently casts the native handle and should become explicit about accepted surface types and presentation policy.
- engine/wayfinder/src/rendering/backend/null/NullDevice.h - explicit no-op handling for presentation config.
- tests/app/EngineConfigTests.cpp - backend parsing and compatibility coverage.
- tests/app/EngineRuntimeTests.cpp - startup, headless, invalid-pair, and frame-order tests.
- engine/wayfinder/CMakeLists.txt - add any new platform/runtime files because engine sources are listed explicitly.
- tests/CMakeLists.txt - register any new platform or app test files.
- .github/AGENTS.md - record the SDL ownership and surface-contract pitfall once the refactor settles.

**Verification**
1. Build and run the focused app and render test targets affected by the refactor:
   ```
   cmake --build --preset debug --target wayfinder_core_tests wayfinder_render_tests
   ctest --preset test -R "EngineRuntime|EngineConfig|NullDevice|SDLGPUDevice"
   ```
2. Run the full test pass through the existing presets once focused tests are green:
   ```
   cmake --build --preset debug
   ctest --preset test
   ```
3. Run lint and tidy on changed files, before considering the work complete:
   ```
   python tools/lint.py --changed
   python tools/tidy.py --changed
   ```
4. Manually validate three runtime modes: SDL3 plus SDL_GPU, SDL3 plus Null, and Null plus Null. Confirm startup, shutdown, resize or close handling where applicable, and that headless mode never touches SDL.
5. Manually confirm that presentation settings now affect the actual render path rather than only cached window state, and that fullscreen is either implemented truthfully or absent from the public contract.

**Decisions**
- Included scope: SDL lifecycle ownership, backend compatibility validation, input/event frame-order unification, typed surface access, presentation-policy relocation, tests, and repo guidance updates.
- Excluded scope: replacing the RenderDevice abstraction, adding new platform or render backends, rewriting renderer orchestration, or broad application or ECS architecture changes unrelated to the audited findings.
- Recommended architectural stance: introduce the smallest new coordinating type that solves backend-global ownership cleanly. Avoid a speculative mega-subsystem that swallows unrelated responsibilities.
- Recommended compatibility matrix: SDL3 plus SDL_GPU is the normal interactive path, SDL3 plus Null remains a supported no-GPU tooling path, Null plus Null is the supported headless path, Null plus SDL_GPU is invalid.

**Further considerations**
1. Prefer deprecating paper API surface quickly instead of keeping placeholder methods around. If fullscreen and window-level VSync are not implemented truthfully after phase 5, remove them rather than carrying misleading contracts.
2. If the typed surface handle can be expressed with existing engine primitives cleanly, use that. If not, a small dedicated tagged type is better than another generic void-pointer escape hatch.
3. Keep the public input query API stable unless coherence tests force a change. Most of the benefit comes from backend ownership and ordering, not from inventing a new input model.