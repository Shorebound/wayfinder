#pragma once

#include "Tag.h"
#include "app/Subsystem.h"
#include "core/TransparentStringHash.h"
#include "wayfinder_exports.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Wayfinder
{
    enum class TagSourceKind
    {
        Code,
        File,
    };

    /**
     * @struct TagDefinition
     * @brief Metadata for a registered tag.
     */
    struct TagDefinition
    {
        std::string Name;
        std::string Comment;
        std::string SourceFile; ///< File that defined this tag, or "(code)" for code-registered tags.
    };

    /**
     * @class TagRegistry
     * @brief Central registry for tag definitions, analogous to Unreal's FGameplayTagsManager.
     *
     * Tags must be registered here before use. Registration can happen:
     * - In code via PluginRegistry::RegisterTag(name, comment)
     * - In data via TOML tag files loaded from config/tags/
     *
     * The registry validates tag requests and issues warnings for unregistered tags.
     * Tag files can be loaded and unloaded dynamically (e.g. for DLC or game modes).
     *
     * This is a GameSubsystem — its lifetime is managed by the Game’s
     * SubsystemCollection. Access the live instance from anywhere via
     * GameSubsystems::Get<TagRegistry>().
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
    class WAYFINDER_API TagRegistry : public GameSubsystem
    {
    public:
        /// Register a tag programmatically (from code). Returns the tag.
        /// Updates existing definition's comment if the tag was already registered.
        Tag RegisterTag(std::string_view name, std::string_view comment = {});

        /// Load tag definitions from a TOML file. Returns number of tags loaded, or -1 on error.
        int LoadTagFile(const std::filesystem::path& path);

        /// Unload all tag definitions that came from the given file.
        void UnloadTagFile(const std::filesystem::path& path);

        /// Request a validated tag by name. Logs a warning if the tag is not registered.
        Tag RequestTag(std::string_view name) const;

        /// Check if a tag name is registered.
        bool IsRegistered(std::string_view name) const;

        /// Look up the full definition for a tag. Returns nullptr if not registered.
        const TagDefinition* FindDefinition(std::string_view name) const;

        /// All registered tag definitions.
        const std::vector<TagDefinition>& GetAllDefinitions() const
        {
            return m_definitions;
        }

        /// All currently loaded tag file paths.
        const std::vector<std::string>& GetLoadedFiles() const
        {
            return m_loadedFiles;
        }

    private:
        /// Ensures all ancestor tags exist (e.g. registering "A.B.C" also registers "A" and "A.B").
        void EnsureAncestors(std::string_view name, TagSourceKind sourceKind, std::string_view sourceFile = {});

        std::vector<TagDefinition> m_definitions;
        std::unordered_map<std::string, size_t, TransparentStringHash, std::equal_to<>> m_index; ///< Name -> index into m_definitions.
        std::vector<std::string> m_loadedFiles;
    };

} // namespace Wayfinder
