#!/usr/bin/env python3
"""gh-issues — GitHub issue relationship manager for Wayfinder.

A standalone CLI tool that manages GitHub issue dependencies (blocked-by,
sub-issues) using the gh CLI and GitHub's GraphQL API.

Requires: gh CLI authenticated with repo scope.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from collections.abc import Callable
from dataclasses import dataclass, field

# --- Constants ---

OWNER = "Shorebound"
REPO = "wayfinder"
MAX_RECURSION_DEPTH = 25

# --- ANSI Colours ---

_colour_enabled: bool | None = None


def _colours_enabled() -> bool:
    global _colour_enabled
    if _colour_enabled is None:
        if os.environ.get("NO_COLOR"):
            _colour_enabled = False
        elif not sys.stdout.isatty():
            _colour_enabled = False
        else:
            _colour_enabled = True
            # Enable ANSI on Windows
            if sys.platform == "win32":
                try:
                    import ctypes

                    kernel32 = ctypes.windll.kernel32  # type: ignore[attr-defined]
                    handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
                    mode = ctypes.c_ulong()
                    kernel32.GetConsoleMode(handle, ctypes.byref(mode))
                    kernel32.SetConsoleMode(
                        handle, mode.value | 0x0004
                    )  # ENABLE_VIRTUAL_TERMINAL_PROCESSING
                except Exception:
                    pass
    return _colour_enabled


class Colour:
    """ANSI colour codes."""

    RESET = "\033[0m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"
    GRAY = "\033[90m"


def c(colour: str) -> str:
    """Return the colour code if colours are enabled, otherwise empty string."""
    return colour if _colours_enabled() else ""


# --- Helpers ---


def pad_right(s: str, width: int) -> str:
    """Pad a string to a minimum width with trailing spaces."""
    return s.ljust(width)


@dataclass
class GhResult:
    """Result from a gh CLI command."""

    data: dict | list | None = None
    error: str = ""

    @property
    def ok(self) -> bool:
        return not self.error


def run_gh(args: str, context: str = "gh") -> GhResult:
    """Run a gh CLI command and parse the output as JSON."""
    try:
        proc = subprocess.run(
            f"gh {args}",
            shell=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError:
        return GhResult(error=f"{context}: gh CLI not found. Install from https://cli.github.com/")

    if proc.returncode != 0:
        output = (proc.stderr or proc.stdout).strip()
        return GhResult(error=f"{context}: gh exited with code {proc.returncode} — {output}")

    try:
        return GhResult(data=json.loads(proc.stdout))
    except json.JSONDecodeError as e:
        return GhResult(error=f"{context}: Failed to parse JSON — {e}")


def run_graphql(query: str, context: str = "GraphQL") -> GhResult:
    """Send a GraphQL query via gh api and return parsed JSON."""
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".graphql", prefix="gh-issues-", delete=False
    ) as f:
        f.write(query)
        temp_path = f.name

    try:
        result = run_gh(f"api graphql -F query=@{temp_path}", context)
    finally:
        os.unlink(temp_path)
    return result


def run_mutation(mutation: str) -> GhResult:
    """Execute a GraphQL mutation and return the result."""
    return run_graphql(mutation, "GraphQL mutation")


# --- Data Types ---


@dataclass
class IssueInfo:
    node_id: str = ""
    title: str = ""


@dataclass
class TreeNode:
    number: int = 0
    title: str = ""
    state: str = ""
    children: list[TreeNode] = field(default_factory=list)


@dataclass
class ChainEntry:
    number: int = 0
    title: str = ""
    state: str = ""
    depth: int = 0


# --- Issue ID Resolution ---


def get_issue_node_ids(numbers: set[int]) -> dict[int, IssueInfo] | None:
    """Batch-fetch node IDs and titles for a set of issue numbers.

    Returns a dict of issue_number -> IssueInfo, or None on error.
    """
    fields = " ".join(
        f'i{num}: issue(number: {num}) {{ id title }}' for num in sorted(numbers)
    )
    query = (
        f'query {{ repository(owner: "{OWNER}", name: "{REPO}") {{ {fields} }} }}'
    )

    result = run_graphql(query, "GetIssueNodeIds")
    if not result.ok:
        print(result.error, file=sys.stderr)
        return None

    data = result.data
    if "errors" in data:
        print(f"GraphQL error: {data['errors'][0]['message']}", file=sys.stderr)
        return None

    issue_map: dict[int, IssueInfo] = {}
    repo = data["data"]["repository"]
    for num in numbers:
        key = f"i{num}"
        if key not in repo or repo[key] is None:
            print(
                f"Issue #{num} not found in repository {OWNER}/{REPO}",
                file=sys.stderr,
            )
            return None
        issue_map[num] = IssueInfo(
            node_id=repo[key]["id"],
            title=repo[key]["title"],
        )
    return issue_map


# --- Relationship Mutations ---


def _do_relationship_action(
    *,
    action_name: str,
    primary_issue: int,
    target_issues: list[int],
    primary_label: str,
    target_label: str,
    mutation_name: str,
    build_mutation: Callable[[str, str], str],
    ok_verb: str,
) -> None:
    """Generic helper for relationship add/remove actions."""
    all_nums = {primary_issue} | set(target_issues)
    ids = get_issue_node_ids(all_nums)
    if ids is None:
        return

    primary_info = ids[primary_issue]
    print(
        f"{c(Colour.CYAN)}{primary_label} #{primary_issue} ({primary_info.title}){c(Colour.RESET)}"
    )

    for target in target_issues:
        target_info = ids[target]
        mutation = build_mutation(primary_info.node_id, target_info.node_id)

        result = run_mutation(mutation)
        if not result.ok:
            print(
                f"{c(Colour.RED)}  FAIL  {target_label} #{target} ({target_info.title}) - {result.error}{c(Colour.RESET)}"
            )
            continue

        if "errors" in result.data:
            msg = result.data["errors"][0]["message"]
            if "already" in msg.lower():
                print(
                    f"{c(Colour.GRAY)}  SKIP  {target_label} #{target} ({target_info.title}) - already exists{c(Colour.RESET)}"
                )
            else:
                print(
                    f"{c(Colour.RED)}  FAIL  {target_label} #{target} ({target_info.title}) - {msg}{c(Colour.RESET)}"
                )
        else:
            print(
                f"{c(Colour.GREEN)}  OK    {ok_verb} #{target} ({target_info.title}){c(Colour.RESET)}"
            )


def add_blocked_by(blocked_issue: int, blocking_issues: list[int]) -> None:
    _do_relationship_action(
        action_name="blocked-by",
        primary_issue=blocked_issue,
        target_issues=blocking_issues,
        primary_label="Issue",
        target_label="blocked by",
        mutation_name="addBlockedBy",
        build_mutation=lambda primary_id, target_id: (
            f'mutation {{ addBlockedBy(input: {{ issueId: "{primary_id}", '
            f'blockingIssueId: "{target_id}" }}) {{ clientMutationId }} }}'
        ),
        ok_verb="blocked by",
    )


def add_blocking(blocking_issue: int, blocked_issues: list[int]) -> None:
    all_nums = {blocking_issue} | set(blocked_issues)
    ids = get_issue_node_ids(all_nums)
    if ids is None:
        return

    blocking_info = ids[blocking_issue]
    print(
        f"{c(Colour.CYAN)}Issue #{blocking_issue} ({blocking_info.title}) now blocking:{c(Colour.RESET)}"
    )

    for blocked in blocked_issues:
        blocked_info = ids[blocked]
        mutation = (
            f'mutation {{ addBlockedBy(input: {{ issueId: "{blocked_info.node_id}", '
            f'blockingIssueId: "{blocking_info.node_id}" }}) {{ clientMutationId }} }}'
        )

        result = run_mutation(mutation)
        if not result.ok:
            print(
                f"{c(Colour.RED)}  FAIL  #{blocked} ({blocked_info.title}) - {result.error}{c(Colour.RESET)}"
            )
            continue

        if "errors" in result.data:
            msg = result.data["errors"][0]["message"]
            if "already" in msg.lower():
                print(
                    f"{c(Colour.GRAY)}  SKIP  #{blocked} ({blocked_info.title}) - already exists{c(Colour.RESET)}"
                )
            else:
                print(
                    f"{c(Colour.RED)}  FAIL  #{blocked} ({blocked_info.title}) - {msg}{c(Colour.RESET)}"
                )
        else:
            print(
                f"{c(Colour.GREEN)}  OK    #{blocked} ({blocked_info.title}){c(Colour.RESET)}"
            )


def add_sub_issue(parent_issue: int, child_issues: list[int]) -> None:
    _do_relationship_action(
        action_name="sub-issue",
        primary_issue=parent_issue,
        target_issues=child_issues,
        primary_label="Parent",
        target_label="sub-issue",
        mutation_name="addSubIssue",
        build_mutation=lambda primary_id, target_id: (
            f'mutation {{ addSubIssue(input: {{ issueId: "{primary_id}", '
            f'subIssueId: "{target_id}" }}) {{ clientMutationId }} }}'
        ),
        ok_verb="sub-issue",
    )


def remove_blocked_by(blocked_issue: int, blocking_issues: list[int]) -> None:
    _do_relationship_action(
        action_name="remove-blocked-by",
        primary_issue=blocked_issue,
        target_issues=blocking_issues,
        primary_label="Issue",
        target_label="remove blocked-by",
        mutation_name="removeBlockedBy",
        build_mutation=lambda primary_id, target_id: (
            f'mutation {{ removeBlockedBy(input: {{ issueId: "{primary_id}", '
            f'blockingIssueId: "{target_id}" }}) {{ clientMutationId }} }}'
        ),
        ok_verb="removed blocked-by",
    )


def remove_blocking(blocking_issue: int, blocked_issues: list[int]) -> None:
    all_nums = {blocking_issue} | set(blocked_issues)
    ids = get_issue_node_ids(all_nums)
    if ids is None:
        return

    blocking_info = ids[blocking_issue]
    print(
        f"{c(Colour.CYAN)}Issue #{blocking_issue} ({blocking_info.title}) removing blocking:{c(Colour.RESET)}"
    )

    for blocked in blocked_issues:
        blocked_info = ids[blocked]
        mutation = (
            f'mutation {{ removeBlockedBy(input: {{ issueId: "{blocked_info.node_id}", '
            f'blockingIssueId: "{blocking_info.node_id}" }}) {{ clientMutationId }} }}'
        )

        result = run_mutation(mutation)
        if not result.ok:
            print(
                f"{c(Colour.RED)}  FAIL  remove blocking #{blocked} ({blocked_info.title}) - {result.error}{c(Colour.RESET)}"
            )
            continue

        if "errors" in result.data:
            msg = result.data["errors"][0]["message"]
            print(
                f"{c(Colour.RED)}  FAIL  remove blocking #{blocked} ({blocked_info.title}) - {msg}{c(Colour.RESET)}"
            )
        else:
            print(
                f"{c(Colour.GREEN)}  OK    removed blocking #{blocked} ({blocked_info.title}){c(Colour.RESET)}"
            )


def remove_sub_issue(parent_issue: int, child_issues: list[int]) -> None:
    _do_relationship_action(
        action_name="remove-sub-issue",
        primary_issue=parent_issue,
        target_issues=child_issues,
        primary_label="Parent",
        target_label="remove sub-issue",
        mutation_name="removeSubIssue",
        build_mutation=lambda primary_id, target_id: (
            f'mutation {{ removeSubIssue(input: {{ issueId: "{primary_id}", '
            f'subIssueId: "{target_id}" }}) {{ clientMutationId }} }}'
        ),
        ok_verb="removed sub-issue",
    )


# --- Query Commands ---


def show_relationships(issue_number: int) -> None:
    query = f'''query {{
  repository(owner: "{OWNER}", name: "{REPO}") {{
    issue(number: {issue_number}) {{
      title state
      labels(first: 20) {{ nodes {{ name }} }}
      assignees(first: 10) {{ nodes {{ login }} }}
      blockedBy(first: 50) {{ nodes {{ number title state }} }}
      blocking(first: 50) {{ nodes {{ number title state }} }}
      subIssues(first: 50) {{ nodes {{ number title state }} }}
      parent {{ number title state }}
    }}
  }}
}}'''

    result = run_graphql(query, "ShowRelationships")
    if not result.ok:
        print(result.error, file=sys.stderr)
        return

    data = result.data
    if "errors" in data:
        print(
            f"{c(Colour.YELLOW)}Note: Some relationship fields may not be available on your GitHub plan.{c(Colour.RESET)}"
        )
        print(data["errors"][0]["message"], file=sys.stderr)
        return

    issue = data["data"]["repository"]["issue"]
    if issue is None:
        print(f"Issue not found or inaccessible: #{issue_number}", file=sys.stderr)
        return

    state = issue["state"]
    title = issue["title"]
    state_colour = c(Colour.GREEN) if state == "CLOSED" else c(Colour.WHITE)

    print(
        f"\n{c(Colour.CYAN)}  #{issue_number} - {title} {state_colour}[{state}]{c(Colour.RESET)}"
    )

    # Labels
    labels = issue["labels"]["nodes"]
    if labels:
        label_str = ", ".join(l["name"] for l in labels)
        print(f"{c(Colour.GRAY)}  Labels: {label_str}{c(Colour.RESET)}")

    # Assignees
    assignees = issue["assignees"]["nodes"]
    if assignees:
        assignee_str = ", ".join(f"@{a['login']}" for a in assignees)
        print(f"{c(Colour.GRAY)}  Assigned: {assignee_str}{c(Colour.RESET)}")

    print()

    # Parent
    if issue["parent"] is not None:
        parent_state = issue["parent"]["state"]
        parent_icon = "[x]" if parent_state == "CLOSED" else "[ ]"
        parent_colour = c(Colour.GRAY) if parent_state == "CLOSED" else c(Colour.MAGENTA)
        print(
            f"{parent_colour}  Parent: {parent_icon} #{issue['parent']['number']} - "
            f"{issue['parent']['title']}{c(Colour.RESET)}"
        )

    # Blocked by
    blocked_by_nodes = issue["blockedBy"]["nodes"]
    if blocked_by_nodes:
        resolved_count = sum(1 for b in blocked_by_nodes if b["state"] == "CLOSED")
        total_count = len(blocked_by_nodes)

        if resolved_count == total_count:
            print(
                f"{c(Colour.GREEN)}  Blocked by (ALL RESOLVED):{c(Colour.RESET)}"
            )
        else:
            print(
                f"{c(Colour.YELLOW)}  Blocked by ({resolved_count}/{total_count} resolved):{c(Colour.RESET)}"
            )

        for b in blocked_by_nodes:
            b_state = b["state"]
            icon = "[x]" if b_state == "CLOSED" else "[ ]"
            colour = c(Colour.GRAY) if b_state == "CLOSED" else c(Colour.WHITE)
            print(
                f"{colour}    {icon} #{b['number']} - {b['title']}{c(Colour.RESET)}"
            )

    # Blocking
    blocking_nodes = issue["blocking"]["nodes"]
    if blocking_nodes:
        done_count = sum(1 for b in blocking_nodes if b["state"] == "CLOSED")
        total_count = len(blocking_nodes)

        print(
            f"{c(Colour.YELLOW)}  Blocking ({done_count}/{total_count} done):{c(Colour.RESET)}"
        )
        for b in blocking_nodes:
            b_state = b["state"]
            icon = "[x]" if b_state == "CLOSED" else "[ ]"
            colour = c(Colour.GRAY) if b_state == "CLOSED" else c(Colour.WHITE)
            print(
                f"{colour}    {icon} #{b['number']} - {b['title']}{c(Colour.RESET)}"
            )

    # Sub-issues
    sub_nodes = issue["subIssues"]["nodes"]
    if sub_nodes:
        done_count = sum(1 for s in sub_nodes if s["state"] == "CLOSED")
        total_count = len(sub_nodes)

        sub_colour = c(Colour.GREEN) if done_count == total_count else c(Colour.YELLOW)
        print(
            f"{sub_colour}  Sub-issues ({done_count}/{total_count} complete):{c(Colour.RESET)}"
        )
        for s in sub_nodes:
            s_state = s["state"]
            icon = "[x]" if s_state == "CLOSED" else "[ ]"
            colour = c(Colour.GRAY) if s_state == "CLOSED" else c(Colour.WHITE)
            print(
                f"{colour}    {icon} #{s['number']} - {s['title']}{c(Colour.RESET)}"
            )

    has_any = (
        issue["parent"] is not None
        or blocked_by_nodes
        or blocking_nodes
        or sub_nodes
    )
    if not has_any:
        print(f"{c(Colour.GRAY)}  No relationships found.{c(Colour.RESET)}")

    # Overall status
    print()
    if state == "CLOSED":
        print(f"{c(Colour.GREEN)}  Status: DONE{c(Colour.RESET)}")
    elif blocked_by_nodes:
        unresolved_count = sum(
            1 for b in blocked_by_nodes if b["state"] != "CLOSED"
        )
        if unresolved_count > 0:
            plural = "s" if unresolved_count != 1 else ""
            print(
                f"{c(Colour.RED)}  Status: BLOCKED ({unresolved_count} unresolved blocker{plural}){c(Colour.RESET)}"
            )
        else:
            print(
                f"{c(Colour.GREEN)}  Status: READY (all blockers resolved){c(Colour.RESET)}"
            )
    else:
        print(f"{c(Colour.GREEN)}  Status: READY{c(Colour.RESET)}")
    print()


def show_batch_summary(issue_numbers: list[int]) -> None:
    unique = sorted(set(issue_numbers))

    fields = " ".join(
        f"i{num}: issue(number: {num}) {{ number title state "
        f"labels(first: 10) {{ nodes {{ name }} }} "
        f"assignees(first: 5) {{ nodes {{ login }} }} "
        f"blockedBy(first: 50) {{ nodes {{ state }} }} "
        f"subIssues(first: 50) {{ nodes {{ state }} }} }}"
        for num in unique
    )
    query = f'query {{ repository(owner: "{OWNER}", name: "{REPO}") {{ {fields} }} }}'

    result = run_graphql(query, "ShowBatchSummary")
    if not result.ok:
        print(result.error, file=sys.stderr)
        return

    data = result.data
    if "errors" in data:
        print(f"GraphQL error: {data['errors'][0]['message']}", file=sys.stderr)
        return

    print(
        f"\n{c(Colour.CYAN)}  {'#':<8}{'State':<9}{'Status':<11}{'Sub-issues':<12}Title{c(Colour.RESET)}"
    )
    print(f"{c(Colour.GRAY)}  {'-' * 75}{c(Colour.RESET)}")

    for num in unique:
        key = f"i{num}"
        iss = data["data"]["repository"][key]
        if iss is None:
            continue

        iss_state = iss["state"]
        iss_title = iss["title"]

        unresolved_blockers = sum(
            1 for b in iss["blockedBy"]["nodes"] if b["state"] != "CLOSED"
        )

        if iss_state == "CLOSED":
            status_str = "DONE"
            status_colour = c(Colour.GREEN)
        elif unresolved_blockers > 0:
            status_str = "BLOCKED"
            status_colour = c(Colour.RED)
        else:
            status_str = "READY"
            status_colour = c(Colour.GREEN)

        subs = iss["subIssues"]["nodes"]
        if subs:
            sub_done = sum(1 for s in subs if s["state"] == "CLOSED")
            sub_str = f"{sub_done}/{len(subs)}"
        else:
            sub_str = "-"

        state_colour = c(Colour.GRAY) if iss_state == "CLOSED" else c(Colour.WHITE)

        print(
            f"  {state_colour}{pad_right(f'#{num}', 8)}{pad_right(iss_state, 9)}"
            f"{status_colour}{pad_right(status_str, 11)}"
            f"{state_colour}{pad_right(sub_str, 12)}{iss_title}{c(Colour.RESET)}"
        )
    print()


def show_ready() -> None:
    gh_result = run_gh(
        f'issue list --repo {OWNER}/{REPO} --state open --limit 200 '
        f'--json number,title,state,labels',
        "ShowReady",
    )
    if not gh_result.ok:
        print(gh_result.error, file=sys.stderr)
        return

    issues = gh_result.data
    if not issues:
        print(
            f"\n{c(Colour.YELLOW)}  No open issues found.{c(Colour.RESET)}\n"
        )
        return

    # Build GraphQL query for blocker/label/assignee info
    fields = " ".join(
        f"i{iss['number']}: issue(number: {iss['number']}) {{ number title state "
        f"labels(first: 10) {{ nodes {{ name }} }} "
        f"assignees(first: 5) {{ nodes {{ login }} }} "
        f"blockedBy(first: 50) {{ nodes {{ state }} }} }}"
        for iss in issues
    )
    query = f'query {{ repository(owner: "{OWNER}", name: "{REPO}") {{ {fields} }} }}'
    gql_result = run_graphql(query, "ShowReady")
    if not gql_result.ok:
        print(gql_result.error, file=sys.stderr)
        return

    data = gql_result.data
    if "errors" in data:
        print(f"GraphQL error: {data['errors'][0]['message']}", file=sys.stderr)
        return

    # Filter to ready issues (no unresolved blockers)
    ready_issues: list[dict] = []

    for iss in issues:
        num = iss["number"]
        key = f"i{num}"
        gql_iss = data["data"]["repository"][key]
        if gql_iss is None:
            continue

        unresolved_blockers = sum(
            1 for b in gql_iss["blockedBy"]["nodes"] if b["state"] != "CLOSED"
        )

        if unresolved_blockers == 0:
            labels = ", ".join(l["name"] for l in gql_iss["labels"]["nodes"])
            if len(labels) > 30:
                labels = labels[:27] + "..."

            assignees = ", ".join(
                f"@{a['login']}" for a in gql_iss["assignees"]["nodes"]
            )
            if not assignees:
                assignees = "-"

            ready_issues.append(
                {
                    "number": gql_iss["number"],
                    "title": gql_iss["title"],
                    "labels": labels,
                    "assignees": assignees,
                }
            )

    ready_issues.sort(key=lambda r: r["number"])

    print()
    if not ready_issues:
        print(
            f"{c(Colour.YELLOW)}  No ready issues found. Everything is blocked!{c(Colour.RESET)}"
        )
    else:
        print(
            f"{c(Colour.GREEN)}  Ready to work ({len(ready_issues)} issues):{c(Colour.RESET)}\n"
        )
        print(
            f"{c(Colour.CYAN)}  {'#':<8}{'Labels':<32}{'Assigned':<11}Title{c(Colour.RESET)}"
        )
        print(f"{c(Colour.GRAY)}  {'-' * 80}{c(Colour.RESET)}")

        for r in ready_issues:
            num_str = f"#{r['number']}"
            print(
                f"  {c(Colour.WHITE)}{pad_right(num_str, 8)}"
                f"{c(Colour.GRAY)}{pad_right(r['labels'], 32)}"
                f"{pad_right(r['assignees'], 11)}"
                f"{c(Colour.WHITE)}{r['title']}{c(Colour.RESET)}"
            )
    print()


def show_milestone_status(milestone_title: str) -> None:
    gh_result = run_gh(
        f'issue list --repo {OWNER}/{REPO} --milestone "{milestone_title}" '
        f'--state all --limit 200 --json number,title,state,labels',
        "ShowMilestoneStatus",
    )
    if not gh_result.ok:
        print(gh_result.error, file=sys.stderr)
        return

    issues = gh_result.data
    if not issues:
        print(
            f"{c(Colour.YELLOW)}No issues found for milestone '{milestone_title}'.{c(Colour.RESET)}"
        )
        return

    open_issues = [i for i in issues if i["state"] in ("OPEN", "open")]
    closed_issues = [i for i in issues if i["state"] not in ("OPEN", "open")]

    total = len(issues)
    done_count = len(closed_issues)
    pct = round(done_count / total * 100)

    # Progress bar
    bar_width = 40
    filled = round(done_count / total * bar_width)
    empty = bar_width - filled
    bar = "#" * filled + "." * empty
    if pct == 100:
        bar_colour = c(Colour.GREEN)
    elif pct >= 50:
        bar_colour = c(Colour.YELLOW)
    else:
        bar_colour = c(Colour.WHITE)

    print(f"\n{c(Colour.CYAN)}  Milestone: {milestone_title}{c(Colour.RESET)}")
    print(f"{bar_colour}  {bar} {pct}% ({done_count}/{total}){c(Colour.RESET)}\n")

    if open_issues:
        open_issues.sort(key=lambda i: i["number"])
        print(
            f"{c(Colour.YELLOW)}  Open ({len(open_issues)}):{c(Colour.RESET)}"
        )
        for iss in open_issues:
            labels = ", ".join(l["name"] for l in iss["labels"])
            label_suffix = f" [{labels}]" if labels else ""
            print(
                f"{c(Colour.WHITE)}    [ ] #{iss['number']} - {iss['title']}{label_suffix}{c(Colour.RESET)}"
            )

    if closed_issues:
        closed_issues.sort(key=lambda i: i["number"])
        print(
            f"{c(Colour.GREEN)}  Closed ({len(closed_issues)}):{c(Colour.RESET)}"
        )
        for iss in closed_issues:
            print(
                f"{c(Colour.GRAY)}    [x] #{iss['number']} - {iss['title']}{c(Colour.RESET)}"
            )
    print()


def show_orphans() -> None:
    gh_result = run_gh(
        f'issue list --repo {OWNER}/{REPO} --state open --limit 200 '
        f'--no-milestone --json number,title,labels',
        "ShowOrphans",
    )
    if not gh_result.ok:
        print(gh_result.error, file=sys.stderr)
        return

    issues = gh_result.data
    if not issues:
        print(
            f"\n{c(Colour.GREEN)}  No open issues without a milestone.{c(Colour.RESET)}\n"
        )
        return

    # Check which have no parent
    fields = " ".join(
        f"i{iss['number']}: issue(number: {iss['number']}) {{ number title "
        f"parent {{ number }} labels(first: 10) {{ nodes {{ name }} }} }}"
        for iss in issues
    )
    query = f'query {{ repository(owner: "{OWNER}", name: "{REPO}") {{ {fields} }} }}'
    gql_result = run_graphql(query, "ShowOrphans")
    if not gql_result.ok:
        print(gql_result.error, file=sys.stderr)
        return

    data = gql_result.data
    if "errors" in data:
        print(f"GraphQL error: {data['errors'][0]['message']}", file=sys.stderr)
        return

    orphans: list[dict] = []
    for iss in issues:
        num = iss["number"]
        key = f"i{num}"
        gql_iss = data["data"]["repository"][key]
        if gql_iss is None:
            continue

        if gql_iss["parent"] is None:
            labels = ", ".join(l["name"] for l in gql_iss["labels"]["nodes"])
            orphans.append(
                {
                    "number": gql_iss["number"],
                    "title": gql_iss["title"],
                    "labels": labels,
                }
            )

    orphans.sort(key=lambda o: o["number"])

    print()
    if not orphans:
        print(
            f"{c(Colour.GREEN)}  No orphaned issues found. Everything has a parent or milestone.{c(Colour.RESET)}"
        )
    else:
        print(
            f"{c(Colour.YELLOW)}  Orphaned issues ({len(orphans)} -- no parent, no milestone):{c(Colour.RESET)}\n"
        )
        for o in orphans:
            label_suffix = f" [{o['labels']}]" if o["labels"] else ""
            print(
                f"{c(Colour.WHITE)}    [ ] #{o['number']} - {o['title']}{label_suffix}{c(Colour.RESET)}"
            )
    print()


def show_chain(issue_number: int) -> None:
    visited: set[int] = set()
    chain: list[ChainEntry] = []

    def walk_blockers(num: int, depth: int) -> None:
        if num in visited:
            return
        if depth >= MAX_RECURSION_DEPTH:
            print(
                f"{c(Colour.YELLOW)}  Walk-Blockers: depth cap ({MAX_RECURSION_DEPTH}) "
                f"reached at #{num} — stopping{c(Colour.RESET)}"
            )
            return
        visited.add(num)

        query = f'''query {{
  repository(owner: "{OWNER}", name: "{REPO}") {{
    issue(number: {num}) {{
      number title state
      blockedBy(first: 50) {{ nodes {{ number title state }} }}
    }}
  }}
}}'''

        result = run_graphql(query, "ShowChain")
        if not result.ok:
            if depth == 0:
                print(
                    f"ShowChain: error for root issue #{num}: {result.error}",
                    file=sys.stderr,
                )
            return

        data = result.data
        if "errors" in data:
            if depth == 0:
                print(
                    f"ShowChain: GraphQL error for root issue #{num}: "
                    f"{data['errors'][0]['message']}",
                    file=sys.stderr,
                )
            return

        issue = data["data"]["repository"]["issue"]
        if issue is None:
            if depth == 0:
                print(
                    f"ShowChain: Issue not found or inaccessible: #{num}",
                    file=sys.stderr,
                )
            return

        chain.append(
            ChainEntry(
                number=issue["number"],
                title=issue["title"],
                state=issue["state"],
                depth=depth,
            )
        )

        for b in issue["blockedBy"]["nodes"]:
            walk_blockers(b["number"], depth + 1)

    walk_blockers(issue_number, 0)

    print(
        f"\n{c(Colour.CYAN)}  Dependency chain for #{issue_number}:{c(Colour.RESET)}\n"
    )

    if len(chain) <= 1:
        print(
            f"{c(Colour.GREEN)}  No blockers in the chain. Issue is independent.{c(Colour.RESET)}\n"
        )
        return

    max_depth = max(e.depth for e in chain) if chain else 0

    for entry in chain:
        indent = " " * (entry.depth * 4)
        icon = "[x]" if entry.state == "CLOSED" else "[ ]"
        if entry.state == "CLOSED":
            colour = c(Colour.GRAY)
        elif entry.depth == max_depth:
            colour = c(Colour.RED)
        else:
            colour = c(Colour.WHITE)
        prefix = "" if entry.depth == 0 else "blocked by -> "
        print(
            f"{colour}  {indent}{prefix}{icon} #{entry.number} - {entry.title}{c(Colour.RESET)}"
        )

    # Summary
    total_unresolved = sum(
        1 for e in chain if e.state != "CLOSED" and e.depth > 0
    )
    unresolved_leaves = [
        e for e in chain if e.depth == max_depth and e.state != "CLOSED"
    ]

    print()
    if total_unresolved == 0:
        print(
            f"{c(Colour.GREEN)}  All blockers resolved! Issue is ready.{c(Colour.RESET)}"
        )
    else:
        print(
            f"{c(Colour.YELLOW)}  {total_unresolved} unresolved blocker(s) in chain, depth {max_depth}{c(Colour.RESET)}"
        )
        if unresolved_leaves:
            leaf_nums = ", ".join(f"#{e.number}" for e in unresolved_leaves)
            print(
                f"{c(Colour.RED)}  Start here: {leaf_nums}{c(Colour.RESET)}"
            )
    print()


# --- Tree ---


def _get_tree_counts(nodes: list[TreeNode]) -> tuple[int, int]:
    """Return (done, total) counts for a tree."""
    done = 0
    total = 0
    for n in nodes:
        total += 1
        if n.state == "CLOSED":
            done += 1
        if n.children:
            sub_done, sub_total = _get_tree_counts(n.children)
            done += sub_done
            total += sub_total
    return done, total


def _fetch_sub_issue_tree(num: int, depth: int) -> TreeNode | None:
    """Recursively fetch the sub-issue tree."""
    if depth > MAX_RECURSION_DEPTH:
        print(
            f"{c(Colour.YELLOW)}  ShowTree: depth cap reached at #{num} — stopping{c(Colour.RESET)}"
        )
        return None

    query = f'''query {{
  repository(owner: "{OWNER}", name: "{REPO}") {{
    issue(number: {num}) {{
      number title state
      subIssues(first: 50) {{
        nodes {{ number title state }}
      }}
    }}
  }}
}}'''

    result = run_graphql(query, "ShowTree")
    if not result.ok:
        print(result.error, file=sys.stderr)
        return None

    data = result.data
    if "errors" in data:
        print(
            f"{c(Colour.YELLOW)}Note: Some fields may not be available on your GitHub plan.{c(Colour.RESET)}"
        )
        print(data["errors"][0]["message"], file=sys.stderr)
        return None

    issue = data["data"]["repository"]["issue"]
    if issue is None:
        return None

    node = TreeNode(
        number=issue["number"],
        title=issue["title"],
        state=issue["state"],
    )

    for child in issue["subIssues"]["nodes"]:
        child_tree = _fetch_sub_issue_tree(child["number"], depth + 1)
        if child_tree is not None:
            node.children.append(child_tree)
        else:
            # Leaf node or fetch failed
            node.children.append(
                TreeNode(
                    number=child["number"],
                    title=child["title"],
                    state=child["state"],
                )
            )

    return node


def _print_tree_node(node: TreeNode, depth: int) -> None:
    """Pretty-print a sub-issue tree node."""
    indent = " " * (depth * 4)
    icon = "[x]" if node.state == "CLOSED" else "[ ]"

    if depth == 0:
        colour = c(Colour.GREEN) if node.state == "CLOSED" else c(Colour.CYAN)
    else:
        colour = c(Colour.GRAY) if node.state == "CLOSED" else c(Colour.WHITE)

    child_progress = ""
    if node.children:
        done, total = _get_tree_counts(node.children)
        child_progress = f" ({done}/{total})"

    print(
        f"{colour}  {indent}{icon} #{node.number} - {node.title}{child_progress}{c(Colour.RESET)}"
    )

    for child in node.children:
        _print_tree_node(child, depth + 1)


def show_tree(issue_number: int) -> None:
    tree = _fetch_sub_issue_tree(issue_number, 0)
    if tree is None:
        print(
            f"Issue not found or inaccessible: #{issue_number}", file=sys.stderr
        )
        return

    print()
    _print_tree_node(tree, 0)
    print()


# --- Argument Parsing ---


def parse_int_list(s: str) -> list[int]:
    """Parse a comma-separated list of integers from a string."""
    result = []
    for token in s.split(","):
        token = token.strip()
        if not token:
            continue
        try:
            result.append(int(token))
        except ValueError:
            print(f"Invalid issue number: '{token}'", file=sys.stderr)
            return []
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="gh-issues",
        description="GitHub issue relationship manager for Wayfinder.\n"
        "Manages issue dependencies and sub-issue hierarchies using "
        "GitHub's GraphQL API via the gh CLI.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""examples:
  python tools/gh-issues.py blocked-by 12 7           # #12 is blocked by #7
  python tools/gh-issues.py blocking 7 12,15          # #7 blocks #12 and #15
  python tools/gh-issues.py sub-issue 10 41,42        # #41, #42 are sub-issues of #10
  python tools/gh-issues.py show 12                   # Show relationships for #12
  python tools/gh-issues.py show 12,15,20             # Batch summary for #12, #15, #20
  python tools/gh-issues.py tree 10                   # Sub-issue tree for #10
  python tools/gh-issues.py ready                     # List unblocked issues
  python tools/gh-issues.py status --milestone "v0.1" # Milestone progress
  python tools/gh-issues.py orphans                   # Find orphaned issues
  python tools/gh-issues.py chain 12                  # Dependency chain for #12""",
    )
    sub = parser.add_subparsers(dest="action", help="Action to perform")

    # --- Relationship mutations ---
    for name, help_text, issue_help, targets_help in [
        (
            "blocked-by",
            "Mark issue as blocked by target(s)",
            "Issue number to mark as blocked",
            "Comma-separated blocker issue numbers",
        ),
        (
            "blocking",
            "Mark issue as blocking target(s)",
            "Issue number that is blocking",
            "Comma-separated blocked issue numbers",
        ),
        (
            "sub-issue",
            "Add sub-issue(s) under parent",
            "Parent issue number",
            "Comma-separated child issue numbers",
        ),
        (
            "remove-blocked-by",
            "Remove blocked-by relationship(s)",
            "Issue number to unblock",
            "Comma-separated blocker issue numbers to remove",
        ),
        (
            "remove-blocking",
            "Remove blocking relationship(s)",
            "Issue number that was blocking",
            "Comma-separated blocked issue numbers to remove",
        ),
        (
            "remove-sub-issue",
            "Remove sub-issue(s) from parent",
            "Parent issue number",
            "Comma-separated child issue numbers to remove",
        ),
    ]:
        p = sub.add_parser(name, help=help_text)
        p.add_argument("issue", type=str, help=issue_help)
        p.add_argument("targets", type=str, help=targets_help)

    # --- Query commands ---
    p_show = sub.add_parser("show", help="Show relationships for issue(s)")
    p_show.add_argument(
        "issue", type=str, help="Issue number(s), comma-separated for batch summary"
    )

    p_tree = sub.add_parser("tree", help="Show sub-issue hierarchy")
    p_tree.add_argument("issue", type=str, help="Root issue number")

    p_chain = sub.add_parser("chain", help="Walk blocked-by dependency chain")
    p_chain.add_argument("issue", type=str, help="Issue number")

    sub.add_parser("ready", help="List unblocked open issues")

    p_status = sub.add_parser("status", help="Show milestone progress")
    p_status.add_argument(
        "--milestone",
        "-m",
        required=True,
        help="Milestone name",
    )

    sub.add_parser("orphans", help="Find issues with no parent/milestone")

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if not args.action:
        parser.print_help()
        return 1

    action = args.action

    # Commands that don't need issue numbers
    if action == "ready":
        show_ready()
        return 0

    if action == "orphans":
        show_orphans()
        return 0

    if action == "status":
        show_milestone_status(args.milestone)
        return 0

    # Commands that need at least an issue number
    issues = parse_int_list(args.issue)
    if not issues:
        return 1

    if action == "show":
        if len(issues) > 1:
            show_batch_summary(issues)
        else:
            show_relationships(issues[0])
        return 0

    if action == "tree":
        if len(issues) > 1:
            print("tree accepts only a single issue number.", file=sys.stderr)
            return 1
        show_tree(issues[0])
        return 0

    if action == "chain":
        if len(issues) > 1:
            print("chain accepts only a single issue number.", file=sys.stderr)
            return 1
        show_chain(issues[0])
        return 0

    # Commands that need issue + target
    targets = parse_int_list(args.targets)
    if not targets:
        return 1

    if len(issues) > 1:
        print(f"{action} accepts only a single issue number.", file=sys.stderr)
        return 1
    single_issue = issues[0]

    dispatch = {
        "blocked-by": lambda: add_blocked_by(single_issue, targets),
        "blocking": lambda: add_blocking(single_issue, targets),
        "sub-issue": lambda: add_sub_issue(single_issue, targets),
        "remove-blocked-by": lambda: remove_blocked_by(single_issue, targets),
        "remove-blocking": lambda: remove_blocking(single_issue, targets),
        "remove-sub-issue": lambda: remove_sub_issue(single_issue, targets),
    }

    handler = dispatch.get(action)
    if handler:
        handler()
    else:
        print(f"Unknown action: {action}", file=sys.stderr)
        parser.print_help()
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
