#include "GameplayTagRegistry.h"
#include "core/Log.h"

#include <algorithm>
#include <string>
#include <toml++/toml.hpp>

namespace Wayfinder
{
    GameplayTag GameplayTagRegistry::RegisterTag(const std::string_view name, const std::string_view comment)
    {
        const std::string nameStr(name);
        if (auto it = m_index.find(nameStr); it != m_index.end())
        {
            // Update existing definition (code overrides data comment if non-empty).
            if (!comment.empty())
            {
                m_definitions.at(it->second).Comment = std::string(comment);
            }
            // Mark as code-owned so UnloadTagFile() won't remove it.
            m_definitions.at(it->second).SourceFile = "(code)";
            return GameplayTag::FromName(name);
        }

        EnsureAncestors(name, GameplayTagSourceKind::Code);

        const size_t idx = m_definitions.size();
        m_definitions.push_back({.Name = nameStr, .Comment = std::string(comment), .SourceFile = "(code)"});
        m_index[nameStr] = idx;

        WAYFINDER_INFO(LogEngine, "GameplayTagRegistry: registered tag '{}'{}", name, comment.empty() ? "" : " — " + std::string(comment));

        return GameplayTag::FromName(name);
    }

    int GameplayTagRegistry::LoadTagFile(const std::filesystem::path& path)
    {
        const std::string canonical = std::filesystem::weakly_canonical(path).string();

        try
        {
            const toml::table data = toml::parse_file(canonical);
            const toml::array* tags = data.get_as<toml::array>("tags");
            if (!tags)
            {
                WAYFINDER_WARN(LogEngine, "Tag file '{}' contains no [[tags]] array", canonical);
                return 0;
            }

            int count = 0;
            for (const toml::node& node : *tags)
            {
                const toml::table* entry = node.as_table();
                if (!entry)
                {
                    continue;
                }

                const auto* nameNode = entry->get("name");
                const auto name = nameNode != nullptr ? nameNode->value<std::string>() : std::optional<std::string>{};
                if (!name || name->empty())
                {
                    WAYFINDER_WARN(LogEngine, "Tag file '{}': skipping entry without 'name'", canonical);
                    continue;
                }

                const auto* commentNode = entry->get("comment");
                const std::string comment = commentNode != nullptr ? commentNode->value_or(std::string{}) : std::string{};

                EnsureAncestors(*name, GameplayTagSourceKind::File, canonical);

                if (auto it = m_index.find(*name); it != m_index.end())
                {
                    auto& def = m_definitions.at(it->second);
                    if (!comment.empty())
                    {
                        def.Comment = comment;
                    }
                    // Only overwrite SourceFile for definitions that were not
                    // registered from code, so UnloadTagFile won't erase them.
                    if (def.SourceFile != "(code)")
                    {
                        def.SourceFile = canonical;
                    }
                }
                else
                {
                    const size_t idx = m_definitions.size();
                    m_definitions.push_back({.Name = *name, .Comment = comment, .SourceFile = canonical});
                    m_index[*name] = idx;
                }

                ++count;
            }

            m_loadedFiles.push_back(canonical);
            WAYFINDER_INFO(LogEngine, "GameplayTagRegistry: loaded {} tag(s) from '{}'", count, canonical);
            return count;
        }
        catch (const toml::parse_error& err)
        {
            WAYFINDER_ERROR(LogEngine, "Failed to parse tag file '{}': {}", canonical, err.description());
            return -1;
        }
    }

    void GameplayTagRegistry::UnloadTagFile(const std::filesystem::path& path)
    {
        const std::string canonical = std::filesystem::weakly_canonical(path).string();

        // Remove definitions sourced from this file
        std::erase_if(m_definitions, [&](const GameplayTagDefinition& def)
        {
            return def.SourceFile == canonical;
        });

        // Rebuild index
        m_index.clear();
        for (size_t i = 0; i < m_definitions.size(); ++i)
        {
            const auto& definition = m_definitions.at(i);
            m_index[definition.Name] = i;
        }

        // Remove from loaded file list
        std::erase(m_loadedFiles, canonical);

        WAYFINDER_INFO(LogEngine, "GameplayTagRegistry: unloaded tag file '{}'", canonical);
    }

    GameplayTag GameplayTagRegistry::RequestTag(const std::string_view name) const
    {
        if (!IsRegistered(name))
        {
            WAYFINDER_WARN(LogEngine,
                "GameplayTagRegistry: requested unregistered tag '{}'. "
                "Register it in a tag file or via PluginRegistry::RegisterTag().",
                name);
        }

        return GameplayTag::FromName(name);
    }

    bool GameplayTagRegistry::IsRegistered(const std::string_view name) const
    {
        return m_index.contains(std::string(name));
    }

    const GameplayTagDefinition* GameplayTagRegistry::FindDefinition(const std::string_view name) const
    {
        if (const auto it = m_index.find(std::string(name)); it != m_index.end())
        {
            return &m_definitions.at(it->second);
        }
        return nullptr;
    }

    void GameplayTagRegistry::EnsureAncestors(std::string_view name, GameplayTagSourceKind sourceKind, std::string_view sourceFile)
    {
        const std::string_view resolvedSourceFile = sourceKind == GameplayTagSourceKind::Code ? std::string_view{"(code)"} : sourceFile;
        std::string::size_type pos = 0;
        while ((pos = name.find('.', pos)) != std::string::npos)
        {
            const std::string ancestor = std::string{name.substr(0, pos)};
            if (!m_index.contains(ancestor))
            {
                const size_t idx = m_definitions.size();
                m_definitions.push_back({.Name = ancestor, .Comment = {}, .SourceFile = std::string{resolvedSourceFile}});
                m_index[ancestor] = idx;
            }
            ++pos;
        }
    }

} // namespace Wayfinder
