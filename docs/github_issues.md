# GitHub Issue Management

How Wayfinder tracks work via GitHub Issues: labels, milestones, issue relationships, and the CLI tool for managing them.

Repository: `Shorebound/wayfinder`. Requires `gh` CLI authenticated with repo scope.

## Labels

### Priority

| Label | Meaning |
|-------|---------|
| `P1-must-do` | Foundation work, unblocks everything else |
| `P2-should-do` | Core engine systems |
| `P3-nice-to-have` | Important but not blocking |
| `P4-horizon` | Future / aspirational |

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

A PowerShell script that manages issue relationships via the GitHub GraphQL API. Handles node ID lookups automatically - you just pass issue numbers.

### Blocked-by (issue depends on another)

```powershell
# Issue #12 is blocked by #7
.\tools\gh-issues\gh-issues.ps1 blocked-by 12 7

# Issue #12 is blocked by multiple issues
.\tools\gh-issues\gh-issues.ps1 blocked-by 12 7,8,9
```

### Blocking (issue prevents another from proceeding)

```powershell
# Issue #7 is blocking #12 and #15
.\tools\gh-issues\gh-issues.ps1 blocking 7 12,15
```

### Sub-issues

```powershell
# Issues #41, #42 become sub-issues of #10
.\tools\gh-issues\gh-issues.ps1 sub-issue 10 41,42
```

### Remove relationships

```powershell
# Remove: #12 is no longer blocked by #7
.\tools\gh-issues\gh-issues.ps1 remove-blocked-by 12 7

# Remove: #7 no longer blocking #12
.\tools\gh-issues\gh-issues.ps1 remove-blocking 7 12

# Remove: #41 is no longer a sub-issue of #10
.\tools\gh-issues\gh-issues.ps1 remove-sub-issue 10 41
```

### Show relationships

```powershell
# Detailed view with completion status, labels, assignees, and blocker summary
.\tools\gh-issues\gh-issues.ps1 show 12

# Compact table for comparing multiple issues
.\tools\gh-issues\gh-issues.ps1 show 12,15,20
```

### Sub-issue tree

```powershell
# Two-level hierarchy with completion counts
.\tools\gh-issues\gh-issues.ps1 tree 10
```

### Dependency chain

```powershell
# Walk the full blocked-by chain recursively, highlights the critical path
.\tools\gh-issues\gh-issues.ps1 chain 12
```

### Ready issues

```powershell
# List all open issues with no unresolved blockers
.\tools\gh-issues\gh-issues.ps1 ready 0
```

### Milestone status

```powershell
# Progress bar and issue breakdown for a milestone
.\tools\gh-issues\gh-issues.ps1 status 0 -Milestone "Phase 1: Foundation"
```

### Orphaned issues

```powershell
# Find open issues with no parent and no milestone
.\tools\gh-issues\gh-issues.ps1 orphans 0
```

For `ready`, `status`, and `orphans`, pass `0` as the issue number (it's ignored).

## GraphQL API Reference

For agents or scripts that need to call the API directly instead of using the tool.

### Mutations

**Add blocked-by:**
```graphql
mutation {
  addBlockedBy(input: {
    issueId: "<NODE_ID_OF_BLOCKED_ISSUE>"
    blockingIssueId: "<NODE_ID_OF_BLOCKING_ISSUE>"
  }) { clientMutationId }
}
```

**Remove blocked-by:**
```graphql
mutation {
  removeBlockedBy(input: {
    issueId: "<NODE_ID_OF_BLOCKED_ISSUE>"
    blockingIssueId: "<NODE_ID_OF_BLOCKING_ISSUE>"
  }) { clientMutationId }
}
```

**Add sub-issue:**
```graphql
mutation {
  addSubIssue(input: {
    issueId: "<NODE_ID_OF_PARENT>"
    subIssueId: "<NODE_ID_OF_CHILD>"
  }) { clientMutationId }
}
```

**Remove sub-issue:**
```graphql
mutation {
  removeSubIssue(input: {
    issueId: "<NODE_ID_OF_PARENT>"
    subIssueId: "<NODE_ID_OF_CHILD>"
  }) { clientMutationId }
}
```

### Queries

**Get an issue's node ID:**
```graphql
query {
  repository(owner: "Shorebound", name: "wayfinder") {
    issue(number: 12) { id title }
  }
}
```

**Query all relationships for an issue:**
```graphql
query {
  repository(owner: "Shorebound", name: "wayfinder") {
    issue(number: 12) {
      blockedBy(first: 50) { nodes { number title state } }
      blocking(first: 50)  { nodes { number title state } }
      subIssues(first: 50) { nodes { number title state } }
      parent { number title }
    }
  }
}
```

### Working with the `gh` CLI

GraphQL queries must be passed via file to avoid PowerShell escaping issues:

```powershell
Set-Content -Path temp.graphql -Value 'mutation { addBlockedBy(input: { issueId: "NODE_ID", blockingIssueId: "NODE_ID" }) { clientMutationId } }' -Encoding ascii -NoNewline
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

**Sub-issues** use a lighter structure — no plan reference (the parent carries it), and "Scope" instead of "Implementation Notes" since they describe a single concrete piece of work:

```markdown
## Summary

One sentence: what this sub-issue delivers.

## Scope

What to build, what to test, key constraints.

## Definition of Done

- Specific, testable conditions for this piece.
```

**Closed issues** that were completed before the template existed use a retrospective format:

```markdown
## Summary

What the problem was.

## What Was Done

What was built or changed to solve it.

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
