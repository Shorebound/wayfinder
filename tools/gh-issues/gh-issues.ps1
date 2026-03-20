#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Manages GitHub issue relationships (blocked-by, sub-issues) for the Wayfinder project.

.DESCRIPTION
    A helper tool for agents and developers to manage GitHub issue dependencies and
    sub-issue hierarchies using GitHub's native GraphQL API. Handles node ID lookups
    automatically.

    Requires: gh CLI authenticated with repo scope.

.PARAMETER Action
    The operation to perform:
      blocked-by         - Mark an issue as blocked by another issue
      blocking           - Mark an issue as blocking another issue (reverse of blocked-by)
      sub-issue          - Add an issue as a sub-issue of a parent
      remove-blocked-by  - Remove a blocked-by relationship
      remove-blocking    - Remove a blocking relationship
      remove-sub-issue   - Remove a sub-issue from its parent
      show               - Show relationships for an issue (or multiple with -Target)
      tree               - Show sub-issue hierarchy with completion progress
      ready              - List open issues that are unblocked and ready to work on
      status             - Show milestone progress (requires -Milestone)
      orphans            - Find open issues with no parent and no milestone
      chain              - Walk the blocked-by dependency chain for an issue

.PARAMETER Issue
    The issue number(s) to operate on. Most commands take a single number.
    For show, pass multiple comma-separated numbers for a batch summary.
    Not required for ready, status, or orphans.

.PARAMETER Target
    The target issue number(s). For blocked-by/blocking: the other issue(s).
    For sub-issue: the child issue(s) to add under Issue.
    For show: additional issue numbers to display in a compact table.

.PARAMETER Milestone
    The milestone title for the status command.

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 blocked-by 12 7
    # Issue #12 is now blocked by #7

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 blocked-by 12 7,8,9
    # Issue #12 is now blocked by #7, #8, and #9

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 blocking 7 12,15
    # Issue #7 is now blocking #12 and #15

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 sub-issue 10 41,42,43
    # Issues #41, #42, #43 are now sub-issues of #10

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 remove-blocked-by 12 7
    # Issue #12 is no longer blocked by #7

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 remove-sub-issue 10 41
    # Issue #41 is no longer a sub-issue of #10

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 show 12
    # Shows all relationships for issue #12 with completion status

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 show 12,15,20
    # Shows a compact summary table for issues #12, #15, and #20

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 tree 10
    # Shows sub-issue tree with completion counts for issue #10

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 ready 0
    # Lists all open issues that are ready to work on (no unresolved blockers)

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 status 0 -Milestone "v0.1"
    # Shows progress for milestone "v0.1"

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 orphans 0
    # Finds open issues with no parent and no milestone

.EXAMPLE
    .\tools\gh-issues\gh-issues.ps1 chain 12
    # Shows the full dependency chain for issue #12
#>

param(
    [Parameter(Mandatory, Position = 0)]
    [ValidateSet("blocked-by", "blocking", "sub-issue", "remove-blocked-by", "remove-blocking", "remove-sub-issue", "show", "tree", "ready", "status", "orphans", "chain")]
    [string]$Action,

    [Parameter(Position = 1)]
    [int[]]$Issue = @(0),

    [Parameter(Position = 2)]
    [int[]]$Target,

    [Parameter()]
    [string]$Milestone
)

$ErrorActionPreference = "Continue"
$OWNER = "Shorebound"
$REPO  = "wayfinder"

# --- Helpers ---

function Invoke-GhJson {
    <#
    .SYNOPSIS
        Runs a gh CLI command, validates the exit code, and parses stdout as JSON.
    #>
    param(
        [Parameter(Mandatory)]
        [string[]]$Arguments,
        [string]$ErrorContext = "gh"
    )

    $rawOutput = & gh @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $stdout = ($rawOutput | Where-Object { $_ -is [string] -or $_.GetType().Name -ne 'ErrorRecord' }) -join "`n"

    if ($exitCode -ne 0) {
        $stderr = ($rawOutput | Where-Object { $_.GetType().Name -eq 'ErrorRecord' }) -join "`n"
        throw "${ErrorContext}: gh exited with code $exitCode — $stderr"
    }

    try {
        return $stdout | ConvertFrom-Json
    }
    catch {
        throw "${ErrorContext}: Failed to parse JSON — $($_.Exception.Message)"
    }
}

function Get-IssueNodeId {
    param([int[]]$Numbers)

    # Deduplicate to avoid GraphQL alias collisions
    $Numbers = $Numbers | Select-Object -Unique

    # Build aliases: i7, i12, etc.
    $fields = ($Numbers | ForEach-Object { "i$($_): issue(number: $_) { id title }" }) -join " "
    $query = "query { repository(owner: `"$OWNER`", name: `"$REPO`") { $fields } }"

    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        Set-Content -Path $tempFile -Value $query -Encoding ascii -NoNewline
        $result = Invoke-GhJson -Arguments @("api", "graphql", "-F", "query=@$tempFile") -ErrorContext "Get-IssueNodeId"

        if ($result.errors) {
            Write-Error "GraphQL error: $($result.errors[0].message)"
            return $null
        }

        $map = @{}
        foreach ($num in $Numbers) {
            $entry = $result.data.repository."i$num"
            if (-not $entry) {
                Write-Error "Issue #$num not found in repository $OWNER/$REPO"
                return $null
            }
            $map[$num] = @{ Id = $entry.id; Title = $entry.title }
        }
        return $map
    }
    finally {
        Remove-Item -Path $tempFile -ErrorAction SilentlyContinue
    }
}

