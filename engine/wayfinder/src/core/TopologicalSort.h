#pragma once

#include <format>
#include <functional>
#include <queue>
#include <span>
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
        // Compute in-degree from adjacency
        std::vector<size_t> inDegree(nodeCount, 0);
        for (size_t i = 0; i < nodeCount; ++i)
        {
            for (const size_t dependent : adjacency[i])
            {
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

            // Extract cycle path from residual nodes with non-zero in-degree
            size_t start = 0;
            for (size_t i = 0; i < nodeCount; ++i)
            {
                if (inDegree[i] > 0)
                {
                    start = i;
                    break;
                }
            }

            // Follow edges to trace the cycle
            std::vector<bool> visited(nodeCount, false);
            std::vector<size_t> path;
            size_t current = start;

            while (not visited[current])
            {
                visited[current] = true;
                path.push_back(current);

                bool found = false;
                for (const size_t next : adjacency[current])
                {
                    if (inDegree[next] > 0)
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

            // current is the node we revisited - extract the cycle from the path
            auto getName = [&](size_t idx) -> std::string
            {
                if (nameOf)
                {
                    return nameOf(idx);
                }
                return std::format("node[{}]", idx);
            };

            std::string cyclePath;
            bool inCycle = false;
            for (const size_t node : path)
            {
                if (node == current)
                {
                    inCycle = true;
                }
                if (inCycle)
                {
                    if (not cyclePath.empty())
                    {
                        cyclePath += " -> ";
                    }
                    cyclePath += getName(node);
                }
            }
            // Close the cycle
            if (not cyclePath.empty())
            {
                cyclePath += " -> ";
                cyclePath += getName(current);
            }

            result.CyclePath = std::move(cyclePath);
        }

        return result;
    }

} // namespace Wayfinder
