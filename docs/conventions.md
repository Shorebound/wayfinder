# Naming and Formatting Conventions

Reference for issue titles, PR titles, PR bodies, branch names, and commits.

---

## Issue Titles

**Format:** Imperative verb phrase describing the target outcome.

| ✅ Good | ❌ Avoid |
|---|---|
| `Batched GPU Texture Uploads` | `Textures are slow` |
| `Replace ClearEntities Fallback Sweep with Authoritative Scene Entity Tracking` | `ClearEntities problem` |
| `Material-Owned Sampler State` | `Move samplers` |

**Rules:**
- Describe the *target state*, not the problem.
- No issue number, no prefix — labels carry domain and priority.
- Aim for under ~80 characters when possible, but don't sacrifice clarity.

---

## Issue Bodies

See [`docs/github_issues.md`](github_issues.md) for the full template.

**Standard structure:** Summary → Implementation Notes (optional) → Definition of Done

```markdown
## Summary
2-3 sentences: what the issue addresses and why it matters.

## Implementation Notes
Technical context, constraints, design decisions. Skip if the summary covers it.

## Definition of Done
- Specific, testable acceptance criteria.
```

**Sub-issues** use the lighter variant: Summary → Scope → Definition of Done.

---

## PR Titles

**Format:** `<type>(<scope>): <short imperative description>`

### Types

| Type | When to use |
|---|---|
| `feat` | New capability or system |
| `fix` | Bug fix |
| `refactor` | Restructuring without behaviour change |
| `test` | Test-only changes |
| `chore` | Build, deps, tooling, CI |
| `docs` | Documentation only |

### Scopes

Matches `domain:` labels: `rendering`, `core`, `scene`, `assets`, `tools`, `build`, `physics`, `audio`, `platform`.

### Examples

| ✅ Good |
|---|
| `feat(rendering): texture asset pipeline` |
| `refactor(core): replace polymorphic EventQueue with typed event buffers` |
| `fix(rendering): add missing #include <utility> in FrameAllocator.h` |
| `refactor(scene): scene system hardening` |

**Rules:**
- Lowercase after the colon.
- No period at the end.
- No `[WIP]` prefix — use GitHub's **draft PR** feature instead.
- `Closes #N` goes in the body, not the title.
- Keep under ~72 characters.

---

## PR Bodies

```markdown
## Summary

1-3 sentences: what this PR does and why.

Closes #<issue_number>

## Changes

### <Subsystem>
- Bullet points describing what changed.

### <Another subsystem>
- ...

## Testing

- What was tested and how.
```

Small PRs (< ~50 lines) can skip "Changes" and use Summary alone.

---

## Branch Names

**Format:** `<type>/<issue_number>-<short-kebab-slug>`

### Types

| Type | When to use |
|---|---|
| `feature/` | New capability |
| `fix/` | Bug fix |
| `refactor/` | Restructuring |
| `chore/` | Build, tooling, deps |

### Examples

| Issue | Branch |
|---|---|
| #32 Texture Asset Pipeline | `feature/32-texture-asset-pipeline` |
| #104 Scene System Hardening | `refactor/104-scene-system-hardening` |
| #69 Replace EventQueue | `refactor/69-typed-event-buffers` |
| #102 FrameAllocator missing include | `fix/102-frameallocator-missing-include` |

**Rules:**
- Always include the issue number.
- Slug: 3-5 words max, kebab-case.

---

## Commits

Same format as PR titles: `type(scope): description`

```
feat(rendering): add TextureManager
fix(core): handle null entity in GetComponent
```

---

## Quick Reference

| Artifact | Format | Example |
|---|---|---|
| Issue title | Imperative outcome | `Batched GPU Texture Uploads` |
| Issue body | Summary → Impl Notes → DoD | (per `docs/github_issues.md`) |
| PR title | `type(scope): description` | `feat(rendering): texture asset pipeline` |
| PR body | Summary → Changes → Testing | |
| Branch | `type/N-short-slug` | `feature/32-texture-asset-pipeline` |
| Commit | `type(scope): description` | `feat(rendering): add TextureManager` |
