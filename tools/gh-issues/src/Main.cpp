/// gh-issues — GitHub issue relationship manager for Wayfinder
///
/// A standalone CLI tool that manages GitHub issue dependencies (blocked-by,
/// sub-issues) using the gh CLI and GitHub's GraphQL API.
///
/// Requires: gh CLI authenticated with repo scope.

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#include <windows.h>
#endif

using json = nlohmann::json;

// --- Constants ---

static constexpr std::string_view OWNER = "Shorebound";
static constexpr std::string_view REPO = "wayfinder";
static constexpr int MAX_RECURSION_DEPTH = 25;

// --- ANSI Colours ---

namespace Colour
{
    static constexpr std::string_view Reset = "\033[0m";
    static constexpr std::string_view Red = "\033[31m";
    static constexpr std::string_view Green = "\033[32m";
    static constexpr std::string_view Yellow = "\033[33m";
    static constexpr std::string_view Magenta = "\033[35m";
    static constexpr std::string_view Cyan = "\033[36m";
    static constexpr std::string_view White = "\033[37m";
    static constexpr std::string_view Gray = "\033[90m";
} // namespace Colour

static void EnableAnsiOnWindows()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE)
    {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode))
        {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
}

// --- Helpers ---

/// Pads a string to a minimum width with trailing spaces.
static std::string PadRight(const std::string& s, size_t width)
{
    if (s.size() >= width)
    {
        return s;
    }
    return s + std::string(width - s.size(), ' ');
}

/// Creates a temporary file, writes content, and returns its path.
/// The caller is responsible for removing the file.
static std::filesystem::path WriteTempFile(const std::string& content)
{
    auto tempDir = std::filesystem::temp_directory_path();
    // Use a pseudo-random name to avoid collisions
    auto name = std::format("gh-issues-{}.graphql", std::rand());
    auto path = tempDir / name;
    std::ofstream ofs(path, std::ios::binary);
    ofs << content;
    return path;
}

struct ProcessResult
{
    std::string Output;
    int ExitCode = -1;
};

/// Runs a shell command and captures stdout.
static ProcessResult RunProcess(const std::string& command)
{
    ProcessResult result;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        result.ExitCode = -1;
        return result;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe))
    {
        result.Output += buffer;
    }
    result.ExitCode = pclose(pipe);
#ifdef _WIN32
    // _pclose returns the process exit code directly on Windows
#else
    // On POSIX, pclose returns the status from waitpid — extract exit code
    if (WIFEXITED(result.ExitCode))
    {
        result.ExitCode = WEXITSTATUS(result.ExitCode);
    }
#endif
    return result;
}

/// Runs a gh CLI command and parses the output as JSON.
/// Returns a pair of (json, error_string). On success, error is empty.
struct GhResult
{
    json Data;
    std::string Error;
    bool Ok() const
    {
        return Error.empty();
    }
};

static GhResult RunGh(const std::string& args, std::string_view context = "gh")
{
    auto cmd = std::format("gh {} 2>&1", args);
    auto proc = RunProcess(cmd);

    if (proc.ExitCode != 0)
    {
        return {{}, std::format("{}: gh exited with code {} — {}", context, proc.ExitCode, proc.Output)};
    }

    try
    {
        return {json::parse(proc.Output), {}};
    }
    catch (const json::exception& e)
    {
        return {{}, std::format("{}: Failed to parse JSON — {}", context, e.what())};
    }
}

/// Sends a GraphQL query via gh api and returns parsed JSON.
static GhResult RunGraphQL(const std::string& query, std::string_view context = "GraphQL")
{
    auto tempFile = WriteTempFile(query);
    auto cmd = std::format("api graphql -F query=@{}", tempFile.string());
    auto result = RunGh(cmd, context);
    std::filesystem::remove(tempFile);
    return result;
}

/// Executes a GraphQL mutation and returns the result.
static GhResult RunMutation(const std::string& mutation)
{
    return RunGraphQL(mutation, "GraphQL mutation");
}

// --- Data Types ---

struct IssueInfo
{
    std::string NodeId;
    std::string Title;
};

struct IssueNode
{
    int Number = 0;
    std::string Title;
    std::string State;
};

struct TreeNode
{
    int Number = 0;
    std::string Title;
    std::string State;
    std::vector<TreeNode> Children;
};

struct ChainEntry
{
    int Number = 0;
    std::string Title;
    std::string State;
    int Depth = 0;
};

// --- Issue ID Resolution ---

/// Batch-fetches node IDs and titles for a set of issue numbers.
/// Returns a map of issue_number -> IssueInfo.
static std::map<int, IssueInfo> GetIssueNodeIds(const std::set<int>& numbers)
{
    // Build GraphQL aliases: i7, i12, etc.
    std::string fields;
    for (int num : numbers)
    {
        if (!fields.empty())
        {
            fields += " ";
        }
        fields += std::format("i{}: issue(number: {}) {{ id title }}", num, num);
    }
    auto query = std::format(R"(query {{ repository(owner: "{}", name: "{}") {{ {} }} }})", OWNER, REPO, fields);

    auto result = RunGraphQL(query, "GetIssueNodeIds");
    if (!result.Ok())
    {
        std::cerr << result.Error << "\n";
        return {};
    }

    if (result.Data.contains("errors"))
    {
        std::cerr << "GraphQL error: " << result.Data["errors"][0]["message"] << "\n";
        return {};
    }

    std::map<int, IssueInfo> map;
    for (int num : numbers)
    {
        auto key = std::format("i{}", num);
        auto& repo = result.Data["data"]["repository"];
        if (!repo.contains(key) || repo[key].is_null())
        {
            std::cerr << std::format("Issue #{} not found in repository {}/{}\n", num, OWNER, REPO);
            return {};
        }
        map[num] = IssueInfo{
        .NodeId = repo[key]["id"].get<std::string>(),
        .Title = repo[key]["title"].get<std::string>(),
        };
    }
    return map;
}

// --- Relationship Mutations ---

