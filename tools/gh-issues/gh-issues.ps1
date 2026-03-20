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
      show               - Show relationships for an issue

.PARAMETER Issue
    The issue number to operate on.

.PARAMETER Target
    The target issue number(s). For blocked-by/blocking: the other issue(s).
    For sub-issue: the child issue(s) to add under Issue.

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
    # Shows all relationships for issue #12
#>

param(
    [Parameter(Mandatory, Position = 0)]
    [ValidateSet("blocked-by", "blocking", "sub-issue", "remove-blocked-by", "remove-blocking", "remove-sub-issue", "show")]
    [string]$Action,

    [Parameter(Mandatory, Position = 1)]
    [int]$Issue,

    [Parameter(Position = 2)]
    [int[]]$Target
)

$ErrorActionPreference = "Continue"
$OWNER = "Shorebound"
$REPO  = "wayfinder"

# --- Helpers ---

function Get-IssueNodeId {
    param([int[]]$Numbers)

    # Build aliases: i7, i12, etc.
    $fields = ($Numbers | ForEach-Object { "i$($_): issue(number: $_) { id title }" }) -join " "
    $query = "query { repository(owner: `"$OWNER`", name: `"$REPO`") { $fields } }"

    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        Set-Content -Path $tempFile -Value $query -Encoding ascii -NoNewline
        $raw = gh api graphql -F query=@$tempFile 2>&1
        $result = ($raw | Where-Object { $_ -is [string] -or $_.GetType().Name -ne 'ErrorRecord' }) -join "`n" | ConvertFrom-Json

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
        $raw = gh api graphql -F query=@$tempFile 2>&1
        $json = ($raw | Where-Object { $_ -is [string] -or $_.GetType().Name -ne 'ErrorRecord' }) -join "`n" | ConvertFrom-Json
        return $json
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
        $result = Invoke-GraphQLMutation -Mutation $mutation

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
        $result = Invoke-GraphQLMutation -Mutation $mutation

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
        $result = Invoke-GraphQLMutation -Mutation $mutation

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
    param([int]$BlockedIssue, [int[]]$BlockingIssues)

    $allNums = @($BlockedIssue) + $BlockingIssues | Select-Object -Unique
    $ids = Get-IssueNodeId -Numbers $allNums
    if (-not $ids) { return }

    $blockedInfo = $ids[$BlockedIssue]
    Write-Host "Issue #$BlockedIssue ($($blockedInfo.Title))" -ForegroundColor Cyan

    foreach ($blocker in $BlockingIssues) {
        $blockerInfo = $ids[$blocker]
        $mutation = "mutation { removeBlockedBy(input: { issueId: `"$($blockedInfo.Id)`", blockingIssueId: `"$($blockerInfo.Id)`" }) { clientMutationId } }"
        $result = Invoke-GraphQLMutation -Mutation $mutation

        if ($result.errors) {
            Write-Host "  FAIL  remove blocked-by #$blocker ($($blockerInfo.Title)) - $($result.errors[0].message)" -ForegroundColor Red
        } else {
            Write-Host "  OK    removed blocked-by #$blocker ($($blockerInfo.Title))" -ForegroundColor Green
        }
        Start-Sleep -Milliseconds 250
    }
}

function Remove-Blocking {
    param([int]$BlockingIssue, [int[]]$BlockedIssues)

    $allNums = @($BlockingIssue) + $BlockedIssues | Select-Object -Unique
    $ids = Get-IssueNodeId -Numbers $allNums
    if (-not $ids) { return }

    $blockingInfo = $ids[$BlockingIssue]
    Write-Host "Issue #$BlockingIssue ($($blockingInfo.Title)) removing blocking:" -ForegroundColor Cyan

    foreach ($blocked in $BlockedIssues) {
        $blockedInfo = $ids[$blocked]
        $mutation = "mutation { removeBlockedBy(input: { issueId: `"$($blockedInfo.Id)`", blockingIssueId: `"$($blockingInfo.Id)`" }) { clientMutationId } }"
        $result = Invoke-GraphQLMutation -Mutation $mutation

        if ($result.errors) {
            Write-Host "  FAIL  remove blocking #$blocked ($($blockedInfo.Title)) - $($result.errors[0].message)" -ForegroundColor Red
        } else {
            Write-Host "  OK    removed blocking #$blocked ($($blockedInfo.Title))" -ForegroundColor Green
        }
        Start-Sleep -Milliseconds 250
    }
}

