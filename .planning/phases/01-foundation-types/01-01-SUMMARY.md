---
phase: 01-foundation-types
plan: 01
subsystem: gameplay
tags: [tag-system, rename, api-cleanup]

requires: []
provides:
  - "Tag/TagContainer/TagRegistry types (renamed from GameplayTag*)"
  - "TagContainer::AddTags bulk merge method"
  - "NativeTag in Wayfinder namespace"
  - "Tag construction restricted to TagRegistry and PluginRegistry (friend)"
affects: [02-tag-driven-state, all downstream consumers of Tag types]

tech-stack:
  added: []
  patterns:
    - "Registry-mediated tag construction - Tags created only via TagRegistry/PluginRegistry"

key-files:
  created: []
  modified:
    - engine/wayfinder/src/gameplay/Tag.h
    - engine/wayfinder/src/gameplay/Tag.cpp
    - engine/wayfinder/src/gameplay/TagRegistry.h
    - engine/wayfinder/src/gameplay/TagRegistry.cpp
    - engine/wayfinder/src/gameplay/NativeTag.h
    - engine/wayfinder/CMakeLists.txt
    - engine/wayfinder/src/gameplay/Game.h
    - engine/wayfinder/src/gameplay/Game.cpp
    - engine/wayfinder/src/gameplay/GameState.h
    - engine/wayfinder/src/gameplay/GameState.cpp
    - engine/wayfinder/src/gameplay/GameStateMachine.h
    - engine/wayfinder/src/scene/ComponentRegistry.cpp
    - engine/wayfinder/src/plugins/PluginRegistry.h
    - engine/wayfinder/src/plugins/PluginRegistry.cpp
    - engine/wayfinder/src/app/Subsystem.h
    - sandbox/journey/src/JourneyGame.cpp
    - tests/gameplay/TagTests.cpp
    - tests/gameplay/TagRegistryTests.cpp
    - tests/gameplay/GameStateMachineTests.cpp
    - tests/plugins/PluginRegistryTests.cpp
    - tests/core/InternedStringTests.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Added friend class Plugins::PluginRegistry to Tag for RegisterTag construction during Plugin::Build()"
  - "Kept JSON key 'gameplay_tags' in ComponentRegistry for data-file backward compatibility"

patterns-established:
  - "Registry-mediated construction: Tag's constructors are private, only TagRegistry and PluginRegistry can create valid tags"

requirements-completed: [CAP-01]

duration: ~25min
completed: 2026-04-04
---

# Phase 01 Plan 01: Rename GameplayTag to Tag - Summary

**Renamed entire gameplay tag system from GameplayTag/GameplayTagContainer/GameplayTagRegistry to Tag/TagContainer/TagRegistry, restricted tag construction to registries, added TagContainer::AddTags, and moved NativeTag into the Wayfinder namespace.**

## Performance

- **Completed:** 2026-04-04T05:26:06Z
- **Tasks:** 2/2
- **Files modified:** 22

## Accomplishments

- Renamed all 6 core types: Tag, TagContainer, ActiveTags, TagRegistry, TagDefinition, TagSourceKind
- Removed FromName/FromInterned from Tag public API - tags now constructed only via TagRegistry or PluginRegistry (friend)
- Added TagContainer::AddTags for bulk merging
- Moved NativeTag into Wayfinder namespace (was global)
- Updated all 10+ engine consumers, sandbox, and 5 test files
- All 4 test suites pass (render, core, scene, physics)

## Task Commits

Each task was committed atomically:

1. **Task 1: Rename core tag files and update public API** - `868e94c` (refactor)
2. **Task 2: Update all consumers, tests, and sandbox** - `e1139c9` (refactor)

## Files Created/Modified

