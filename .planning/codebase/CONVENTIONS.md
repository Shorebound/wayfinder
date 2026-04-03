# Code Conventions

**Last mapped:** 2026-04-03
**Focus areas:** core, app, plugins, gameplay/simulation

---

## Formatting

Enforced by `.clang-format` (LLVM base with overrides):

| Rule | Setting |
|------|---------|
| Indent | 4 spaces, no tabs |
| Column limit | 220 |
| Pointer alignment | Left (`T*`, `T&`) |
| Qualifier alignment | Left (west const: `const T`) |
| Braces | Allman style (after function, class, namespace, control) |
| Lambda body | OuterScope indentation, brace before body |
| Single-line functions | Only if empty body |
| Namespace indentation | All |

```cpp
void MyFunction()
{
    // body
}

auto lambda = [](int x)
{
    return x * 2;
};
```

---

## Naming

| Concept | Convention | Examples |
|---------|-----------|---------|
| Types/Classes/Structs | PascalCase | `RenderDevice`, `GameplayTag`, `SceneDocument` |
| Interfaces | `I` prefix + PascalCase | `IRenderer`, `ILogger`, `IRenderPass` |
| Functions/Methods | PascalCase, verb prefix | `CreateEntity()`, `HasComponent()`, `GetDeltaTime()` |
| Query patterns | `IsX`, `HasX`, `WasX` | `IsValid()`, `HasTag()`, `IsChildOf()` |
| Accessors | `GetX`, `SetX` | `GetName()`, `SetRunning()` |
| Factories | `CreateX`, `DestroyX`, `LoadX`, `SaveX` | `CreateBuffer()`, `LoadScene()` |
| Private members | `m_` prefix + camelCase | `m_world`, `m_generation`, `m_cached` |
| Public/struct fields | PascalCase | `Position`, `Width`, `Title`, `Enabled` |
| Constants | SCREAMING_SNAKE_CASE | `SPIRV_MAGIC`, `INDEX_ELEMENT_SIZE` |
| Local variables | camelCase | `pixelData`, `assetId`, `registry` |
| Template params | `T` prefix + PascalCase | `TError`, `TAllocator`, `TRenderPass` |
| Namespaces | PascalCase | `Wayfinder::Rendering`, `Wayfinder::Scene` |
| Enum values | PascalCase (except abbreviations) | `PixelFormat::RGBA`, `GameState::Ready` |

### Spelling

British/Australian throughout: `Initialise`, `Colour`, `Serialise`, `Behaviour`, `Normalise`.

---

## C++ Style Rules

### Headers
- `#pragma once` (not `#ifndef` guards)
- Minimise includes, forward-declare where possible
- Include order: local engine headers, platform headers, standard library, third-party

### Modern C++
- `auto` when type is clear from RHS; spell explicitly when it isn't
- Trailing return types: `auto Foo(args) -> ReturnType`
- `and`, `or`, `not` over `&&`, `||`, `!`
- West const: `const T`, `const auto*`
- `enum class` over `enum`; `std::to_underlying()` over `static_cast<int>`
- `std::optional<T>` over sentinels or null pointers
- `using namespace` only for literals: `using namespace std::chrono_literals`
- No em-dashes (U+2014) in code, strings, or comments - use ` - `, `--`, or reword

### Error Handling
- `Result<T, E>` (`core/Result.h`) for recoverable failures
- `MakeError("message")` for error construction
- Pattern: check `.has_value()`, access `.value()` or `.error()`
- `[[nodiscard]]` on functions returning resources, handles, or values caller must inspect
- Exceptions only for truly unrecoverable situations; hot paths never throw

```cpp
auto LoadTexture(std::string_view path) -> Result<TextureHandle>;

auto result = compiler.Initialise(desc);
if (not result.has_value())
{
    Log::Error(LogEngine, "Init failed: {}", result.error());
    return MakeError("Initialisation failed");
}
```

### Resource Management
- RAII everywhere; every dynamic resource owned by a destructor
- `std::unique_ptr` or custom RAII wrappers
- Non-owning access: `std::span<T>` (contiguous) or references
- Never transfer ownership via raw pointer
- `noexcept` on move ctors/assignment; `= delete` copy on move-only types

### Logging
- Category-based via `Wayfinder::Log` functions
- Categories: `LogEngine`, `LogRenderer`, `LogInput`, `LogAudio`, `LogAssets`, `LogPhysics`, `LogGame`, `LogScene`
- Compile-time format validation via `std::format_string`

```cpp
Log::Info(LogEngine, "Entity created: {}", entityId);
Log::Warn(LogRenderer, "Frame took {:.2f}ms", deltaMs);
Log::Error(LogAssets, "Failed to load: {}", path);
```

### Concepts & Templates
- `requires` clauses over SFINAE
- Named concepts when reused; inline `requires` otherwise
- `constexpr` for compile-time-capable values/functions
- `static_assert` for layout size, trivial-type guarantees

### Comments
- `/** ... */` Javadoc-style for public API (`@brief`, `@param`, `@return`, `@todo`)
- `///` for inline implementation notes
- `@prototype` for scaffolding with replacement plan
- `@todo` for planned follow-up (link `#nnn` when issue exists)
- `@fixme` for incorrect or fragile code

---

## Static Analysis

### clang-tidy (`.clang-tidy`)

**Enabled categories:**
- `bugprone-*` - bug detection
- `clang-analyzer-*` - static analysis
- `cppcoreguidelines-*` (selective) - Core Guidelines
- `misc-*` (selective) - miscellaneous
- `modernize-*` (except `use-trailing-return-type`) - modernisation
- `performance-*` - performance
- `readability-*` (selective) - clarity

**Key rules:** max 6 function parameters, no magic numbers in production code.

**Test override (`tests/.clang-tidy`):** Relaxes `exception-escape`, `use-after-move`, disables `modernize-*`, `performance-*`, `readability-*` for test flexibility.

### Lint Pipeline

```bash
python tools/lint.py --changed    # Format + banned includes (changed files)
python tools/lint.py --fix        # Auto-fix via clang-format + format-fixup.py
python tools/tidy.py --changed    # Static analysis (changed files)
```

**Banned includes:** Direct `#include <flecs.h>` must use `"ecs/Flecs.h"` wrapper.

---

## Commit & Branch Conventions

### Branches

`<scope>/<issue_number>-<short-kebab-slug>`

### Commits & PR Titles

`<type>(<scope>): <description>`

| Type | Usage |
|------|-------|
| `feat` | New feature |
| `fix` | Bug fix |
| `refactor` | Code restructuring |
| `test` | Test additions/changes |
| `chore` | Build, tooling, deps |
| `docs` | Documentation |
| `all` | Cross-cutting |

| Scope | Domain |
|-------|--------|
| `rendering` | Render pipeline |
| `core` | Core primitives |
| `scene` | Scene/ECS |
| `assets` | Asset pipeline |
| `tools` | CLI tools |
| `build` | Build system |
| `physics` | Physics |
| `audio` | Audio |
| `platform` | Platform abstraction |
