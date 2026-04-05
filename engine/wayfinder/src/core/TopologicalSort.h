#pragma once

#include <format>
#include <functional>
#include <queue>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace Wayfinder
{

    /// Result of a topological sort operation.
    struct TopologicalSortResult
    {
        std::vector<size_t> Order; ///< Indices in topological order.
        bool HasCycle = false;
        std::string CyclePath; ///< Human-readable cycle description (empty if no cycle).
    };

    /**
     * @brief Standalone Kahn's algorithm for topological sorting.
     *
     * Reusable by SubsystemRegistry, SystemRegistrar, AppBuilder, and any
     * other system that needs dependency-ordered initialisation.
     *
     * @param nodeCount     Number of nodes in the graph.
     * @param adjacency     adjacency[i] = list of nodes that i points to (i -> dependent).
     * @param nameOf        Optional function returning a human-readable name for node i (used in cycle path).
     * @return TopologicalSortResult with the computed order, or cycle information on failure.
     */
    [[nodiscard]] inline auto TopologicalSort(size_t nodeCount, std::span<const std::vector<size_t>> adjacency, std::function<std::string(size_t)> nameOf = {}) -> TopologicalSortResult
    {
        // precondition: adjacency.size() == nodeCount
        if (adjacency.size() != nodeCount)
        {
            throw std::out_of_range(std::format("TopologicalSort: adjacency.size() ({}) != nodeCount ({})", adjacency.size(), nodeCount));
        }

        // Compute in-degree from adjacency, validating edge targets
        std::vector<size_t> inDegree(nodeCount, 0);
        for (size_t i = 0; i < nodeCount; ++i)
        {
            for (const size_t dependent : adjacency[i])
            {
                if (dependent >= nodeCount)
                {
                    throw std::out_of_range(std::format("TopologicalSort: edge target {} out of range [0, {})", dependent, nodeCount));
                }
                ++inDegree[dependent];
            }
        }

        // Kahn's algorithm
        std::queue<size_t> ready;
        for (size_t i = 0; i < nodeCount; ++i)
        {
            if (inDegree[i] == 0)
            {
                ready.push(i);
            }
        }

        TopologicalSortResult result;
        result.Order.reserve(nodeCount);

        while (not ready.empty())
        {
            const size_t current = ready.front();
            ready.pop();
            result.Order.push_back(current);

            for (const size_t dependent : adjacency[current])
            {
                --inDegree[dependent];
                if (inDegree[dependent] == 0)
                {
                    ready.push(dependent);
                }
            }
        }

        if (result.Order.size() != nodeCount)
        {
            result.HasCycle = true;

            // Extract cycle path from residual nodes using back-edge detection.
            // After Kahn's, all residual nodes have inDegree > 0.
            auto getName = [&](size_t idx) -> std::string
            {
                return nameOf ? nameOf(idx) : std::format("node[{}]", idx);
            };

            std::vector<int> pos(nodeCount, -1);
            std::vector<bool> globalVisited(nodeCount, false);
            std::vector<size_t> cyclePath;

            for (size_t s = 0; s < nodeCount and cyclePath.empty(); ++s)
            {
                if (inDegree[s] == 0 or globalVisited[s])
                {
                    continue;
                }

                std::vector<size_t> path;
                size_t current = s;

                while (not globalVisited[current] and pos[current] < 0)
                {
                    pos[current] = static_cast<int>(path.size());
                    path.push_back(current);

                    bool found = false;
                    for (const size_t next : adjacency[current])
                    {
                        if (inDegree[next] > 0 and not globalVisited[next])
                        {
                            current = next;
                            found = true;
                            break;
                        }
                    }

                    if (not found)
                    {
                        break;
                    }
                }

                if (pos[current] >= 0)
                {
                    // Back-edge found: cycle is path[pos[current]..end]
                    for (size_t i = static_cast<size_t>(pos[current]); i < path.size(); ++i)
                    {
                        cyclePath.push_back(path[i]);
                    }
                }

                // Mark visited and reset pos for next walk
                for (const size_t node : path)
                {
                    globalVisited[node] = true;
                    pos[node] = -1;
                }
            }

            // Build human-readable cycle string
            std::string cycleStr;
            for (const size_t node : cyclePath)
            {
                if (not cycleStr.empty())
                {
                    cycleStr += " -> ";
                }
                cycleStr += getName(node);
            }
            // Close the cycle
            if (not cyclePath.empty())
            {
                cycleStr += " -> ";
                cycleStr += getName(cyclePath.front());
            }

            result.CyclePath = std::move(cycleStr);
        }

        return result;
    }

} // namespace Wayfinder