function Invoke-GraphQLMutation {
    param([string]$Mutation)

    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        Set-Content -Path $tempFile -Value $Mutation -Encoding ascii -NoNewline
        # Invoke-GhJson throws on transport/parse failure; mutation callers
        # still check $result.errors for GraphQL-level API errors.
        $result = Invoke-GhJson -Arguments @("api", "graphql", "-F", "query=@$tempFile") -ErrorContext "GraphQL mutation"
        return $result
    }
    finally {
        Remove-Item -Path $tempFile -ErrorAction SilentlyContinue
    }
}

# --- Actions ---

function Add-BlockedBy {
    param([int]$BlockedIssue, [int[]]$BlockingIssues)

    $allNums = @($BlockedIssue) + $BlockingIssues | Select-Object -Unique
    $ids = Get-IssueNodeId -Numbers $allNums
    if (-not $ids) { return }

    $blockedInfo = $ids[$BlockedIssue]
    Write-Host "Issue #$BlockedIssue ($($blockedInfo.Title))" -ForegroundColor Cyan

    foreach ($blocker in $BlockingIssues) {
        $blockerInfo = $ids[$blocker]
        $mutation = "mutation { addBlockedBy(input: { issueId: `"$($blockedInfo.Id)`", blockingIssueId: `"$($blockerInfo.Id)`" }) { clientMutationId } }"
        try {
            $result = Invoke-GraphQLMutation -Mutation $mutation
        } catch {
            Write-Host "  FAIL  blocked by #$blocker ($($blockerInfo.Title)) - $($_.Exception.Message)" -ForegroundColor Red
            Start-Sleep -Milliseconds 250
            continue
        }

        if ($result.errors) {
            $msg = $result.errors[0].message
            if ($msg -match "already been taken") {
                Write-Host "  SKIP  blocked by #$blocker ($($blockerInfo.Title)) - already exists" -ForegroundColor DarkGray
            } else {
                Write-Host "  FAIL  blocked by #$blocker ($($blockerInfo.Title)) - $msg" -ForegroundColor Red
            }
        } else {
            Write-Host "  OK    blocked by #$blocker ($($blockerInfo.Title))" -ForegroundColor Green
        }
        Start-Sleep -Milliseconds 250
    }
}

function Add-Blocking {
    param([int]$BlockingIssue, [int[]]$BlockedIssues)

    $allNums = @($BlockingIssue) + $BlockedIssues | Select-Object -Unique
    $ids = Get-IssueNodeId -Numbers $allNums
    if (-not $ids) { return }

    $blockingInfo = $ids[$BlockingIssue]
    Write-Host "Issue #$BlockingIssue ($($blockingInfo.Title)) now blocking:" -ForegroundColor Cyan

    foreach ($blocked in $BlockedIssues) {
        $blockedInfo = $ids[$blocked]
        # addBlockedBy: issueId = the blocked one, blockingIssueId = the blocker
        $mutation = "mutation { addBlockedBy(input: { issueId: `"$($blockedInfo.Id)`", blockingIssueId: `"$($blockingInfo.Id)`" }) { clientMutationId } }"
        try {
            $result = Invoke-GraphQLMutation -Mutation $mutation
        } catch {
            Write-Host "  FAIL  #$blocked ($($blockedInfo.Title)) - $($_.Exception.Message)" -ForegroundColor Red
            Start-Sleep -Milliseconds 250
            continue
        }

        if ($result.errors) {
            $msg = $result.errors[0].message
            if ($msg -match "already been taken") {
                Write-Host "  SKIP  #$blocked ($($blockedInfo.Title)) - already exists" -ForegroundColor DarkGray
            } else {
                Write-Host "  FAIL  #$blocked ($($blockedInfo.Title)) - $msg" -ForegroundColor Red
            }
        } else {
            Write-Host "  OK    #$blocked ($($blockedInfo.Title))" -ForegroundColor Green
        }
        Start-Sleep -Milliseconds 250
    }
}

function Add-SubIssue {
    param([int]$ParentIssue, [int[]]$ChildIssues)

    $allNums = @($ParentIssue) + $ChildIssues | Select-Object -Unique
    $ids = Get-IssueNodeId -Numbers $allNums
    if (-not $ids) { return }

    $parentInfo = $ids[$ParentIssue]
    Write-Host "Parent #$ParentIssue ($($parentInfo.Title))" -ForegroundColor Cyan

    foreach ($child in $ChildIssues) {
        $childInfo = $ids[$child]
        $mutation = "mutation { addSubIssue(input: { issueId: `"$($parentInfo.Id)`", subIssueId: `"$($childInfo.Id)`" }) { clientMutationId } }"
        try {
            $result = Invoke-GraphQLMutation -Mutation $mutation
        } catch {
            Write-Host "  FAIL  sub-issue #$child ($($childInfo.Title)) - $($_.Exception.Message)" -ForegroundColor Red
            Start-Sleep -Milliseconds 250
            continue
        }

        if ($result.errors) {
            $msg = $result.errors[0].message
            if ($msg -match "already") {
                Write-Host "  SKIP  sub-issue #$child ($($childInfo.Title)) - already exists" -ForegroundColor DarkGray
            } else {
                Write-Host "  FAIL  sub-issue #$child ($($childInfo.Title)) - $msg" -ForegroundColor Red
            }
        } else {
            Write-Host "  OK    sub-issue #$child ($($childInfo.Title))" -ForegroundColor Green
        }
        Start-Sleep -Milliseconds 250
    }
}

