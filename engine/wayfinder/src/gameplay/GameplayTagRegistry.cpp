#include "GameplayTagRegistry.h"
#include "core/Log.h"

#include <algorithm>
#include <toml++/toml.hpp>

namespace Wayfinder
{
    GameplayTag GameplayTagRegistry::RegisterTag(const std::string& name, const std::string& comment)
    {
        if (auto it = m_index.find(name); it != m_index.end())
        {
            // Update existing definition (code overrides data comment if non-empty).
            if (!comment.empty()) m_definitions[it->second].Comment = comment;
            // Mark as code-owned so UnloadTagFile() won't remove it.
            m_definitions[it->second].SourceFile = "(code)";
            return GameplayTag::FromName(name);
        }

        EnsureAncestors(name, "(code)");

        const size_t idx = m_definitions.size();
        m_definitions.push_back({name, comment, "(code)"});
        m_index[name] = idx;

        WAYFINDER_INFO(LogEngine, "GameplayTagRegistry: registered tag '{}'{}", name, comment.empty() ? "" : " — " + comment);

        return GameplayTag::FromName(name);
    }

    int GameplayTagRegistry::LoadTagFile(const std::filesystem::path& path)
    {
        const std::string canonical = std::filesystem::weakly_canonical(path).string();

        try
        {
            const toml::table data = toml::parse_file(canonical);
            const toml::array* tags = data["tags"].as_array();
            if (!tags)
            {
                WAYFINDER_WARNING(LogEngine, "Tag file '{}' contains no [[tags]] array", canonical);
                return 0;
            }

            int count = 0;
            for (const toml::node& node : *tags)
            {
                const toml::table* entry = node.as_table();
                if (!entry) continue;

                const auto name = (*entry)["name"].value<std::string>();
                if (!name || name->empty())
                {
                    WAYFINDER_WARNING(LogEngine, "Tag file '{}': skipping entry without 'name'", canonical);
                    continue;
                }

                const auto comment = (*entry)["comment"].value_or(std::string{});

                EnsureAncestors(*name, canonical);

                if (auto it = m_index.find(*name); it != m_index.end())
                {
                    auto& def = m_definitions[it->second];
                    if (!comment.empty()) def.Comment = comment;
                    // Only overwrite SourceFile for definitions that were not
                    // registered from code, so UnloadTagFile won't erase them.
                    if (def.SourceFile != "(code)") def.SourceFile = canonical;
                }
                else
                {
                    const size_t idx = m_definitions.size();
                    m_definitions.push_back({*name, comment, canonical});
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
        std::erase_if(m_definitions, [&](const GameplayTagDefinition& def) { return def.SourceFile == canonical; });

        // Rebuild index
        m_index.clear();
        for (size_t i = 0; i < m_definitions.size(); ++i)
            m_index[m_definitions[i].Name] = i;

        // Remove from loaded file list
        std::erase(m_loadedFiles, canonical);

        WAYFINDER_INFO(LogEngine, "GameplayTagRegistry: unloaded tag file '{}'", canonical);
    }

    GameplayTag GameplayTagRegistry::RequestTag(const std::string& name) const
    {
        if (!IsRegistered(name))
        {
            WAYFINDER_WARNING(LogEngine,
                "GameplayTagRegistry: requested unregistered tag '{}'. "
                "Register it in a tag file or via ModuleRegistry::RegisterTag().",
                name);
        }

        return GameplayTag::FromName(name);
    }

    bool GameplayTagRegistry::IsRegistered(const std::string& name) const
    {
        return m_index.contains(name);
    }

    const GameplayTagDefinition* GameplayTagRegistry::FindDefinition(const std::string& name) const
    {
        if (const auto it = m_index.find(name); it != m_index.end()) return &m_definitions[it->second];
        return nullptr;
    }

    void GameplayTagRegistry::EnsureAncestors(const std::string& name, const std::string& sourceFile)
    {
        std::string::size_type pos = 0;
        while ((pos = name.find('.', pos)) != std::string::npos)
        {
            std::string ancestor = name.substr(0, pos);
            if (!m_index.contains(ancestor))
            {
                const size_t idx = m_definitions.size();
                m_definitions.push_back({ancestor, {}, sourceFile});
                m_index[ancestor] = idx;
            }
            ++pos;
        }
    }

} // namespace Wayfinder
