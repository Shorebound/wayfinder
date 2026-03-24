#include "TagRegistrar.h"
#include "core/Log.h"

namespace Wayfinder::Plugins
{
    void TagRegistrar::Register(Descriptor descriptor)
    {
        for (const auto& existing : m_descriptors)
        {
            if (existing.Name == descriptor.Name)
            {
                WAYFINDER_ERROR(LogEngine, "TagRegistrar: duplicate tag name '{}' — registration rejected", descriptor.Name);
                return;
            }
        }

        WAYFINDER_INFO(LogEngine, "TagRegistrar: registered tag '{}'", descriptor.Name);
        m_descriptors.push_back(std::move(descriptor));
    }

    void TagRegistrar::AddFile(std::string relativePath)
    {
        for (const auto& existing : m_files)
        {
            if (existing == relativePath)
            {
                WAYFINDER_INFO(LogEngine, "TagRegistrar: tag file '{}' already registered; skipping", relativePath);
                return;
            }
        }

        WAYFINDER_INFO(LogEngine, "TagRegistrar: registered tag file '{}'", relativePath);
        m_files.push_back(std::move(relativePath));
    }

} // namespace Wayfinder::Plugins