function Remove-BlockedBy {
    [CmdletBinding(SupportsShouldProcess=$true, ConfirmImpact='Medium')]
    param([int]$BlockedIssue, [int[]]$BlockingIssues)

    $allNums = @($BlockedIssue) + $BlockingIssues | Select-Object -Unique
    $ids = Get-IssueNodeId -Numbers $allNums
    if (-not $ids) { return }

    $blockedInfo = $ids[$BlockedIssue]
    Write-Host "Issue #$BlockedIssue ($($blockedInfo.Title))" -ForegroundColor Cyan

    foreach ($blocker in $BlockingIssues) {
        $blockerInfo = $ids[$blocker]
        if (-not $PSCmdlet.ShouldProcess("#$BlockedIssue ($($blockedInfo.Title))", "Remove blocked-by #$blocker ($($blockerInfo.Title))")) {
            continue
        }
        $mutation = "mutation { removeBlockedBy(input: { issueId: `"$($blockedInfo.Id)`", blockingIssueId: `"$($blockerInfo.Id)`" }) { clientMutationId } }"
        try {
            $result = Invoke-GraphQLMutation -Mutation $mutation
        } catch {
            Write-Host "  FAIL  remove blocked-by #$blocker ($($blockerInfo.Title)) - $($_.Exception.Message)" -ForegroundColor Red
            Start-Sleep -Milliseconds 250
            continue
        }

        if ($result.errors) {
            Write-Host "  FAIL  remove blocked-by #$blocker ($($blockerInfo.Title)) - $($result.errors[0].message)" -ForegroundColor Red
        } else {
            Write-Host "  OK    removed blocked-by #$blocker ($($blockerInfo.Title))" -ForegroundColor Green
        }
        Start-Sleep -Milliseconds 250
    }
}

function Remove-Blocking {
    [CmdletBinding(SupportsShouldProcess=$true, ConfirmImpact='Medium')]
    param([int]$BlockingIssue, [int[]]$BlockedIssues)

    $allNums = @($BlockingIssue) + $BlockedIssues | Select-Object -Unique
    $ids = Get-IssueNodeId -Numbers $allNums
    if (-not $ids) { return }

    $blockingInfo = $ids[$BlockingIssue]
    Write-Host "Issue #$BlockingIssue ($($blockingInfo.Title)) removing blocking:" -ForegroundColor Cyan

    foreach ($blocked in $BlockedIssues) {
        $blockedInfo = $ids[$blocked]
        if (-not $PSCmdlet.ShouldProcess("#$blocked ($($blockedInfo.Title))", "Remove blocking from #$BlockingIssue ($($blockingInfo.Title))")) {
            continue
        }
        $mutation = "mutation { removeBlockedBy(input: { issueId: `"$($blockedInfo.Id)`", blockingIssueId: `"$($blockingInfo.Id)`" }) { clientMutationId } }"
        try {
            $result = Invoke-GraphQLMutation -Mutation $mutation
        } catch {
            Write-Host "  FAIL  remove blocking #$blocked ($($blockedInfo.Title)) - $($_.Exception.Message)" -ForegroundColor Red
            Start-Sleep -Milliseconds 250
            continue
        }

        if ($result.errors) {
            Write-Host "  FAIL  remove blocking #$blocked ($($blockedInfo.Title)) - $($result.errors[0].message)" -ForegroundColor Red
        } else {
            Write-Host "  OK    removed blocking #$blocked ($($blockedInfo.Title))" -ForegroundColor Green
        }
        Start-Sleep -Milliseconds 250
    }
}

function Remove-SubIssue {
    [CmdletBinding(SupportsShouldProcess=$true, ConfirmImpact='Medium')]
    param([int]$ParentIssue, [int[]]$ChildIssues)

    $allNums = @($ParentIssue) + $ChildIssues | Select-Object -Unique
    $ids = Get-IssueNodeId -Numbers $allNums
    if (-not $ids) { return }

    $parentInfo = $ids[$ParentIssue]
    Write-Host "Parent #$ParentIssue ($($parentInfo.Title))" -ForegroundColor Cyan

    foreach ($child in $ChildIssues) {
        $childInfo = $ids[$child]
        if (-not $PSCmdlet.ShouldProcess("#$child ($($childInfo.Title))", "Remove sub-issue from #$ParentIssue ($($parentInfo.Title))")) {
            continue
        }
        $mutation = "mutation { removeSubIssue(input: { issueId: `"$($parentInfo.Id)`", subIssueId: `"$($childInfo.Id)`" }) { clientMutationId } }"
        try {
            $result = Invoke-GraphQLMutation -Mutation $mutation
        } catch {
            Write-Host "  FAIL  remove sub-issue #$child ($($childInfo.Title)) - $($_.Exception.Message)" -ForegroundColor Red
            Start-Sleep -Milliseconds 250
            continue
        }

        if ($result.errors) {
            Write-Host "  FAIL  remove sub-issue #$child ($($childInfo.Title)) - $($result.errors[0].message)" -ForegroundColor Red
        } else {
            Write-Host "  OK    removed sub-issue #$child ($($childInfo.Title))" -ForegroundColor Green
        }
        Start-Sleep -Milliseconds 250
    }
}