- `engine/wayfinder/src/gameplay/Tag.h` - Renamed from GameplayTag.h; removed FromName/FromInterned; added friend TagRegistry and PluginRegistry
- `engine/wayfinder/src/gameplay/Tag.cpp` - Renamed from GameplayTag.cpp; Parent() uses private constructor
- `engine/wayfinder/src/gameplay/TagRegistry.h` - Renamed from GameplayTagRegistry.h; all types renamed
- `engine/wayfinder/src/gameplay/TagRegistry.cpp` - Renamed from GameplayTagRegistry.cpp; uses Tag private constructor
- `engine/wayfinder/src/gameplay/NativeTag.h` - Renamed from NativeGameplayTag.h; wrapped in Wayfinder namespace
- `engine/wayfinder/CMakeLists.txt` - Updated source file entries
- `engine/wayfinder/src/gameplay/Game.h` - Renamed AddGameplayTag/RemoveGameplayTag/HasGameplayTag to AddTag/RemoveTag/HasTag
- `engine/wayfinder/src/gameplay/Game.cpp` - Updated includes, subsystem registration to TagRegistry
- `engine/wayfinder/src/gameplay/GameState.h` - Updated include and type references
- `engine/wayfinder/src/gameplay/GameState.cpp` - Updated include and type references
- `engine/wayfinder/src/gameplay/GameStateMachine.h` - Updated include
- `engine/wayfinder/src/scene/ComponentRegistry.cpp` - Updated types; kept "gameplay_tags" data key
- `engine/wayfinder/src/plugins/PluginRegistry.h` - Updated return type to Tag
- `engine/wayfinder/src/plugins/PluginRegistry.cpp` - Uses Tag private constructor via friend
- `engine/wayfinder/src/app/Subsystem.h` - Updated doc comments
- `sandbox/journey/src/JourneyGame.cpp` - Updated include and comments
- `tests/gameplay/TagTests.cpp` - Rewritten with registry-based construction and AddTags tests
- `tests/gameplay/TagRegistryTests.cpp` - Renamed from GameplayTagRegistryTests.cpp
- `tests/gameplay/GameStateMachineTests.cpp` - Updated tag run condition tests with TagRegistry
- `tests/plugins/PluginRegistryTests.cpp` - Updated include
- `tests/core/InternedStringTests.cpp` - Updated test data strings
- `tests/CMakeLists.txt` - Updated test file entries

## Decisions Made

1. **PluginRegistry friend access** - Added `friend class Plugins::PluginRegistry` to Tag because PluginRegistry::RegisterTag needs to construct valid Tags during Plugin::Build(), before TagRegistry exists. This is a legitimate use case for friend access.
2. **Kept "gameplay_tags" JSON key** - ComponentRegistry still uses "gameplay_tags" as the JSON key name for scene files, preserving data-file backward compatibility. Only C++ types were renamed.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added friend class Plugins::PluginRegistry to Tag**
- **Found during:** Task 2 (updating PluginRegistry.cpp)
- **Issue:** PluginRegistry::RegisterTag() returned a Tag, but with FromName removed, it could no longer construct one. Tag's constructors are private, accessible only to friend TagRegistry.
- **Fix:** Added `friend class Plugins::PluginRegistry` and forward declaration to Tag.h. PluginRegistry.cpp now uses `Tag{tagName}` directly.
- **Files modified:** engine/wayfinder/src/gameplay/Tag.h, engine/wayfinder/src/plugins/PluginRegistry.cpp
- **Verification:** Build + all tests pass
- **Committed in:** e1139c9

---

**Total deviations:** 1 auto-fixed (Rule 3 - blocking issue)
**Impact on plan:** Necessary for compilation. PluginRegistry is a legitimate tag creator during plugin build phase.

## Issues Encountered

- **git mv on untracked file:** NativeGameplayTag.h was untracked, so `git mv` failed. Fixed by `git add` first, then `git mv`.
- **em-dash matching:** Some files contained UTF-8 em-dashes that caused replace_string_in_file failures. Resolved by using smaller match context.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None.

## Next Phase Readiness

- Tag system rename is complete and all tests pass
- Ready for Phase 01 Plan 02 and downstream phases
- No blockers

## Self-Check: PENDING

---
*Phase: 01-foundation-types*
*Completed: 2026-04-04*