static void AddBlockedBy(int blockedIssue, const std::vector<int>& blockingIssues)
{
    std::set<int> allNums{blockedIssue};
    allNums.insert(blockingIssues.begin(), blockingIssues.end());

    auto ids = GetIssueNodeIds(allNums);
    if (ids.empty())
    {
        return;
    }

    auto& blockedInfo = ids[blockedIssue];
    std::cout << std::format("{}Issue #{} ({}){}\n", Colour::Cyan, blockedIssue, blockedInfo.Title, Colour::Reset);

    for (int blocker : blockingIssues)
    {
        auto& blockerInfo = ids[blocker];
        auto mutation = std::format(R"(mutation {{ addBlockedBy(input: {{ issueId: "{}", blockingIssueId: "{}" }}) {{ clientMutationId }} }})", blockedInfo.NodeId, blockerInfo.NodeId);

        auto result = RunMutation(mutation);
        if (!result.Ok())
        {
            std::cout << std::format("{}  FAIL  blocked by #{} ({}) - {}{}\n", Colour::Red, blocker, blockerInfo.Title, result.Error, Colour::Reset);
            continue;
        }

        if (result.Data.contains("errors"))
        {
            auto msg = result.Data["errors"][0]["message"].get<std::string>();
            if (msg.find("already been taken") != std::string::npos)
            {
                std::cout << std::format("{}  SKIP  blocked by #{} ({}) - already exists{}\n", Colour::Gray, blocker, blockerInfo.Title, Colour::Reset);
            }
            else
            {
                std::cout << std::format("{}  FAIL  blocked by #{} ({}) - {}{}\n", Colour::Red, blocker, blockerInfo.Title, msg, Colour::Reset);
            }
        }
        else
        {
            std::cout << std::format("{}  OK    blocked by #{} ({}){}\n", Colour::Green, blocker, blockerInfo.Title, Colour::Reset);
        }
    }
}

static void AddBlocking(int blockingIssue, const std::vector<int>& blockedIssues)
{
    std::set<int> allNums{blockingIssue};
    allNums.insert(blockedIssues.begin(), blockedIssues.end());

    auto ids = GetIssueNodeIds(allNums);
    if (ids.empty())
    {
        return;
    }

    auto& blockingInfo = ids[blockingIssue];
    std::cout << std::format("{}Issue #{} ({}) now blocking:{}\n", Colour::Cyan, blockingIssue, blockingInfo.Title, Colour::Reset);

    for (int blocked : blockedIssues)
    {
        auto& blockedInfo = ids[blocked];
        auto mutation = std::format(R"(mutation {{ addBlockedBy(input: {{ issueId: "{}", blockingIssueId: "{}" }}) {{ clientMutationId }} }})", blockedInfo.NodeId, blockingInfo.NodeId);

        auto result = RunMutation(mutation);
        if (!result.Ok())
        {
            std::cout << std::format("{}  FAIL  #{} ({}) - {}{}\n", Colour::Red, blocked, blockedInfo.Title, result.Error, Colour::Reset);
            continue;
        }

        if (result.Data.contains("errors"))
        {
            auto msg = result.Data["errors"][0]["message"].get<std::string>();
            if (msg.find("already been taken") != std::string::npos)
            {
                std::cout << std::format("{}  SKIP  #{} ({}) - already exists{}\n", Colour::Gray, blocked, blockedInfo.Title, Colour::Reset);
            }
            else
            {
                std::cout << std::format("{}  FAIL  #{} ({}) - {}{}\n", Colour::Red, blocked, blockedInfo.Title, msg, Colour::Reset);
            }
        }
        else
        {
            std::cout << std::format("{}  OK    #{} ({}){}\n", Colour::Green, blocked, blockedInfo.Title, Colour::Reset);
        }
    }
}

static void AddSubIssue(int parentIssue, const std::vector<int>& childIssues)
{
    std::set<int> allNums{parentIssue};
    allNums.insert(childIssues.begin(), childIssues.end());

    auto ids = GetIssueNodeIds(allNums);
    if (ids.empty())
    {
        return;
    }

    auto& parentInfo = ids[parentIssue];
    std::cout << std::format("{}Parent #{} ({}){}\n", Colour::Cyan, parentIssue, parentInfo.Title, Colour::Reset);

    for (int child : childIssues)
    {
        auto& childInfo = ids[child];
        auto mutation = std::format(R"(mutation {{ addSubIssue(input: {{ issueId: "{}", subIssueId: "{}" }}) {{ clientMutationId }} }})", parentInfo.NodeId, childInfo.NodeId);

        auto result = RunMutation(mutation);
        if (!result.Ok())
        {
            std::cout << std::format("{}  FAIL  sub-issue #{} ({}) - {}{}\n", Colour::Red, child, childInfo.Title, result.Error, Colour::Reset);
            continue;
        }

        if (result.Data.contains("errors"))
        {
            auto msg = result.Data["errors"][0]["message"].get<std::string>();
            if (msg.find("already") != std::string::npos)
            {
                std::cout << std::format("{}  SKIP  sub-issue #{} ({}) - already exists{}\n", Colour::Gray, child, childInfo.Title, Colour::Reset);
            }
            else
            {
                std::cout << std::format("{}  FAIL  sub-issue #{} ({}) - {}{}\n", Colour::Red, child, childInfo.Title, msg, Colour::Reset);
            }
        }
        else
        {
            std::cout << std::format("{}  OK    sub-issue #{} ({}){}\n", Colour::Green, child, childInfo.Title, Colour::Reset);
        }
    }
}

static void RemoveBlockedBy(int blockedIssue, const std::vector<int>& blockingIssues)
{
    std::set<int> allNums{blockedIssue};
    allNums.insert(blockingIssues.begin(), blockingIssues.end());

    auto ids = GetIssueNodeIds(allNums);
    if (ids.empty())
    {
        return;
    }

    auto& blockedInfo = ids[blockedIssue];
    std::cout << std::format("{}Issue #{} ({}){}\n", Colour::Cyan, blockedIssue, blockedInfo.Title, Colour::Reset);

    for (int blocker : blockingIssues)
    {
        auto& blockerInfo = ids[blocker];
        auto mutation = std::format(R"(mutation {{ removeBlockedBy(input: {{ issueId: "{}", blockingIssueId: "{}" }}) {{ clientMutationId }} }})", blockedInfo.NodeId, blockerInfo.NodeId);

        auto result = RunMutation(mutation);
        if (!result.Ok())
        {
            std::cout << std::format("{}  FAIL  remove blocked-by #{} ({}) - {}{}\n", Colour::Red, blocker, blockerInfo.Title, result.Error, Colour::Reset);
            continue;
        }

        if (result.Data.contains("errors"))
        {
            std::cout << std::format("{}  FAIL  remove blocked-by #{} ({}) - {}{}\n", Colour::Red, blocker, blockerInfo.Title, result.Data["errors"][0]["message"].get<std::string>(), Colour::Reset);
        }
        else
        {
            std::cout << std::format("{}  OK    removed blocked-by #{} ({}){}\n", Colour::Green, blocker, blockerInfo.Title, Colour::Reset);
        }
    }
}