function Show-Relationships {
    param([int]$IssueNumber)

    $query = @"
query {
  repository(owner: "$OWNER", name: "$REPO") {
    issue(number: $IssueNumber) {
      title
      state
      labels(first: 20) { nodes { name } }
      assignees(first: 10) { nodes { login } }
      blockedBy(first: 50) { nodes { number title state } }
      blocking(first: 50) { nodes { number title state } }
      subIssues(first: 50) { nodes { number title state } }
      parent { number title state }
    }
  }
}
"@

    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        Set-Content -Path $tempFile -Value $query -Encoding ascii -NoNewline
        $result = Invoke-GhJson -Arguments @("api", "graphql", "-F", "query=@$tempFile") -ErrorContext "Show-Relationships"
    }
    finally {
        Remove-Item -Path $tempFile -ErrorAction SilentlyContinue
    }

    if ($result.errors) {
        Write-Host "Note: Some relationship fields may not be available on your GitHub plan." -ForegroundColor Yellow
        Write-Error $result.errors[0].message
        return
    }

    $issue = $result.data.repository.issue
    if ($null -eq $issue) {
        Write-Error "Issue not found or inaccessible: #$IssueNumber"
        return
    }
    $stateColor = if ($issue.state -eq "CLOSED") { "Green" } else { "White" }

    Write-Host ""
    Write-Host "  #$IssueNumber - $($issue.title) " -NoNewline -ForegroundColor Cyan
    Write-Host "[$($issue.state)]" -ForegroundColor $stateColor

    # Labels
    if ($issue.labels.nodes.Count -gt 0) {
        $labelStr = ($issue.labels.nodes | ForEach-Object { $_.name }) -join ", "
        Write-Host "  Labels: $labelStr" -ForegroundColor DarkGray
    }

    # Assignees
    if ($issue.assignees.nodes.Count -gt 0) {
        $assigneeStr = ($issue.assignees.nodes | ForEach-Object { "@$($_.login)" }) -join ", "
        Write-Host "  Assigned: $assigneeStr" -ForegroundColor DarkGray
    }

    Write-Host ""

    # Parent with state
    if ($issue.parent) {
        $parentIcon = if ($issue.parent.state -eq "CLOSED") { "[x]" } else { "[ ]" }
        $parentColor = if ($issue.parent.state -eq "CLOSED") { "DarkGray" } else { "Magenta" }
        Write-Host "  Parent: $parentIcon #$($issue.parent.number) - $($issue.parent.title)" -ForegroundColor $parentColor
    }

    # Blocked by with completion count
    $blockedByNodes = @($issue.blockedBy.nodes)
    if ($blockedByNodes.Count -gt 0) {
        $resolvedCount = @($blockedByNodes | Where-Object { $_.state -eq "CLOSED" }).Count
        $totalCount = $blockedByNodes.Count
        if ($resolvedCount -eq $totalCount) {
            Write-Host "  Blocked by (ALL RESOLVED):" -ForegroundColor Green
        } else {
            Write-Host "  Blocked by ($resolvedCount/$totalCount resolved):" -ForegroundColor Yellow
        }
        foreach ($b in $blockedByNodes) {
            $icon = if ($b.state -eq "CLOSED") { "[x]" } else { "[ ]" }
            $color = if ($b.state -eq "CLOSED") { "DarkGray" } else { "White" }
            Write-Host "    $icon #$($b.number) - $($b.title)" -ForegroundColor $color
        }
    }

    # Blocking with completion count
    $blockingNodes = @($issue.blocking.nodes)
    if ($blockingNodes.Count -gt 0) {
        $doneCount = @($blockingNodes | Where-Object { $_.state -eq "CLOSED" }).Count
        $totalCount = $blockingNodes.Count
        Write-Host "  Blocking ($doneCount/$totalCount done):" -ForegroundColor Yellow
        foreach ($b in $blockingNodes) {
            $icon = if ($b.state -eq "CLOSED") { "[x]" } else { "[ ]" }
            $color = if ($b.state -eq "CLOSED") { "DarkGray" } else { "White" }
            Write-Host "    $icon #$($b.number) - $($b.title)" -ForegroundColor $color
        }
    }

    # Sub-issues with completion count
    $subNodes = @($issue.subIssues.nodes)
    if ($subNodes.Count -gt 0) {
        $doneCount = @($subNodes | Where-Object { $_.state -eq "CLOSED" }).Count
        $totalCount = $subNodes.Count
        $subColor = if ($doneCount -eq $totalCount) { "Green" } else { "Yellow" }
        Write-Host "  Sub-issues ($doneCount/$totalCount complete):" -ForegroundColor $subColor
        foreach ($s in $subNodes) {
            $icon = if ($s.state -eq "CLOSED") { "[x]" } else { "[ ]" }
            $color = if ($s.state -eq "CLOSED") { "DarkGray" } else { "White" }
            Write-Host "    $icon #$($s.number) - $($s.title)" -ForegroundColor $color
        }
    }

    $hasAny = $issue.parent -or $blockedByNodes.Count -gt 0 -or $blockingNodes.Count -gt 0 -or $subNodes.Count -gt 0
    if (-not $hasAny) {
        Write-Host "  No relationships found." -ForegroundColor DarkGray
    }

    # Overall status summary
    Write-Host ""
    if ($issue.state -eq "CLOSED") {
        Write-Host "  Status: DONE" -ForegroundColor Green
    } elseif ($blockedByNodes.Count -gt 0) {
        $unresolvedCount = @($blockedByNodes | Where-Object { $_.state -ne "CLOSED" }).Count
        if ($unresolvedCount -gt 0) {
            $plural = if ($unresolvedCount -ne 1) { "s" } else { "" }
            Write-Host "  Status: BLOCKED ($unresolvedCount unresolved blocker$plural)" -ForegroundColor Red
        } else {
            Write-Host "  Status: READY (all blockers resolved)" -ForegroundColor Green
        }
    } else {
        Write-Host "  Status: READY" -ForegroundColor Green
    }
    Write-Host ""
}

