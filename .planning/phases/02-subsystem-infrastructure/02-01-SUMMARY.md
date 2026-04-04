---
phase: 02-subsystem-infrastructure
plan: 01
status: complete
started: 2026-04-04
completed: 2026-04-04
---

# Plan 02-01 Summary: SubsystemRegistry

## What Was Built

SubsystemRegistry<TBase> - a dependency-ordered, capability-gated subsystem registry template that replaces SubsystemCollection<TBase> for the v2 architecture.

### Key Features
- **Dependency ordering**: Kahn's topological sort ensures subsystems initialise in correct dependency order
- **Capability gating**: Subsystems with RequiredCapabilities are only activated when the effective CapabilitySet satisfies HasAll; empty caps always activate
- **Abstract-type resolution**: `Register<TConcrete, TAbstract>()` allows querying by both concrete and abstract types
- **Cycle detection**: Finalise() detects and reports cycles with readable path messages
- **Fail-fast initialisation**: First failure aborts init and reverse-shuts-down already-initialised subsystems
- **v2 Initialise signature**: AppSubsystem and StateSubsystem now have `Initialise(EngineContext&) -> Result<void>`

### Files

key-files:
  created:
    - engine/wayfinder/src/app/SubsystemRegistry.h
  modified:
    - engine/wayfinder/src/app/AppSubsystem.h
    - engine/wayfinder/src/app/StateSubsystem.h
    - engine/wayfinder/CMakeLists.txt
    - tests/app/SubsystemTests.cpp

## Deviations

- **Entry struct uses constructor instead of designated initialisers**: `std::type_index` lacks a default constructor, making `Entry` non-aggregate. Used constructor-style init instead. No functional impact.
- **`using Subsystem::Initialise;` added to base classes**: The v2 `Initialise(EngineContext&)` overload hides the v1 no-arg `Initialise()`. Added using-declaration to maintain backward compatibility with SubsystemCollection.
- **`insert_or_assign` for abstract redirect map**: `std::unordered_map::operator[]` requires default-constructible value type; `std::type_index` is not. Used `insert_or_assign` instead.

## Test Results

15 new SubsystemRegistry test cases added (107 assertions total across all suites):
- Topological init order, reverse shutdown order, three-node chain
- Two-node and three-node cycle detection
- Unregistered dependency detection
- Abstract-type resolution (Get by abstract and concrete)
- Capability gating (skip when unsatisfied, activate when satisfied, empty always activates)
- Fail-fast with reverse cleanup
- Deps<> helper, IsRegistered, IsFinalised, const Get
- Multiple independent subsystems

All existing SubsystemCollection tests (8 cases) pass unchanged.
Full test suite: 4/4 test executables pass with 0 regressions.

## Self-Check: PASSED
