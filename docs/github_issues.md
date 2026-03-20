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
.\tools\gh-issues\gh-issues.ps1 show 12
# Output:
# Issue #12 - P2.1: Scene Entity Index (O(1) lookup) [OPEN]
#   Blocked by:
#     [ ] #7 - P1.1: Test Coverage Expansion
```

Closed dependencies show `[x]`, open ones show `[ ]`.

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

## Workflow

When **completing a task**: close the issue and check if any issues it was blocking are now unblocked.

When **breaking down a large issue**: create new issues for the sub-tasks, then use `sub-issue` to link them to the parent.

When **starting a new task**: use `show` to check if it has unresolved blockers before beginning work.