function Show-BatchSummary {
    param([int[]]$IssueNumbers)

    # Deduplicate to avoid GraphQL alias collisions
    $IssueNumbers = $IssueNumbers | Select-Object -Unique

    # Build a single GraphQL query for all issues
    $fields = ($IssueNumbers | ForEach-Object { "i$($_): issue(number: $_) { number title state labels(first: 10) { nodes { name } } assignees(first: 5) { nodes { login } } blockedBy(first: 50) { nodes { state } } subIssues(first: 50) { nodes { state } } }" }) -join " "
    $query = "query { repository(owner: `"$OWNER`", name: `"$REPO`") { $fields } }"

    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        Set-Content -Path $tempFile -Value $query -Encoding ascii -NoNewline
        $result = Invoke-GhJson -Arguments @("api", "graphql", "-F", "query=@$tempFile") -ErrorContext "Show-BatchSummary"
    }
    finally {
        Remove-Item -Path $tempFile -ErrorAction SilentlyContinue
    }

    if ($result.errors) {
        Write-Error "GraphQL error: $($result.errors[0].message)"
        return
    }

    Write-Host ""
    Write-Host "  #       State    Status     Sub-issues  Title" -ForegroundColor Cyan
    Write-Host "  $('-' * 75)" -ForegroundColor DarkGray

    foreach ($num in $IssueNumbers) {
        $issue = $result.data.repository."i$num"
        if (-not $issue) { continue }

        $stateStr = $issue.state.PadRight(6)

        # Determine status
        $blockers = @($issue.blockedBy.nodes)
        $unresolvedBlockers = @($blockers | Where-Object { $_.state -ne "CLOSED" }).Count
        if ($issue.state -eq "CLOSED") {
            $statusStr = "DONE"
            $statusColor = "Green"
        } elseif ($unresolvedBlockers -gt 0) {
            $statusStr = "BLOCKED"
            $statusColor = "Red"
        } else {
            $statusStr = "READY"
            $statusColor = "Green"
        }

        # Sub-issue progress
        $subs = @($issue.subIssues.nodes)
        if ($subs.Count -gt 0) {
            $subDone = @($subs | Where-Object { $_.state -eq "CLOSED" }).Count
            $subStr = "$subDone/$($subs.Count)"
        } else {
            $subStr = "-"
        }

        $stateColor = if ($issue.state -eq "CLOSED") { "DarkGray" } else { "White" }

        Write-Host "  " -NoNewline
        Write-Host ("#$num").PadRight(8) -NoNewline -ForegroundColor $stateColor
        Write-Host $stateStr.PadRight(9) -NoNewline -ForegroundColor $stateColor
        Write-Host $statusStr.PadRight(11) -NoNewline -ForegroundColor $statusColor
        Write-Host $subStr.PadRight(12) -NoNewline -ForegroundColor $stateColor
        Write-Host $issue.title -ForegroundColor $stateColor
    }
    Write-Host ""
}

function Show-Ready {
    # Fetch all open issues, then check which ones have no unresolved blockers
    $issues = Invoke-GhJson -Arguments @("issue", "list", "--repo", "$OWNER/$REPO", "--state", "open", "--limit", "200", "--json", "number,title,labels,assignees") -ErrorContext "Show-Ready"

    if (-not $issues -or $issues.Count -eq 0) {
        Write-Host "No open issues found." -ForegroundColor DarkGray
        return
    }

    # Batch-query blockers for all open issues via GraphQL
    $fields = ($issues | ForEach-Object { "i$($_.number): issue(number: $($_.number)) { number title blockedBy(first: 50) { nodes { state } } labels(first: 10) { nodes { name } } assignees(first: 5) { nodes { login } } }" }) -join " "
    $query = "query { repository(owner: `"$OWNER`", name: `"$REPO`") { $fields } }"

    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        Set-Content -Path $tempFile -Value $query -Encoding ascii -NoNewline
        $result = Invoke-GhJson -Arguments @("api", "graphql", "-F", "query=@$tempFile") -ErrorContext "Show-Ready"
    }
    finally {
        Remove-Item -Path $tempFile -ErrorAction SilentlyContinue
    }

    if ($result.errors) {
        Write-Error "GraphQL error: $($result.errors[0].message)"
        return
    }

    $readyIssues = @()
    foreach ($issue in $issues) {
        $gqlIssue = $result.data.repository."i$($issue.number)"
        if (-not $gqlIssue) { continue }

        $blockers = @($gqlIssue.blockedBy.nodes)
        $unresolvedBlockers = @($blockers | Where-Object { $_.state -ne "CLOSED" }).Count
        if ($unresolvedBlockers -eq 0) {
            $readyIssues += $gqlIssue
        }
    }

    Write-Host ""
    if ($readyIssues.Count -eq 0) {
        Write-Host "  No ready issues found. Everything is blocked!" -ForegroundColor Yellow
    } else {
        Write-Host "  Ready to work ($($readyIssues.Count) issues):" -ForegroundColor Green
        Write-Host ""
        Write-Host "  #       Labels                          Assigned   Title" -ForegroundColor Cyan
        Write-Host "  $('-' * 80)" -ForegroundColor DarkGray

        foreach ($r in $readyIssues | Sort-Object { $_.number }) {
            $labels = ($r.labels.nodes | ForEach-Object { $_.name }) -join ", "
            if ($labels.Length -gt 30) { $labels = $labels.Substring(0, 27) + "..." }
            $assignees = ($r.assignees.nodes | ForEach-Object { "@$($_.login)" }) -join ", "
            if (-not $assignees) { $assignees = "-" }

            Write-Host "  " -NoNewline
            Write-Host ("#$($r.number)").PadRight(8) -NoNewline -ForegroundColor White
            Write-Host $labels.PadRight(32) -NoNewline -ForegroundColor DarkGray
            Write-Host $assignees.PadRight(11) -NoNewline -ForegroundColor DarkGray
            Write-Host $r.title -ForegroundColor White
        }
    }
    Write-Host ""
}