function Remove-SubIssue {
    param([int]$ParentIssue, [int[]]$ChildIssues)

    $allNums = @($ParentIssue) + $ChildIssues | Select-Object -Unique
    $ids = Get-IssueNodeId -Numbers $allNums
    if (-not $ids) { return }

    $parentInfo = $ids[$ParentIssue]
    Write-Host "Parent #$ParentIssue ($($parentInfo.Title))" -ForegroundColor Cyan

    foreach ($child in $ChildIssues) {
        $childInfo = $ids[$child]
        $mutation = "mutation { removeSubIssue(input: { issueId: `"$($parentInfo.Id)`", subIssueId: `"$($childInfo.Id)`" }) { clientMutationId } }"
        $result = Invoke-GraphQLMutation -Mutation $mutation

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
      blockedBy(first: 50) { nodes { number title state } }
      blocking(first: 50) { nodes { number title state } }
      subIssues(first: 50) { nodes { number title state } }
      parent { number title }
    }
  }
}
"@

    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        Set-Content -Path $tempFile -Value $query -Encoding ascii -NoNewline
        $raw = gh api graphql -F query=@$tempFile 2>&1
        $result = ($raw | Where-Object { $_ -is [string] -or $_.GetType().Name -ne 'ErrorRecord' }) -join "`n" | ConvertFrom-Json
    }
    finally {
        Remove-Item -Path $tempFile -ErrorAction SilentlyContinue
    }

    if ($result.errors) {
        # Might not support all fields - fall back to simpler query
        Write-Host "Note: Some relationship fields may not be available on your GitHub plan." -ForegroundColor Yellow
        Write-Error $result.errors[0].message
        return
    }

    $issue = $result.data.repository.issue
    Write-Host ""
    Write-Host "Issue #$IssueNumber - $($issue.title) [$($issue.state)]" -ForegroundColor Cyan
    Write-Host ""

    if ($issue.parent) {
        Write-Host "  Parent: #$($issue.parent.number) ($($issue.parent.title))" -ForegroundColor Magenta
    }

    if ($issue.blockedBy.nodes.Count -gt 0) {
        Write-Host "  Blocked by:" -ForegroundColor Yellow
        foreach ($b in $issue.blockedBy.nodes) {
            $icon = if ($b.state -eq "CLOSED") { "[x]" } else { "[ ]" }
            Write-Host "    $icon #$($b.number) - $($b.title)" -ForegroundColor $(if ($b.state -eq "CLOSED") { "DarkGray" } else { "White" })
        }
    }

    if ($issue.blocking.nodes.Count -gt 0) {
        Write-Host "  Blocking:" -ForegroundColor Yellow
        foreach ($b in $issue.blocking.nodes) {
            $icon = if ($b.state -eq "CLOSED") { "[x]" } else { "[ ]" }
            Write-Host "    $icon #$($b.number) - $($b.title)" -ForegroundColor $(if ($b.state -eq "CLOSED") { "DarkGray" } else { "White" })
        }
    }

    if ($issue.subIssues.nodes.Count -gt 0) {
        Write-Host "  Sub-issues:" -ForegroundColor Yellow
        foreach ($s in $issue.subIssues.nodes) {
            $icon = if ($s.state -eq "CLOSED") { "[x]" } else { "[ ]" }
            Write-Host "    $icon #$($s.number) - $($s.title)" -ForegroundColor $(if ($s.state -eq "CLOSED") { "DarkGray" } else { "White" })
        }
    }

    $hasAny = $issue.parent -or $issue.blockedBy.nodes.Count -gt 0 -or $issue.blocking.nodes.Count -gt 0 -or $issue.subIssues.nodes.Count -gt 0
    if (-not $hasAny) {
        Write-Host "  No relationships found." -ForegroundColor DarkGray
    }
    Write-Host ""
}

# --- Main ---

switch ($Action) {
    "blocked-by" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "blocked-by requires -Target issue number(s). Example: .\gh-issues.ps1 blocked-by 12 7,8"
            exit 1
        }
        Add-BlockedBy -BlockedIssue $Issue -BlockingIssues $Target
    }
    "blocking" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "blocking requires -Target issue number(s). Example: .\gh-issues.ps1 blocking 7 12,15"
            exit 1
        }
        Add-Blocking -BlockingIssue $Issue -BlockedIssues $Target
    }
    "sub-issue" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "sub-issue requires -Target issue number(s). Example: .\gh-issues.ps1 sub-issue 10 41,42"
            exit 1
        }
        Add-SubIssue -ParentIssue $Issue -ChildIssues $Target
    }
    "remove-blocked-by" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "remove-blocked-by requires -Target issue number(s). Example: .\gh-issues.ps1 remove-blocked-by 12 7"
            exit 1
        }
        Remove-BlockedBy -BlockedIssue $Issue -BlockingIssues $Target
    }
    "remove-blocking" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "remove-blocking requires -Target issue number(s). Example: .\gh-issues.ps1 remove-blocking 7 12,15"
            exit 1
        }
        Remove-Blocking -BlockingIssue $Issue -BlockedIssues $Target
    }
    "remove-sub-issue" {
        if (-not $Target -or $Target.Count -eq 0) {
            Write-Error "remove-sub-issue requires -Target issue number(s). Example: .\gh-issues.ps1 remove-sub-issue 10 41"
            exit 1
        }
        Remove-SubIssue -ParentIssue $Issue -ChildIssues $Target
    }
    "show" {
        Show-Relationships -IssueNumber $Issue
    }
}