static void RemoveBlocking(int blockingIssue, const std::vector<int>& blockedIssues)
{
    std::set<int> allNums{blockingIssue};
    allNums.insert(blockedIssues.begin(), blockedIssues.end());

    auto ids = GetIssueNodeIds(allNums);
    if (ids.empty())
    {
        return;
    }

    auto& blockingInfo = ids[blockingIssue];
    std::cout << std::format("{}Issue #{} ({}) removing blocking:{}\n", Colour::Cyan, blockingIssue, blockingInfo.Title, Colour::Reset);

    for (int blocked : blockedIssues)
    {
        auto& blockedInfo = ids[blocked];
        auto mutation = std::format(R"(mutation {{ removeBlockedBy(input: {{ issueId: "{}", blockingIssueId: "{}" }}) {{ clientMutationId }} }})", blockedInfo.NodeId, blockingInfo.NodeId);

        auto result = RunMutation(mutation);
        if (!result.Ok())
        {
            std::cout << std::format("{}  FAIL  remove blocking #{} ({}) - {}{}\n", Colour::Red, blocked, blockedInfo.Title, result.Error, Colour::Reset);
            continue;
        }

        if (result.Data.contains("errors"))
        {
            std::cout << std::format("{}  FAIL  remove blocking #{} ({}) - {}{}\n", Colour::Red, blocked, blockedInfo.Title, result.Data["errors"][0]["message"].get<std::string>(), Colour::Reset);
        }
        else
        {
            std::cout << std::format("{}  OK    removed blocking #{} ({}){}\n", Colour::Green, blocked, blockedInfo.Title, Colour::Reset);
        }
    }
}

static void RemoveSubIssue(int parentIssue, const std::vector<int>& childIssues)
{
    std::set<int> allNums{parentIssue};
    allNums.insert(childIssues.begin(), childIssues.end());

    auto ids = GetIssueNodeIds(allNums);
    if (ids.empty())
    {
        return;
    }

    auto& parentInfo = ids[parentIssue];
    std::cout << std::format("{}Parent #{} ({}){}\n", Colour::Cyan, parentIssue, parentInfo.Title, Colour::Reset);

    for (int child : childIssues)
    {
        auto& childInfo = ids[child];
        auto mutation = std::format(R"(mutation {{ removeSubIssue(input: {{ issueId: "{}", subIssueId: "{}" }}) {{ clientMutationId }} }})", parentInfo.NodeId, childInfo.NodeId);

        auto result = RunMutation(mutation);
        if (!result.Ok())
        {
            std::cout << std::format("{}  FAIL  remove sub-issue #{} ({}) - {}{}\n", Colour::Red, child, childInfo.Title, result.Error, Colour::Reset);
            continue;
        }

        if (result.Data.contains("errors"))
        {
            std::cout << std::format("{}  FAIL  remove sub-issue #{} ({}) - {}{}\n", Colour::Red, child, childInfo.Title, result.Data["errors"][0]["message"].get<std::string>(), Colour::Reset);
        }
        else
        {
            std::cout << std::format("{}  OK    removed sub-issue #{} ({}){}\n", Colour::Green, child, childInfo.Title, Colour::Reset);
        }
    }
}

// --- Query Commands ---