function Show-MilestoneStatus {
    param([string]$MilestoneTitle)

    $issues = Invoke-GhJson -Arguments @("issue", "list", "--repo", "$OWNER/$REPO", "--milestone", "$MilestoneTitle", "--state", "all", "--limit", "200", "--json", "number,title,state,labels") -ErrorContext "Show-MilestoneStatus"

    if (-not $issues -or $issues.Count -eq 0) {
        Write-Host "No issues found for milestone '$MilestoneTitle'." -ForegroundColor Yellow
        return
    }

    $open = @($issues | Where-Object { $_.state -eq "OPEN" })
    $closed = @($issues | Where-Object { $_.state -eq "CLOSED" })
    $total = $issues.Count
    $doneCount = $closed.Count
    $pct = [math]::Round(($doneCount / $total) * 100)

    # Progress bar
    $barWidth = 40
    $filled = [math]::Round(($doneCount / $total) * $barWidth)
    $empty = $barWidth - $filled
    $bar = ("#" * $filled) + ("." * $empty)
    $barColor = if ($pct -eq 100) { "Green" } elseif ($pct -ge 50) { "Yellow" } else { "White" }

    Write-Host ""
    Write-Host "  Milestone: $MilestoneTitle" -ForegroundColor Cyan
    Write-Host "  $bar $pct% ($doneCount/$total)" -ForegroundColor $barColor
    Write-Host ""

    # Group by label category if possible
    if ($open.Count -gt 0) {
        Write-Host "  Open ($($open.Count)):" -ForegroundColor Yellow
        foreach ($i in $open | Sort-Object { $_.number }) {
            $labels = ($i.labels | ForEach-Object { $_.name }) -join ", "
            $labelSuffix = if ($labels) { " [$labels]" } else { "" }
            Write-Host "    [ ] #$($i.number) - $($i.title)$labelSuffix" -ForegroundColor White
        }
    }

    if ($closed.Count -gt 0) {
        Write-Host "  Closed ($($closed.Count)):" -ForegroundColor Green
        foreach ($i in $closed | Sort-Object { $_.number }) {
            Write-Host "    [x] #$($i.number) - $($i.title)" -ForegroundColor DarkGray
        }
    }
    Write-Host ""
}

function Show-Orphans {
    $issues = Invoke-GhJson -Arguments @("issue", "list", "--repo", "$OWNER/$REPO", "--state", "open", "--limit", "200", "--no-milestone", "--json", "number,title,labels") -ErrorContext "Show-Orphans"

    if (-not $issues -or $issues.Count -eq 0) {
        Write-Host ""
        Write-Host "  No open issues without a milestone." -ForegroundColor Green
        Write-Host ""
        return
    }

    # Check which of these also have no parent
    $fields = ($issues | ForEach-Object { "i$($_.number): issue(number: $($_.number)) { number title parent { number } labels(first: 10) { nodes { name } } }" }) -join " "
    $query = "query { repository(owner: `"$OWNER`", name: `"$REPO`") { $fields } }"

    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        Set-Content -Path $tempFile -Value $query -Encoding ascii -NoNewline
        $result = Invoke-GhJson -Arguments @("api", "graphql", "-F", "query=@$tempFile") -ErrorContext "Show-Orphans"
    }
    finally {
        Remove-Item -Path $tempFile -ErrorAction SilentlyContinue
    }

    if ($result.errors) {
        Write-Error "GraphQL error: $($result.errors[0].message)"
        return
    }

    $orphans = @()
    foreach ($issue in $issues) {
        $gqlIssue = $result.data.repository."i$($issue.number)"
        if (-not $gqlIssue) { continue }
        if (-not $gqlIssue.parent) {
            $orphans += $gqlIssue
        }
    }

    Write-Host ""
    if ($orphans.Count -eq 0) {
        Write-Host "  No orphaned issues found. Everything has a parent or milestone." -ForegroundColor Green
    } else {
        Write-Host "  Orphaned issues ($($orphans.Count) -- no parent, no milestone):" -ForegroundColor Yellow
        Write-Host ""
        foreach ($o in $orphans | Sort-Object { $_.number }) {
            $labels = ($o.labels.nodes | ForEach-Object { $_.name }) -join ", "
            $labelSuffix = if ($labels) { " [$labels]" } else { "" }
            Write-Host "    [ ] #$($o.number) - $($o.title)$labelSuffix" -ForegroundColor White
        }
    }
    Write-Host ""
}

