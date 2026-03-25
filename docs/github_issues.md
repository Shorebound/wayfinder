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

## CLI Tool: `gh-issues`

Python script (`tools/gh-issues.py`). No build step required — just needs Python 3.10+ and `gh` CLI authenticated with repo scope. Manages issue relationships via the GitHub GraphQL API. Handles node ID lookups automatically — just pass issue numbers. Targets accept comma-separated lists (e.g. `7,8,9`).

```bash
# Add relationships
python tools/gh-issues.py blocked-by 12 7         # #12 is blocked by #7
python tools/gh-issues.py blocking 7 12,15        # #7 is blocking #12 and #15
python tools/gh-issues.py sub-issue 10 41,42      # #41, #42 become sub-issues of #10

# Remove relationships
python tools/gh-issues.py remove-blocked-by 12 7  # undo blocked-by
python tools/gh-issues.py remove-blocking 7 12    # undo blocking
python tools/gh-issues.py remove-sub-issue 10 41  # undo sub-issue

# Inspect
python tools/gh-issues.py show 12                 # relationships, labels, blocker summary
python tools/gh-issues.py show 12,15,20           # compact table for multiple issues
python tools/gh-issues.py tree 10                 # sub-issue hierarchy with completion counts
python tools/gh-issues.py chain 12                # walk blocked-by chain, find critical path

# Discovery
python tools/gh-issues.py ready                                        # all open unblocked issues
python tools/gh-issues.py status --milestone "Phase 1: Foundation"     # milestone progress
python tools/gh-issues.py orphans                                      # issues with no parent or milestone
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

GraphQL queries must go through a file to avoid shell escaping issues:

```bash
echo 'mutation { ... }' > /tmp/temp.graphql
gh api graphql -F query=@/tmp/temp.graphql
rm /tmp/temp.graphql
```

The `tools/gh-issues.py` script handles this automatically.

## Workflow

- Before starting: check `python tools/gh-issues.py show <N>` for unresolved blockers.
- On completion: close the issue, check if anything it was blocking
  is now unblocked.
- Use `Closes #N` in the PR body to auto-close issues on merge.
- Metadata (priority, difficulty, domain) goes on labels, not in
  issue bodies.
- Task breakdowns go in sub-issues, not checklists.
- Dependencies go in blocked-by relationships, not prose.