static void ShowRelationships(int issueNumber)
{
    auto query = std::format(R"(query {{
  repository(owner: "{}", name: "{}") {{
    issue(number: {}) {{
      title state
      labels(first: 20) {{ nodes {{ name }} }}
      assignees(first: 10) {{ nodes {{ login }} }}
      blockedBy(first: 50) {{ nodes {{ number title state }} }}
      blocking(first: 50) {{ nodes {{ number title state }} }}
      subIssues(first: 50) {{ nodes {{ number title state }} }}
      parent {{ number title state }}
    }}
  }}
}})",
    OWNER, REPO, issueNumber);

    auto result = RunGraphQL(query, "ShowRelationships");
    if (!result.Ok())
    {
        std::cerr << result.Error << "\n";
        return;
    }

    if (result.Data.contains("errors"))
    {
        std::cout << std::format("{}Note: Some relationship fields may not be available on your GitHub plan.{}\n", Colour::Yellow, Colour::Reset);
        std::cerr << result.Data["errors"][0]["message"] << "\n";
        return;
    }

    auto& issue = result.Data["data"]["repository"]["issue"];
    if (issue.is_null())
    {
        std::cerr << std::format("Issue not found or inaccessible: #{}\n", issueNumber);
        return;
    }

    auto state = issue["state"].get<std::string>();
    auto title = issue["title"].get<std::string>();
    auto stateColour = (state == "CLOSED") ? Colour::Green : Colour::White;

    std::cout << std::format("\n{}  #{} - {} {}[{}]{}\n", Colour::Cyan, issueNumber, title, stateColour, state, Colour::Reset);

    // Labels
    auto& labels = issue["labels"]["nodes"];
    if (!labels.empty())
    {
        std::string labelStr;
        for (auto& l : labels)
        {
            if (!labelStr.empty())
            {
                labelStr += ", ";
            }
            labelStr += l["name"].get<std::string>();
        }
        std::cout << std::format("{}  Labels: {}{}\n", Colour::Gray, labelStr, Colour::Reset);
    }

    // Assignees
    auto& assignees = issue["assignees"]["nodes"];
    if (!assignees.empty())
    {
        std::string assigneeStr;
        for (auto& a : assignees)
        {
            if (!assigneeStr.empty())
            {
                assigneeStr += ", ";
            }
            assigneeStr += "@" + a["login"].get<std::string>();
        }
        std::cout << std::format("{}  Assigned: {}{}\n", Colour::Gray, assigneeStr, Colour::Reset);
    }

    std::cout << "\n";

    // Parent
    if (!issue["parent"].is_null())
    {
        auto parentState = issue["parent"]["state"].get<std::string>();
        auto parentIcon = (parentState == "CLOSED") ? "[x]" : "[ ]";
        auto parentColour = (parentState == "CLOSED") ? Colour::Gray : Colour::Magenta;
        std::cout << std::format("{}  Parent: {} #{} - {}{}\n", parentColour, parentIcon, issue["parent"]["number"].get<int>(), issue["parent"]["title"].get<std::string>(), Colour::Reset);
    }

    // Blocked by
    auto& blockedByNodes = issue["blockedBy"]["nodes"];
    if (!blockedByNodes.empty())
    {
        int resolvedCount = 0;
        for (auto& b : blockedByNodes)
        {
            if (b["state"] == "CLOSED")
            {
                resolvedCount++;
            }
        }
        int totalCount = static_cast<int>(blockedByNodes.size());

        if (resolvedCount == totalCount)
        {
            std::cout << std::format("{}  Blocked by (ALL RESOLVED):{}\n", Colour::Green, Colour::Reset);
        }
        else
        {
            std::cout << std::format("{}  Blocked by ({}/{} resolved):{}\n", Colour::Yellow, resolvedCount, totalCount, Colour::Reset);
        }

        for (auto& b : blockedByNodes)
        {
            auto bState = b["state"].get<std::string>();
            auto icon = (bState == "CLOSED") ? "[x]" : "[ ]";
            auto colour = (bState == "CLOSED") ? Colour::Gray : Colour::White;
            std::cout << std::format("{}    {} #{} - {}{}\n", colour, icon, b["number"].get<int>(), b["title"].get<std::string>(), Colour::Reset);
        }
    }

    // Blocking
    auto& blockingNodes = issue["blocking"]["nodes"];
    if (!blockingNodes.empty())
    {
        int doneCount = 0;
        for (auto& b : blockingNodes)
        {
            if (b["state"] == "CLOSED")
            {
                doneCount++;
            }
        }
        int totalCount = static_cast<int>(blockingNodes.size());

        std::cout << std::format("{}  Blocking ({}/{} done):{}\n", Colour::Yellow, doneCount, totalCount, Colour::Reset);
        for (auto& b : blockingNodes)
        {
            auto bState = b["state"].get<std::string>();
            auto icon = (bState == "CLOSED") ? "[x]" : "[ ]";
            auto colour = (bState == "CLOSED") ? Colour::Gray : Colour::White;
            std::cout << std::format("{}    {} #{} - {}{}\n", colour, icon, b["number"].get<int>(), b["title"].get<std::string>(), Colour::Reset);
        }
    }

    // Sub-issues
    auto& subNodes = issue["subIssues"]["nodes"];
    if (!subNodes.empty())
    {
        int doneCount = 0;
        for (auto& s : subNodes)
        {
            if (s["state"] == "CLOSED")
            {
                doneCount++;
            }
        }
        int totalCount = static_cast<int>(subNodes.size());

        auto subColour = (doneCount == totalCount) ? Colour::Green : Colour::Yellow;
        std::cout << std::format("{}  Sub-issues ({}/{} complete):{}\n", subColour, doneCount, totalCount, Colour::Reset);
        for (auto& s : subNodes)
        {
            auto sState = s["state"].get<std::string>();
            auto icon = (sState == "CLOSED") ? "[x]" : "[ ]";
            auto colour = (sState == "CLOSED") ? Colour::Gray : Colour::White;
            std::cout << std::format("{}    {} #{} - {}{}\n", colour, icon, s["number"].get<int>(), s["title"].get<std::string>(), Colour::Reset);
        }
    }

    bool hasAny = !issue["parent"].is_null() || !blockedByNodes.empty() || !blockingNodes.empty() || !subNodes.empty();
    if (!hasAny)
    {
        std::cout << std::format("{}  No relationships found.{}\n", Colour::Gray, Colour::Reset);
    }

    // Overall status
    std::cout << "\n";
    if (state == "CLOSED")
    {
        std::cout << std::format("{}  Status: DONE{}\n", Colour::Green, Colour::Reset);
    }
    else if (!blockedByNodes.empty())
    {
        int unresolvedCount = 0;
        for (auto& b : blockedByNodes)
        {
            if (b["state"] != "CLOSED")
            {
                unresolvedCount++;
            }
        }
        if (unresolvedCount > 0)
        {
            auto plural = (unresolvedCount != 1) ? "s" : "";
            std::cout << std::format("{}  Status: BLOCKED ({} unresolved blocker{}){}\n", Colour::Red, unresolvedCount, plural, Colour::Reset);
        }
        else
        {
            std::cout << std::format("{}  Status: READY (all blockers resolved){}\n", Colour::Green, Colour::Reset);
        }
    }
    else
    {
        std::cout << std::format("{}  Status: READY{}\n", Colour::Green, Colour::Reset);
    }
    std::cout << "\n";
}

static void ShowBatchSummary(const std::vector<int>& issueNumbers)
{
    std::set<int> unique(issueNumbers.begin(), issueNumbers.end());

    std::string fields;
    for (int num : unique)
    {
        if (!fields.empty())
        {
            fields += " ";
        }
        fields += std::format("i{}: issue(number: {}) {{ number title state labels(first: 10) {{ nodes {{ name }} }} "
                              "assignees(first: 5) {{ nodes {{ login }} }} blockedBy(first: 50) {{ nodes {{ state }} "
                              "}} subIssues(first: 50) {{ nodes {{ state }} }} }}",
        num, num);
    }
    auto query = std::format(R"(query {{ repository(owner: "{}", name: "{}") {{ {} }} }})", OWNER, REPO, fields);

    auto result = RunGraphQL(query, "ShowBatchSummary");
    if (!result.Ok())
    {
        std::cerr << result.Error << "\n";
        return;
    }

    if (result.Data.contains("errors"))
    {
        std::cerr << "GraphQL error: " << result.Data["errors"][0]["message"] << "\n";
        return;
    }

    std::cout << std::format("\n{}  #       State    Status     Sub-issues  Title{}\n", Colour::Cyan, Colour::Reset);
    std::cout << std::format("{}  {}{}\n", Colour::Gray, std::string(75, '-'), Colour::Reset);

    for (int num : unique)
    {
        auto key = std::format("i{}", num);
        auto& iss = result.Data["data"]["repository"][key];
        if (iss.is_null())
        {
            continue;
        }

        auto issState = iss["state"].get<std::string>();
        auto issTitle = iss["title"].get<std::string>();

        int unresolvedBlockers = 0;
        for (auto& b : iss["blockedBy"]["nodes"])
        {
            if (b["state"] != "CLOSED")
            {
                unresolvedBlockers++;
            }
        }

        std::string statusStr;
        std::string_view statusColour;
        if (issState == "CLOSED")
        {
            statusStr = "DONE";
            statusColour = Colour::Green;
        }
        else if (unresolvedBlockers > 0)
        {
            statusStr = "BLOCKED";
            statusColour = Colour::Red;
        }
        else
        {
            statusStr = "READY";
            statusColour = Colour::Green;
        }

        auto& subs = iss["subIssues"]["nodes"];
        std::string subStr = "-";
        if (!subs.empty())
        {
            int subDone = 0;
            for (auto& s : subs)
            {
                if (s["state"] == "CLOSED")
                {
                    subDone++;
                }
            }
            subStr = std::format("{}/{}", subDone, subs.size());
        }

        auto stateColour = (issState == "CLOSED") ? Colour::Gray : Colour::White;

        std::cout << std::format(
        "  {}{}{}{}{}{}{}  {}{}\n", stateColour, PadRight(std::format("#{}", num), 8), PadRight(issState, 9), statusColour, PadRight(statusStr, 11), stateColour, PadRight(subStr, 12), issTitle, Colour::Reset);
    }
    std::cout << "\n";
}