function Show-Chain {
    param([int]$IssueNumber)

    $visited = @{}
    $chain = [System.Collections.ArrayList]::new()

    $maxWalkDepth = 25

    function Walk-Blockers([int]$Num, [int]$Depth) {
        if ($visited.ContainsKey($Num)) { return }
        if ($Depth -ge $maxWalkDepth) {
            Write-Host "  Walk-Blockers: depth cap ($maxWalkDepth) reached at #$Num — stopping" -ForegroundColor Yellow
            return
        }
        $visited[$Num] = $true

        $query = @"
query {
  repository(owner: "$OWNER", name: "$REPO") {
    issue(number: $Num) {
      number title state
      blockedBy(first: 50) { nodes { number title state } }
    }
  }
}
"@
        $tempFile = [System.IO.Path]::GetTempFileName()
        try {
            Set-Content -Path $tempFile -Value $query -Encoding ascii -NoNewline
            $result = Invoke-GhJson -Arguments @("api", "graphql", "-F", "query=@$tempFile") -ErrorContext "Show-Chain"
        }
        finally {
            Remove-Item -Path $tempFile -ErrorAction SilentlyContinue
        }

        if ($result.errors) {
            if ($Depth -eq 0) { throw "Show-Chain: GraphQL error for root issue #${Num}: $($result.errors[0].message)" }
            return
        }

        $issue = $result.data.repository.issue
        if ($null -eq $issue) {
            if ($Depth -eq 0) { throw "Show-Chain: Issue not found or inaccessible: #$Num" }
            Write-Error "Issue not found or inaccessible: #$Num"
            return
        }
        [void]$chain.Add(@{ Number = $issue.number; Title = $issue.title; State = $issue.state; Depth = $Depth })

        $blockers = @($issue.blockedBy.nodes)
        foreach ($b in $blockers) {
            Walk-Blockers -Num $b.number -Depth ($Depth + 1)
        }
        Start-Sleep -Milliseconds 100
    }

    Walk-Blockers -Num $IssueNumber -Depth 0

    Write-Host ""
    Write-Host "  Dependency chain for #${IssueNumber}:" -ForegroundColor Cyan
    Write-Host ""

    if ($chain.Count -le 1) {
        Write-Host "  No blockers in the chain. Issue is independent." -ForegroundColor Green
        Write-Host ""
        return
    }

    # Find leaf nodes (deepest unresolved) -- these are the critical path
    $maxDepth = ($chain | ForEach-Object { $_.Depth } | Measure-Object -Maximum).Maximum

    foreach ($entry in $chain) {
        $indent = "    " * $entry.Depth
        $icon = if ($entry.State -eq "CLOSED") { "[x]" } else { "[ ]" }
        $color = if ($entry.State -eq "CLOSED") { "DarkGray" } elseif ($entry.Depth -eq $maxDepth) { "Red" } else { "White" }
        $prefix = if ($entry.Depth -eq 0) { "" } else { "blocked by -> " }
        Write-Host "  $indent$prefix$icon #$($entry.Number) - $($entry.Title)" -ForegroundColor $color
    }

    # Summary
    $unresolvedLeaves = @($chain | Where-Object { $_.Depth -eq $maxDepth -and $_.State -ne "CLOSED" })
    $totalUnresolved = @($chain | Where-Object { $_.State -ne "CLOSED" -and $_.Depth -gt 0 }).Count
    Write-Host ""
    if ($totalUnresolved -eq 0) {
        Write-Host "  All blockers resolved! Issue is ready." -ForegroundColor Green
    } else {
        Write-Host "  $totalUnresolved unresolved blocker(s) in chain, depth $maxDepth" -ForegroundColor Yellow
        if ($unresolvedLeaves.Count -gt 0) {
            $leafNums = ($unresolvedLeaves | ForEach-Object { "#$($_.Number)" }) -join ", "
            Write-Host "  Start here: $leafNums" -ForegroundColor Red
        }
    }
    Write-Host ""
}

