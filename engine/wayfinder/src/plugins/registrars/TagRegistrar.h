#pragma once

#include "plugins/IRegistrar.h"
#include "wayfinder_exports.h"

#include <string>
#include <vector>

namespace Wayfinder::Plugins
{
    /// Internal storage for gameplay-tag descriptors and tag-file paths.
    /// Owned by PluginRegistry — not a subsystem.
    class WAYFINDER_API TagRegistrar : public ::Wayfinder::IRegistrar
    {
    public:
        struct Descriptor
        {
            std::string Name;
            std::string Comment;
        };

        /// Register a code-defined tag descriptor.
        void Register(Descriptor descriptor);

        /// Register a tag-file path (relative to config dir).
        void AddFile(std::string relativePath);

        /// Read-only access to descriptors.
        const std::vector<Descriptor>& GetDescriptors() const
        {
            return m_descriptors;
        }

        /// Read-only access to registered tag-file paths.
        const std::vector<std::string>& GetFiles() const
        {
            return m_files;
        }

    private:
        std::vector<Descriptor> m_descriptors;
        std::vector<std::string> m_files;
    };

} // namespace Wayfinder::Plugins