static void ShowReady()
{
    auto ghResult = RunGh(std::format("issue list --repo {}/{} --state open --limit 200 --json number,title,state,labels", OWNER, REPO), "ShowReady");
    if (!ghResult.Ok())
    {
        std::cerr << ghResult.Error << "\n";
        return;
    }

    auto& issues = ghResult.Data;
    if (issues.empty())
    {
        std::cout << std::format("\n{}  No open issues found.{}\n\n", Colour::Yellow, Colour::Reset);
        return;
    }

    // Build GraphQL query for blocker/label/assignee info
    std::string fields;
    for (auto& iss : issues)
    {
        int num = iss["number"].get<int>();
        if (!fields.empty())
        {
            fields += " ";
        }
        fields += std::format("i{}: issue(number: {}) {{ number title state labels(first: 10) {{ nodes {{ name }} }} "
                              "assignees(first: 5) {{ nodes {{ login }} }} blockedBy(first: 50) {{ nodes {{ state }} }} }}",
        num, num);
    }
    auto query = std::format(R"(query {{ repository(owner: "{}", name: "{}") {{ {} }} }})", OWNER, REPO, fields);
    auto gqlResult = RunGraphQL(query, "ShowReady");
    if (!gqlResult.Ok())
    {
        std::cerr << gqlResult.Error << "\n";
        return;
    }

    if (gqlResult.Data.contains("errors"))
    {
        std::cerr << "GraphQL error: " << gqlResult.Data["errors"][0]["message"] << "\n";
        return;
    }

    // Filter to ready issues (no unresolved blockers)
    struct ReadyIssue
    {
        int Number;
        std::string Title;
        std::string Labels;
        std::string Assignees;
    };
    std::vector<ReadyIssue> readyIssues;

    for (auto& iss : issues)
    {
        int num = iss["number"].get<int>();
        auto key = std::format("i{}", num);
        auto& gqlIs = gqlResult.Data["data"]["repository"][key];
        if (gqlIs.is_null())
        {
            continue;
        }

        int unresolvedBlockers = 0;
        for (auto& b : gqlIs["blockedBy"]["nodes"])
        {
            if (b["state"] != "CLOSED")
            {
                unresolvedBlockers++;
            }
        }

        if (unresolvedBlockers == 0)
        {
            std::string labels;
            for (auto& l : gqlIs["labels"]["nodes"])
            {
                if (!labels.empty())
                {
                    labels += ", ";
                }
                labels += l["name"].get<std::string>();
            }
            if (labels.size() > 30)
            {
                labels = labels.substr(0, 27) + "...";
            }

            std::string assigneeStr;
            for (auto& a : gqlIs["assignees"]["nodes"])
            {
                if (!assigneeStr.empty())
                {
                    assigneeStr += ", ";
                }
                assigneeStr += "@" + a["login"].get<std::string>();
            }
            if (assigneeStr.empty())
            {
                assigneeStr = "-";
            }

            readyIssues.push_back({
            .Number = gqlIs["number"].get<int>(),
            .Title = gqlIs["title"].get<std::string>(),
            .Labels = std::move(labels),
            .Assignees = std::move(assigneeStr),
            });
        }
    }

    std::sort(readyIssues.begin(), readyIssues.end(), [](auto& a, auto& b)
    {
        return a.Number < b.Number;
    });

    std::cout << "\n";
    if (readyIssues.empty())
    {
        std::cout << std::format("{}  No ready issues found. Everything is blocked!{}\n", Colour::Yellow, Colour::Reset);
    }
    else
    {
        std::cout << std::format("{}  Ready to work ({} issues):{}\n\n", Colour::Green, readyIssues.size(), Colour::Reset);
        std::cout << std::format("{}  #       Labels                          Assigned   Title{}\n", Colour::Cyan, Colour::Reset);
        std::cout << std::format("{}  {}{}\n", Colour::Gray, std::string(80, '-'), Colour::Reset);

        for (auto& r : readyIssues)
        {
            std::cout << std::format(
            "  {}{}{}{}{}{}  {}{}\n", Colour::White, PadRight(std::format("#{}", r.Number), 8), Colour::Gray, PadRight(r.Labels, 32), PadRight(r.Assignees, 11), Colour::White, r.Title, Colour::Reset);
        }
    }
    std::cout << "\n";
}

