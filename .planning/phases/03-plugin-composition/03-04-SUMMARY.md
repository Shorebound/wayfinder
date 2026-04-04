# Plan 03-04 Summary: ConfigService, ConfigRegistrar, 3-Tier TOML Layering

## Outcome
Created the per-plugin configuration system replacing monolithic EngineConfig with typed, per-plugin config loading.

## What Was Built

### ConfigRegistrar (engine/wayfinder/src/app/ConfigRegistrar.h/.cpp)
- Build-time registrar tracking declared config types and TOML file keys
- `DeclareConfig(key, type)` records plugin config declarations
- `LoadTable(key, configDir, savedDir)` implements 3-tier TOML layering with caching
- Shallow-recursive merge: values overridden at each level, sub-tables merged recursively

### ConfigService (engine/wayfinder/src/app/ConfigService.h/.cpp)
- AppSubsystem providing address-stable, typed config storage at runtime
- `Store<T>()`, `Get<T>()`, `TryGet<T>()`, `Has<T>()` with type-erased void* storage
- Pointer stability guaranteed across multiple Store calls

### AppBuilder::LoadConfig<T>(key)
- Plugin-facing API for loading per-plugin configs from TOML
- 3-tier layering: struct defaults < config/<key>.toml < saved/config/<key>.toml
- Cached: multiple calls with the same key parse once
- Falls back to T{} defaults when project paths not set or files missing

### OnConfigReloaded() Stub
- Added `virtual void OnConfigReloaded() {}` to AppSubsystem and StateSubsystem
- Marked @prototype for future file-watcher integration

## Commits
- 984e096: feat(03-04): ConfigRegistrar, ConfigService, and AppBuilder::LoadConfig
- 0da6803: test(03-04): ConfigService tests, OnConfigReloaded stubs, TOML fixtures

## Tests Added
- 16 new test cases in ConfigServiceTests.cpp (42 assertions)
- ConfigRegistrar: DeclareConfig, LoadTable caching, TOML parsing
- ConfigRegistrar 3-Tier Layering: Layer 2 only, Layer 2+3 merge, Layer 1 defaults
- ConfigService: Store/Get round-trip, TryGet nullptr, address stability, Shutdown
- AppBuilder LoadConfig: valid paths, empty paths, caching, layered overrides
- OnConfigReloaded Stub: default no-crash, custom override fires

## Test Count
374 tests (all passing)
