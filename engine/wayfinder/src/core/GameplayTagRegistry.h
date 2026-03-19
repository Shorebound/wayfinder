#pragma once

#include "GameplayTag.h"
#include "wayfinder_exports.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    /**
     * @struct GameplayTagDefinition
     * @brief Metadata for a registered gameplay tag.
     */
    struct GameplayTagDefinition
    {
        std::string Name;
        std::string Comment;
        std::string SourceFile; ///< File that defined this tag, or "(code)" for code-registered tags.
    };

    /**
     * @class GameplayTagRegistry
     * @brief Central registry for gameplay tag definitions, analogous to Unreal's FGameplayTagsManager.
     *
     * Tags must be registered here before use. Registration can happen:
     * - In code via ModuleRegistry::RegisterTag(name, comment)
     * - In data via TOML tag files loaded from config/tags/
     *
     * The registry validates tag requests and issues warnings for unregistered tags.
     * Tag files can be loaded and unloaded dynamically (e.g. for DLC or game modes).
     *
     * TOML tag file format:
     * @code
     * [[tags]]
     * name = "Status"
     * comment = "Root tag for all entity status effects"
     *
     * [[tags]]
     * name = "Status.Burning"
     * comment = "Entity is on fire, taking damage over time"
     * @endcode
     */
    class WAYFINDER_API GameplayTagRegistry
    {
    public:
        /// Register a tag programmatically (from code). Replaces existing definition if present.
        void RegisterTag(const std::string& name, const std::string& comment = {});

        /// Load tag definitions from a TOML file. Returns number of tags loaded, or -1 on error.
        int LoadTagFile(const std::filesystem::path& path);

        /// Unload all tag definitions that came from the given file.
        void UnloadTagFile(const std::filesystem::path& path);

        /// Request a validated tag by name. Logs a warning if the tag is not registered.
        GameplayTag RequestTag(const std::string& name) const;

        /// Check if a tag name is registered.
        bool IsRegistered(const std::string& name) const;

        /// Look up the full definition for a tag. Returns nullptr if not registered.
        const GameplayTagDefinition* FindDefinition(const std::string& name) const;

        /// All registered tag definitions.
        const std::vector<GameplayTagDefinition>& GetAllDefinitions() const { return m_definitions; }

        /// All currently loaded tag file paths.
        const std::vector<std::string>& GetLoadedFiles() const { return m_loadedFiles; }

    private:
        /// Ensures all ancestor tags exist (e.g. registering "A.B.C" also registers "A" and "A.B").
        void EnsureAncestors(const std::string& name, const std::string& sourceFile);

        std::vector<GameplayTagDefinition> m_definitions;
        std::unordered_map<std::string, size_t> m_index; ///< Name -> index into m_definitions.
        std::vector<std::string> m_loadedFiles;
    };

} // namespace Wayfinder