static void ShowMilestoneStatus(const std::string& milestoneTitle)
{
    auto ghResult = RunGh(std::format("issue list --repo {}/{} --milestone \"{}\" --state all --limit 200 --json number,title,state,labels", OWNER, REPO, milestoneTitle), "ShowMilestoneStatus");
    if (!ghResult.Ok())
    {
        std::cerr << ghResult.Error << "\n";
        return;
    }

    auto& issues = ghResult.Data;
    if (issues.empty())
    {
        std::cout << std::format("{}No issues found for milestone '{}'.{}\n", Colour::Yellow, milestoneTitle, Colour::Reset);
        return;
    }

    std::vector<json*> openIssues, closedIssues;
    for (auto& iss : issues)
    {
        if (iss["state"] == "OPEN" || iss["state"] == "open")
        {
            openIssues.push_back(&iss);
        }
        else
        {
            closedIssues.push_back(&iss);
        }
    }

    int total = static_cast<int>(issues.size());
    int doneCount = static_cast<int>(closedIssues.size());
    int pct = static_cast<int>(std::round(static_cast<double>(doneCount) / total * 100));

    // Progress bar
    int barWidth = 40;
    int filled = static_cast<int>(std::round(static_cast<double>(doneCount) / total * barWidth));
    int empty = barWidth - filled;
    std::string bar = std::string(filled, '#') + std::string(empty, '.');
    auto barColour = (pct == 100) ? Colour::Green : (pct >= 50) ? Colour::Yellow : Colour::White;

    std::cout << std::format("\n{}  Milestone: {}{}\n", Colour::Cyan, milestoneTitle, Colour::Reset);
    std::cout << std::format("{}  {} {}% ({}/{}){}\n\n", barColour, bar, pct, doneCount, total, Colour::Reset);

    // Sort helpers
    auto sortByNumber = [](json* a, json* b)
    {
        return (*a)["number"].get<int>() < (*b)["number"].get<int>();
    };

    if (!openIssues.empty())
    {
        std::sort(openIssues.begin(), openIssues.end(), sortByNumber);
        std::cout << std::format("{}  Open ({}):{}\n", Colour::Yellow, openIssues.size(), Colour::Reset);
        for (auto* iss : openIssues)
        {
            std::string labels;
            for (auto& l : (*iss)["labels"])
            {
                if (!labels.empty())
                {
                    labels += ", ";
                }
                labels += l["name"].get<std::string>();
            }
            auto labelSuffix = labels.empty() ? "" : std::format(" [{}]", labels);
            std::cout << std::format("{}    [ ] #{} - {}{}{}\n", Colour::White, (*iss)["number"].get<int>(), (*iss)["title"].get<std::string>(), labelSuffix, Colour::Reset);
        }
    }

    if (!closedIssues.empty())
    {
        std::sort(closedIssues.begin(), closedIssues.end(), sortByNumber);
        std::cout << std::format("{}  Closed ({}):{}\n", Colour::Green, closedIssues.size(), Colour::Reset);
        for (auto* iss : closedIssues)
        {
            std::cout << std::format("{}    [x] #{} - {}{}\n", Colour::Gray, (*iss)["number"].get<int>(), (*iss)["title"].get<std::string>(), Colour::Reset);
        }
    }
    std::cout << "\n";
}

static void ShowOrphans()
{
    auto ghResult = RunGh(std::format("issue list --repo {}/{} --state open --limit 200 --no-milestone --json number,title,labels", OWNER, REPO), "ShowOrphans");
    if (!ghResult.Ok())
    {
        std::cerr << ghResult.Error << "\n";
        return;
    }

    auto& issues = ghResult.Data;
    if (issues.empty())
    {
        std::cout << std::format("\n{}  No open issues without a milestone.{}\n\n", Colour::Green, Colour::Reset);
        return;
    }

    // Check which have no parent
    std::string fields;
    for (auto& iss : issues)
    {
        int num = iss["number"].get<int>();
        if (!fields.empty())
        {
            fields += " ";
        }
        fields += std::format("i{}: issue(number: {}) {{ number title parent {{ number }} labels(first: 10) {{ nodes {{ name }} }} }}", num, num);
    }
    auto query = std::format(R"(query {{ repository(owner: "{}", name: "{}") {{ {} }} }})", OWNER, REPO, fields);
    auto gqlResult = RunGraphQL(query, "ShowOrphans");
    if (!gqlResult.Ok())
    {
        std::cerr << gqlResult.Error << "\n";
        return;
    }

    if (gqlResult.Data.contains("errors"))
    {
        std::cerr << "GraphQL error: " << gqlResult.Data["errors"][0]["message"] << "\n";
        return;
    }

    struct OrphanIssue
    {
        int Number;
        std::string Title;
        std::string Labels;
    };
    std::vector<OrphanIssue> orphans;

    for (auto& iss : issues)
    {
        int num = iss["number"].get<int>();
        auto key = std::format("i{}", num);
        auto& gqlIs = gqlResult.Data["data"]["repository"][key];
        if (gqlIs.is_null())
        {
            continue;
        }

        if (gqlIs["parent"].is_null())
        {
            std::string labels;
            for (auto& l : gqlIs["labels"]["nodes"])
            {
                if (!labels.empty())
                {
                    labels += ", ";
                }
                labels += l["name"].get<std::string>();
            }
            orphans.push_back({
            .Number = gqlIs["number"].get<int>(),
            .Title = gqlIs["title"].get<std::string>(),
            .Labels = std::move(labels),
            });
        }
    }

    std::sort(orphans.begin(), orphans.end(), [](auto& a, auto& b)
    {
        return a.Number < b.Number;
    });

    std::cout << "\n";
    if (orphans.empty())
    {
        std::cout << std::format("{}  No orphaned issues found. Everything has a parent or milestone.{}\n", Colour::Green, Colour::Reset);
    }
    else
    {
        std::cout << std::format("{}  Orphaned issues ({} -- no parent, no milestone):{}\n\n", Colour::Yellow, orphans.size(), Colour::Reset);
        for (auto& o : orphans)
        {
            auto labelSuffix = o.Labels.empty() ? "" : std::format(" [{}]", o.Labels);
            std::cout << std::format("{}    [ ] #{} - {}{}{}\n", Colour::White, o.Number, o.Title, labelSuffix, Colour::Reset);
        }
    }
    std::cout << "\n";
}

