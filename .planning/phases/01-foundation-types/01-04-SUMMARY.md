---
phase: 01-foundation-types
plan: 04
subsystem: plugins
tags: [dll-removal, plugin-system, static-linking, cleanup]

requires: []
provides:
  - "Clean plugin system without DLL loading overhead"
  - "Explicit main() entry points in all sandbox apps"
  - "Simplified build without shared library targets"
affects: [app-shell, sandbox, tools]

tech-stack:
  added: []
  patterns:
    - "Direct plugin construction in main() instead of factory functions"
    - "Static linking only - no DLL boundary concerns"

key-files:
  created: []
  modified:
    - "engine/wayfinder/src/plugins/Plugin.h"
    - "engine/wayfinder/src/plugins/PluginRegistry.h"
    - "engine/wayfinder/CMakeLists.txt"
    - "sandbox/journey/src/JourneyGame.cpp"
    - "sandbox/journey/CMakeLists.txt"
    - "sandbox/journey/project.wayfinder"
    - "sandbox/waystone/src/WaystoneApplication.cpp"
    - "tools/waypoint/src/WaypointMain.cpp"
    - ".github/AGENTS.md"
    - "CMakeLists.txt"

key-decisions:
  - "Always use RegisterDefaultScenePlugins in waypoint (no plugin registry path) since DLL loading is gone"
  - "Removed journey_plugin shared library CMake target as part of DLL system cleanup"
  - "Cleared plugin field from project.wayfinder since it referenced the deleted DLL"

patterns-established:
  - "Explicit main(): sandbox apps construct their root plugin and pass it to Application directly"

requirements-completed: [PLUG-07]

duration: 8min
completed: 2026-04-04
---

# Phase 01 Plan 04: DLL Plugin System Removal Summary

**Removed dead-end DLL plugin system (4 files deleted, 3 consumers updated, shared lib target removed) - all targets compile, all tests pass.**

## Performance

- **Duration:** 8 min
- **Started:** 2026-04-04T06:25:00Z
- **Completed:** 2026-04-04T06:33:00Z
- **Tasks:** 2/2
- **Files modified:** 10

## Accomplishments

- Deleted PluginExport.h, PluginLoader.h/.cpp, and EntryPoint.h from the engine
- Removed CreateGamePlugin() declaration from Plugin.h
- Replaced macro-based entry points (WAYFINDER_IMPLEMENT_GAME_PLUGIN) with explicit main() in journey and waystone
- Removed DLL loading codepath from waypoint CLI, replaced with warning log
- Removed journey_plugin shared library CMake target
- Updated AGENTS.md to reflect the new direct-construction architecture

## Task Commits

Each task was committed atomically:

1. **Task 1: Delete DLL files and remove CreateGamePlugin from Plugin.h** - `476b9ce` (refactor)
2. **Task 2: Update sandbox apps and waypoint tool to use explicit main()** - `a3fdbce` (refactor)

**Plan metadata:** pending (docs: complete plan)

## Files Created/Modified

- `engine/wayfinder/src/plugins/Plugin.h` - Removed CreateGamePlugin declaration and unused memory include
- `engine/wayfinder/src/plugins/PluginRegistry.h` - Cleaned stale CreateGamePlugin comment
- `engine/wayfinder/CMakeLists.txt` - Removed 4 deleted files from source list
- `sandbox/journey/src/JourneyGame.cpp` - Replaced EntryPoint.h/PluginExport.h includes with Application.h, added explicit main()
- `sandbox/journey/CMakeLists.txt` - Removed journey_plugin shared library target
- `sandbox/journey/project.wayfinder` - Removed plugin field
- `sandbox/waystone/src/WaystoneApplication.cpp` - Replaced EntryPoint.h with Application.h, added explicit main()
- `tools/waypoint/src/WaypointMain.cpp` - Removed PluginLoader include, DLL loading codepath, GamePlugin member; added warning log
- `.github/AGENTS.md` - Updated plugin system guidance to reflect direct construction
- `CMakeLists.txt` - Cleaned DLL-related comment

## Decisions Made

- Removed journey_plugin shared library target (deviation Rule 2 - it was part of the DLL system being removed)
- Cleared plugin field from project.wayfinder (would reference nonexistent DLL)
- Waypoint always uses RegisterDefaultScenePlugins fallback now (simplifies code, no plugin registry needed)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing cleanup] Removed journey_plugin shared library CMake target**
- **Found during:** Task 2
- **Issue:** journey/CMakeLists.txt had a SHARED library target for DLL plugin loading that would still build
- **Fix:** Removed the shared library target definition (6 lines)
- **Files modified:** sandbox/journey/CMakeLists.txt
- **Verification:** cmake --preset dev + cmake --build --preset debug
- **Committed in:** a3fdbce (part of Task 2 commit)

**2. [Rule 2 - Missing cleanup] Cleared plugin reference from project.wayfinder**
- **Found during:** Task 2
- **Issue:** project.wayfinder referenced "journey_plugin" DLL that no longer exists
- **Fix:** Removed plugin = "journey_plugin" line
- **Files modified:** sandbox/journey/project.wayfinder
- **Verification:** Build and tests pass
- **Committed in:** a3fdbce (part of Task 2 commit)

**3. [Rule 2 - Missing cleanup] Cleaned stale comment in PluginRegistry.h**
- **Found during:** Task 2 (stale reference verification)
- **Issue:** Comment referenced CreateGamePlugin() which no longer exists
- **Fix:** Updated comment to generic description
- **Files modified:** engine/wayfinder/src/plugins/PluginRegistry.h
- **Committed in:** a3fdbce (part of Task 2 commit)

**4. [Rule 2 - Missing cleanup] Cleaned DLL comment in root CMakeLists.txt**
- **Found during:** Task 2
- **Issue:** Comment about DLL boundary with journey_plugin.dll was stale
- **Fix:** Simplified comment
- **Files modified:** CMakeLists.txt
- **Committed in:** a3fdbce (part of Task 2 commit)

---

**Total deviations:** 4 auto-fixed (all Rule 2 - missing cleanup)
**Impact on plan:** All auto-fixes necessary for completeness of DLL system removal. No scope creep.

## Issues Encountered

None - plan executed cleanly.

## Known Stubs

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

All 4 plans of Phase 01 are now complete. The foundation types (Tag rename, StateMachine, DLL removal) are in place for Phase 02 work.

## Self-Check: PASSED

---
*Phase: 01-foundation-types*
*Completed: 2026-04-04*
