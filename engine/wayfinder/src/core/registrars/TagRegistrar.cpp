#include "TagRegistrar.h"
#include "../Log.h"

namespace Wayfinder
{
    void TagRegistrar::Register(Descriptor descriptor)
    {
        WAYFINDER_INFO(LogEngine, "TagRegistrar: registered tag '{}'", descriptor.Name);
        m_descriptors.push_back(std::move(descriptor));
    }

    void TagRegistrar::AddFile(std::string relativePath)
    {
        WAYFINDER_INFO(LogEngine, "TagRegistrar: registered tag file '{}'", relativePath);
        m_files.push_back(std::move(relativePath));
    }

} // namespace Wayfinder