static void ShowChain(int issueNumber)
{
    std::set<int> visited;
    std::vector<ChainEntry> chain;

    std::function<void(int, int)> walkBlockers = [&](int num, int depth)
    {
        if (visited.contains(num))
        {
            return;
        }
        if (depth >= MAX_RECURSION_DEPTH)
        {
            std::cout << std::format("{}  Walk-Blockers: depth cap ({}) reached at #{} — stopping{}\n", Colour::Yellow, MAX_RECURSION_DEPTH, num, Colour::Reset);
            return;
        }
        visited.insert(num);

        auto query = std::format(R"(query {{
  repository(owner: "{}", name: "{}") {{
    issue(number: {}) {{
      number title state
      blockedBy(first: 50) {{ nodes {{ number title state }} }}
    }}
  }}
}})",
        OWNER, REPO, num);

        auto result = RunGraphQL(query, "ShowChain");
        if (!result.Ok())
        {
            if (depth == 0)
            {
                std::cerr << std::format("ShowChain: error for root issue #{}: {}\n", num, result.Error);
            }
            return;
        }

        if (result.Data.contains("errors"))
        {
            if (depth == 0)
            {
                std::cerr << std::format("ShowChain: GraphQL error for root issue #{}: {}\n", num, result.Data["errors"][0]["message"].get<std::string>());
            }
            return;
        }

        auto& issue = result.Data["data"]["repository"]["issue"];
        if (issue.is_null())
        {
            if (depth == 0)
            {
                std::cerr << std::format("ShowChain: Issue not found or inaccessible: #{}\n", num);
            }
            return;
        }

        chain.push_back({
        .Number = issue["number"].get<int>(),
        .Title = issue["title"].get<std::string>(),
        .State = issue["state"].get<std::string>(),
        .Depth = depth,
        });

        for (auto& b : issue["blockedBy"]["nodes"])
        {
            walkBlockers(b["number"].get<int>(), depth + 1);
        }
    };

    walkBlockers(issueNumber, 0);

    std::cout << std::format("\n{}  Dependency chain for #{}:{}\n\n", Colour::Cyan, issueNumber, Colour::Reset);

    if (chain.size() <= 1)
    {
        std::cout << std::format("{}  No blockers in the chain. Issue is independent.{}\n\n", Colour::Green, Colour::Reset);
        return;
    }

    int maxDepth = 0;
    for (auto& e : chain)
    {
        if (e.Depth > maxDepth)
        {
            maxDepth = e.Depth;
        }
    }

    for (auto& entry : chain)
    {
        auto indent = std::string(entry.Depth * 4, ' ');
        auto icon = (entry.State == "CLOSED") ? "[x]" : "[ ]";
        auto colour = (entry.State == "CLOSED") ? Colour::Gray : (entry.Depth == maxDepth) ? Colour::Red : Colour::White;
        auto prefix = (entry.Depth == 0) ? std::string("") : std::string("blocked by -> ");
        std::cout << std::format("{}  {}{}{} #{} - {}{}\n", colour, indent, prefix, icon, entry.Number, entry.Title, Colour::Reset);
    }

    // Summary
    int totalUnresolved = 0;
    std::vector<ChainEntry*> unresolvedLeaves;
    for (auto& e : chain)
    {
        if (e.State != "CLOSED" && e.Depth > 0)
        {
            totalUnresolved++;
        }
        if (e.Depth == maxDepth && e.State != "CLOSED")
        {
            unresolvedLeaves.push_back(&e);
        }
    }

    std::cout << "\n";
    if (totalUnresolved == 0)
    {
        std::cout << std::format("{}  All blockers resolved! Issue is ready.{}\n", Colour::Green, Colour::Reset);
    }
    else
    {
        std::cout << std::format("{}  {} unresolved blocker(s) in chain, depth {}{}\n", Colour::Yellow, totalUnresolved, maxDepth, Colour::Reset);
        if (!unresolvedLeaves.empty())
        {
            std::string leafNums;
            for (auto* leaf : unresolvedLeaves)
            {
                if (!leafNums.empty())
                {
                    leafNums += ", ";
                }
                leafNums += std::format("#{}", leaf->Number);
            }
            std::cout << std::format("{}  Start here: {}{}\n", Colour::Red, leafNums, Colour::Reset);
        }
    }
    std::cout << "\n";
}

// --- Tree ---

struct TreeCounts
{
    int Done = 0;
    int Total = 0;
};

static TreeCounts GetTreeCounts(const std::vector<TreeNode>& nodes)
{
    TreeCounts counts;
    for (auto& n : nodes)
    {
        counts.Total++;
        if (n.State == "CLOSED")
        {
            counts.Done++;
        }
        if (!n.Children.empty())
        {
            auto sub = GetTreeCounts(n.Children);
            counts.Done += sub.Done;
            counts.Total += sub.Total;
        }
    }
    return counts;
}

static TreeNode FetchSubIssueTree(int num, int depth)
{
    if (depth > MAX_RECURSION_DEPTH)
    {
        std::cout << std::format("{}  ShowTree: depth cap reached at #{} — stopping{}\n", Colour::Yellow, num, Colour::Reset);
        return {};
    }

    auto query = std::format(R"(query {{
  repository(owner: "{}", name: "{}") {{
    issue(number: {}) {{
      number title state
      subIssues(first: 50) {{
        nodes {{ number title state }}
      }}
    }}
  }}
}})",
    OWNER, REPO, num);

    auto result = RunGraphQL(query, "ShowTree");
    if (!result.Ok())
    {
        std::cerr << result.Error << "\n";
        return {};
    }

    if (result.Data.contains("errors"))
    {
        std::cout << std::format("{}Note: Some fields may not be available on your GitHub plan.{}\n", Colour::Yellow, Colour::Reset);
        std::cerr << result.Data["errors"][0]["message"] << "\n";
        return {};
    }

    auto& issue = result.Data["data"]["repository"]["issue"];
    if (issue.is_null())
    {
        return {};
    }

    TreeNode node{
    .Number = issue["number"].get<int>(),
    .Title = issue["title"].get<std::string>(),
    .State = issue["state"].get<std::string>(),
    };

    for (auto& child : issue["subIssues"]["nodes"])
    {
        auto childTree = FetchSubIssueTree(child["number"].get<int>(), depth + 1);
        if (childTree.Number != 0)
        {
            node.Children.push_back(std::move(childTree));
        }
        else
        {
            // Leaf node or fetch failed
            node.Children.push_back({
            .Number = child["number"].get<int>(),
            .Title = child["title"].get<std::string>(),
            .State = child["state"].get<std::string>(),
            });
        }
    }

    return node;
}