function Show-Tree {
    param([int]$IssueNumber)

    # Recursively fetch sub-issues for a given issue number.
    # Returns a tree node: @{ number; title; state; children = @(...) }
    function Fetch-SubIssueTree([int]$Num, [int]$Depth) {
        if ($Depth -gt 25) {
            Write-Host "  Show-Tree: depth cap reached at #$Num — stopping" -ForegroundColor Yellow
            return $null
        }

        $query = @"
query {
  repository(owner: "$OWNER", name: "$REPO") {
    issue(number: $Num) {
      number title state
      subIssues(first: 50) {
        nodes { number title state }
      }
    }
  }
}
"@
        $tempFile = [System.IO.Path]::GetTempFileName()
        try {
            Set-Content -Path $tempFile -Value $query -Encoding ascii -NoNewline
            $result = Invoke-GhJson -Arguments @("api", "graphql", "-F", "query=@$tempFile") -ErrorContext "Show-Tree"
        }
        finally {
            Remove-Item -Path $tempFile -ErrorAction SilentlyContinue
        }

        if ($result.errors) {
            Write-Host "Note: Some fields may not be available on your GitHub plan." -ForegroundColor Yellow
            Write-Error $result.errors[0].message
            return $null
        }

        $issue = $result.data.repository.issue
        if ($null -eq $issue) { return $null }

        $children = @()
        foreach ($child in @($issue.subIssues.nodes)) {
            $childTree = Fetch-SubIssueTree -Num $child.number -Depth ($Depth + 1)
            if ($null -ne $childTree) {
                $children += $childTree
            } else {
                # Leaf node or fetch failed — include with no children
                $children += @{ number = $child.number; title = $child.title; state = $child.state; children = @() }
            }
            Start-Sleep -Milliseconds 50
        }

        return @{ number = $issue.number; title = $issue.title; state = $issue.state; children = $children }
    }

    $tree = Fetch-SubIssueTree -Num $IssueNumber -Depth 0
    if ($null -eq $tree) {
        Write-Error "Issue not found or inaccessible: #$IssueNumber"
        return
    }

    # Count all descendants recursively from the tree structure
    function Get-TreeCounts($nodes) {
        $done = 0; $total = 0
        foreach ($n in $nodes) {
            $total++
            if ($n.state -eq "CLOSED") { $done++ }
            if ($n.children.Count -gt 0) {
                $sub = Get-TreeCounts $n.children
                $done += $sub.Done; $total += $sub.Total
            }
        }
        return @{ Done = $done; Total = $total }
    }

    # Print the tree recursively
    function Print-TreeNode($node, [int]$Depth) {
        $indent = "    " * $Depth
        $icon = if ($node.state -eq "CLOSED") { "[x]" } else { "[ ]" }
        $color = if ($Depth -eq 0) {
            if ($node.state -eq "CLOSED") { "Green" } else { "Cyan" }
        } else {
            if ($node.state -eq "CLOSED") { "DarkGray" } else { "White" }
        }

        $childProgress = ""
        if ($node.children.Count -gt 0) {
            $counts = Get-TreeCounts $node.children
            $childProgress = " ($($counts.Done)/$($counts.Total))"
        }

        Write-Host "  ${indent}$icon #$($node.number) - $($node.title)$childProgress" -ForegroundColor $color

        foreach ($child in $node.children) {
            Print-TreeNode $child ($Depth + 1)
        }
    }

    Write-Host ""
    Print-TreeNode $tree 0
    Write-Host ""
}

# --- Main ---

# Validate required parameters per action
$needsIssue = @("blocked-by", "blocking", "sub-issue", "remove-blocked-by", "remove-blocking", "remove-sub-issue", "show", "tree", "chain")
if ($Action -in $needsIssue -and $Issue.Count -eq 1 -and $Issue[0] -eq 0) {
    Write-Error "$Action requires an issue number. Example: .\gh-issues.ps1 $Action 12"
    exit 1
}

# Actions that accept only a single issue number (show handles multiple via batch summary)
$singleIssueOnly = @("blocked-by", "blocking", "sub-issue", "remove-blocked-by", "remove-blocking", "remove-sub-issue", "tree", "chain")
if ($Action -in $singleIssueOnly -and $Issue.Count -gt 1) {
    Write-Error "$Action accepts only a single issue number. Got: $($Issue -join ', ')"
    exit 1
}

# For commands that need a single issue number
$SingleIssue = $Issue[0]

switch ($Action) {
    "blocked-by" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "blocked-by requires -Target issue number(s). Example: .\gh-issues.ps1 blocked-by 12 7,8"
            exit 1
        }
        Add-BlockedBy -BlockedIssue $SingleIssue -BlockingIssues $Target
    }
    "blocking" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "blocking requires -Target issue number(s). Example: .\gh-issues.ps1 blocking 7 12,15"
            exit 1
        }
        Add-Blocking -BlockingIssue $SingleIssue -BlockedIssues $Target
    }
    "sub-issue" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "sub-issue requires -Target issue number(s). Example: .\gh-issues.ps1 sub-issue 10 41,42"
            exit 1
        }
        Add-SubIssue -ParentIssue $SingleIssue -ChildIssues $Target
    }
    "remove-blocked-by" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "remove-blocked-by requires -Target issue number(s). Example: .\gh-issues.ps1 remove-blocked-by 12 7"
            exit 1
        }
        Remove-BlockedBy -BlockedIssue $SingleIssue -BlockingIssues $Target
    }
    "remove-blocking" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "remove-blocking requires -Target issue number(s). Example: .\gh-issues.ps1 remove-blocking 7 12,15"
            exit 1
        }
        Remove-Blocking -BlockingIssue $SingleIssue -BlockedIssues $Target
    }
    "remove-sub-issue" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "remove-sub-issue requires -Target issue number(s). Example: .\gh-issues.ps1 remove-sub-issue 10 41"
            exit 1
        }
        Remove-SubIssue -ParentIssue $SingleIssue -ChildIssues $Target
    }
    "show" {
        if ($Issue.Count -gt 1) {
            Show-BatchSummary -IssueNumbers $Issue
        } else {
            Show-Relationships -IssueNumber $SingleIssue
        }
    }
    "tree" {
        Show-Tree -IssueNumber $SingleIssue
    }
    "ready" {
        Show-Ready
    }
    "status" {
        if (-not $Milestone) {
            Write-Error "status requires -Milestone. Example: .\gh-issues.ps1 status 0 -Milestone 'v0.1'"
            exit 1
        }
        Show-MilestoneStatus -MilestoneTitle $Milestone
    }
    "orphans" {
        Show-Orphans
    }
    "chain" {
        Show-Chain -IssueNumber $SingleIssue
    }
}
