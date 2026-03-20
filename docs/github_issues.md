# GitHub Issue Management

How Wayfinder tracks work via GitHub Issues: labels, milestones, issue relationships, and the CLI tool for managing them.

Repository: `Shorebound/wayfinder`. Requires `gh` CLI authenticated with repo scope.

## Labels

### Priority

Labels express importance, not scheduling (milestones handle "when").

| Label | Meaning |
|-------|---------|
| `priority:critical` | Can't ship without it. Blocks correctness or architecture. |
| `priority:high` | Core capability with significant impact. Real cost to skipping. |
| `priority:medium` | Valuable but engine functions without it for now. |
| `priority:low` | Polish, experiments, future-proofing. |

### Difficulty
`difficulty:XS`, `difficulty:S`, `difficulty:M`, `difficulty:L`, `difficulty:XL`

### Domain
`domain:rendering`, `domain:core`, `domain:scene`, `domain:physics`, `domain:build`, `domain:audio`, `domain:platform`, `domain:assets`, `domain:tools`

### Workflow

| Label | Meaning |
|-------|---------|
| `agent-ready` | Well-specified enough for an AI agent to pick up |
| `blocked` | Cannot proceed until dependencies are resolved |

## Milestones

| Milestone | Description |
|-----------|-------------|
| Phase 1: Foundation | Core architecture, handles, renderer breakup |
| Phase 2: Core Engine | Physics, materials, camera, audio, input |
| Phase 3: Content Pipeline | Animation, prefabs, UI, streaming |
| Phase 4: Polish & Tooling | Lighting, editor, CI, scripting |
| Phase 5: Horizon | Networking, replay, save/load |

## Issue Relationships

GitHub has native **blocked-by / blocking** dependency relationships (same concept as Jira/Linear). These show up in the issue sidebar under "Relationships" and display "Blocked" badges on project boards.

GitHub also supports **sub-issues** (parent/child hierarchy) for breaking large tasks into smaller pieces.

## CLI Tool: `tools/gh-issues/gh-issues.ps1`

Manages issue relationships via the GitHub GraphQL API. Handles node ID lookups automatically — just pass issue numbers. Targets accept comma-separated lists (e.g. `7,8,9`).

```powershell
$tool = ".\tools\gh-issues\gh-issues.ps1"

# Add relationships
& $tool blocked-by 12 7         # #12 is blocked by #7
& $tool blocking 7 12,15        # #7 is blocking #12 and #15
& $tool sub-issue 10 41,42      # #41, #42 become sub-issues of #10

# Remove relationships
& $tool remove-blocked-by 12 7  # undo blocked-by
& $tool remove-blocking 7 12    # undo blocking
& $tool remove-sub-issue 10 41  # undo sub-issue

# Inspect
& $tool show 12                 # relationships, labels, blocker summary
& $tool show 12,15,20           # compact table for multiple issues
& $tool tree 10                 # sub-issue hierarchy with completion counts
& $tool chain 12                # walk blocked-by chain, find critical path

# Discovery (pass 0 as the issue number — it's ignored)
& $tool ready 0                                        # all open unblocked issues
& $tool status 0 -Milestone "Phase 1: Foundation"      # milestone progress
& $tool orphans 0                                       # issues with no parent or milestone
```

## GraphQL API Reference

For agents or scripts calling the API directly instead of using the CLI tool.

### Mutations

All relationship mutations follow the same shape. The input field names vary:

| Mutation | Input fields |
|----------|-------------|
| `addBlockedBy` | `issueId` (blocked), `blockingIssueId` (blocker) |
| `removeBlockedBy` | `issueId` (blocked), `blockingIssueId` (blocker) |
| `addSubIssue` | `issueId` (parent), `subIssueId` (child) |
| `removeSubIssue` | `issueId` (parent), `subIssueId` (child) |

```graphql
mutation {
  addBlockedBy(input: {
    issueId: "<NODE_ID_A>"
    blockingIssueId: "<NODE_ID_B>"
  }) { clientMutationId }
}
```

### Queries

```graphql
# Get node ID
query { repository(owner: "Shorebound", name: "wayfinder") {
  issue(number: 12) { id title }
}}

# Get all relationships
query { repository(owner: "Shorebound", name: "wayfinder") {
  issue(number: 12) {
    blockedBy(first: 50) { nodes { number title state } }
    blocking(first: 50)  { nodes { number title state } }
    subIssues(first: 50) { nodes { number title state } }
    parent { number title }
  }
}}
```

### `gh` CLI tip

GraphQL queries must go through a file to avoid PowerShell escaping issues:

```powershell
Set-Content -Path temp.graphql -Value 'mutation { ... }' -Encoding ascii -NoNewline
gh api graphql -F query=@temp.graphql
Remove-Item temp.graphql
```

## Issue Body Template

Issue bodies should be concise. Labels carry priority, difficulty, and domain. Sub-issues carry the task breakdown. The body provides the context that neither of these can.

### Structure

```markdown
## Summary

2-3 sentences: what the issue addresses and why it matters.
No metadata — labels handle priority, difficulty, and domain.

## Implementation Notes

Shared guidance that applies across sub-issues or that someone picking
this up needs to know. Technical constraints, design decisions, links
to relevant code or docs. Optional — skip if the summary says it all.

## Definition of Done

- Overarching acceptance criteria.
- For parent issues: "All sub-issues closed" plus any cross-cutting
  verification (e.g. "CI green", "no regressions in existing tests").
- For leaf issues: specific, testable conditions.

**Plan reference:** `docs/plans/<relevant_plan>.md`, section name.
```

### What goes where

| Information | Where it lives | Not in the body |
|---|---|---|
| Priority, difficulty, domain | Labels | ~~Metadata blocks~~ |
| Phase / scheduling | Milestone | ~~"Phase 2" headers~~ |
| Task breakdown | Sub-issues | ~~Checklists, tables of sub-tasks~~ |
| Dependencies | Blocked-by relationships | ~~"Depends on #X" prose~~ |
| Context and intent | **Issue body** | — |

### Variants

**Sub-issues** use a lighter structure (no plan reference — the parent carries it):

```markdown
## Summary         — one sentence: what this sub-issue delivers
## Scope           — what to build, what to test, key constraints
## Definition of Done
```

**Closed issues** that predate the template use a retrospective format:

```markdown
## Summary         — what the problem was
## What Was Done   — what was built or changed
**Status:** Done.
```

### Tips

- If an issue has sub-issues, the body is a **parent overview** — don't duplicate what the children already describe.
- Link to plan docs rather than copying content from them.
- Keep formatting flat. One or two heading levels is enough.

## Workflow

When **starting a task**: use `show` to check for unresolved blockers. If blocked, use `chain` to find what needs doing first.

When **picking what to work on**: use `ready 0` to see all unblocked issues, or `status 0 -Milestone "..."` for milestone-scoped priorities.

When **completing a task**: close the issue and check if any issues it was blocking are now unblocked.

When **breaking down a large issue**: create new issues for the sub-tasks, then use `sub-issue` to link them to the parent. Use `tree` to verify the hierarchy.

When **triaging**: use `orphans 0` to find issues that slipped through without a parent or milestone.