static void PrintTreeNode(const TreeNode& node, int depth)
{
    auto indent = std::string(depth * 4, ' ');
    auto icon = (node.State == "CLOSED") ? "[x]" : "[ ]";

    std::string_view colour;
    if (depth == 0)
    {
        colour = (node.State == "CLOSED") ? Colour::Green : Colour::Cyan;
    }
    else
    {
        colour = (node.State == "CLOSED") ? Colour::Gray : Colour::White;
    }

    std::string childProgress;
    if (!node.Children.empty())
    {
        auto counts = GetTreeCounts(node.Children);
        childProgress = std::format(" ({}/{})", counts.Done, counts.Total);
    }

    std::cout << std::format("{}  {}{} #{} - {}{}{}\n", colour, indent, icon, node.Number, node.Title, childProgress, Colour::Reset);

    for (auto& child : node.Children)
    {
        PrintTreeNode(child, depth + 1);
    }
}

static void ShowTree(int issueNumber)
{
    auto tree = FetchSubIssueTree(issueNumber, 0);
    if (tree.Number == 0)
    {
        std::cerr << std::format("Issue not found or inaccessible: #{}\n", issueNumber);
        return;
    }

    std::cout << "\n";
    PrintTreeNode(tree, 0);
    std::cout << "\n";
}

// --- Argument Parsing ---

/// Parses a comma-separated list of integers from a string.
static std::vector<int> ParseIntList(const std::string& s)
{
    std::vector<int> result;
    std::istringstream stream(s);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        try
        {
            result.push_back(std::stoi(token));
        }
        catch (...)
        {
            std::cerr << std::format("Invalid issue number: '{}'\n", token);
            return {};
        }
    }
    return result;
}

static void PrintUsage(const char* progName)
{
    std::cout << std::format(R"(Usage: {} <action> [issue] [target] [--milestone <name>]

Actions:
  blocked-by <issue> <targets>        Mark issue as blocked by target(s)
  blocking <issue> <targets>          Mark issue as blocking target(s)
  sub-issue <parent> <children>       Add sub-issue(s) under parent
  remove-blocked-by <issue> <targets> Remove blocked-by relationship(s)
  remove-blocking <issue> <targets>   Remove blocking relationship(s)
  remove-sub-issue <parent> <children> Remove sub-issue(s) from parent
  show <issue>                        Show relationships for issue(s)
  tree <issue>                        Show sub-issue hierarchy
  ready                               List unblocked open issues
  status --milestone <name>           Show milestone progress
  orphans                             Find issues with no parent/milestone
  chain <issue>                       Walk blocked-by dependency chain

Issues and targets can be comma-separated: 12,15,20

Examples:
  {} blocked-by 12 7           # #12 is blocked by #7
  {} blocking 7 12,15          # #7 blocks #12 and #15
  {} sub-issue 10 41,42        # #41, #42 are sub-issues of #10
  {} show 12                   # Show relationships for #12
  {} show 12,15,20             # Batch summary for #12, #15, #20
  {} tree 10                   # Sub-issue tree for #10
  {} ready                     # List unblocked issues
  {} status --milestone "v0.1" # Milestone progress
  {} orphans                   # Find orphaned issues
  {} chain 12                  # Dependency chain for #12
)",
    progName, progName, progName, progName, progName, progName, progName, progName, progName, progName, progName);
}

int main(int argc, char** argv)
{
    EnableAnsiOnWindows();
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string action = argv[1];

    // Parse optional --milestone flag
    std::string milestone;
    // Scan all args for --milestone and extract it
    int effectiveArgc = argc;
    for (int i = 2; i < argc; i++)
    {
        if (std::string(argv[i]) == "--milestone" || std::string(argv[i]) == "-m")
        {
            if (i + 1 < argc)
            {
                milestone = argv[i + 1];
                // Shift remaining args (remove these two from consideration)
                effectiveArgc = i;
                break;
            }
            else
            {
                std::cerr << "--milestone requires a value.\n";
                return 1;
            }
        }
    }

    // Commands that don't need issue numbers
    if (action == "ready")
    {
        ShowReady();
        return 0;
    }
    if (action == "orphans")
    {
        ShowOrphans();
        return 0;
    }
    if (action == "status")
    {
        if (milestone.empty())
        {
            std::cerr << "status requires --milestone. Example: gh-issues status --milestone \"v0.1\"\n";
            return 1;
        }
        ShowMilestoneStatus(milestone);
        return 0;
    }

    // Commands that need at least an issue number
    if (effectiveArgc < 3)
    {
        std::cerr << std::format("{} requires an issue number.\n", action);
        PrintUsage(argv[0]);
        return 1;
    }

    auto issues = ParseIntList(argv[2]);
    if (issues.empty())
    {
        return 1;
    }

    // show with multiple issues → batch summary
    if (action == "show")
    {
        if (issues.size() > 1)
        {
            ShowBatchSummary(issues);
        }
        else
        {
            ShowRelationships(issues[0]);
        }
        return 0;
    }

    if (action == "tree")
    {
        if (issues.size() > 1)
        {
            std::cerr << "tree accepts only a single issue number.\n";
            return 1;
        }
        ShowTree(issues[0]);
        return 0;
    }

    if (action == "chain")
    {
        if (issues.size() > 1)
        {
            std::cerr << "chain accepts only a single issue number.\n";
            return 1;
        }
        ShowChain(issues[0]);
        return 0;
    }

    // Commands that need issue + target
    if (effectiveArgc < 4)
    {
        std::cerr << std::format("{} requires target issue number(s).\n", action);
        return 1;
    }
    if (issues.size() > 1)
    {
        std::cerr << std::format("{} accepts only a single issue number.\n", action);
        return 1;
    }
    int singleIssue = issues[0];

    auto targets = ParseIntList(argv[3]);
    if (targets.empty())
    {
        return 1;
    }

    if (action == "blocked-by")
    {
        AddBlockedBy(singleIssue, targets);
    }
    else if (action == "blocking")
    {
        AddBlocking(singleIssue, targets);
    }
    else if (action == "sub-issue")
    {
        AddSubIssue(singleIssue, targets);
    }
    else if (action == "remove-blocked-by")
    {
        RemoveBlockedBy(singleIssue, targets);
    }
    else if (action == "remove-blocking")
    {
        RemoveBlocking(singleIssue, targets);
    }
    else if (action == "remove-sub-issue")
    {
        RemoveSubIssue(singleIssue, targets);
    }
    else
    {
        std::cerr << std::format("Unknown action: {}\n", action);
        PrintUsage(argv[0]);
        return 1;
    }

    return 0;
}
