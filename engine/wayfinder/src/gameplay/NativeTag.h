#pragma once

#include "Tag.h"
#include "TagRegistry.h"
#include <cassert>

namespace Wayfinder
{
    class WAYFINDER_API NativeTag
    {
    public:
        /// Construct and self-register into the global native tag list.
        NativeTag(const char* name, const char* comment = "") : m_name(name), m_comment(comment)
        {
            // Insert into linked list of native tags for later registration.
            m_next = s_head;
            s_head = this;
        }

        /// Resolve to the cached Tag (valid after RegisterAll).
        operator Tag() const
        {
            // precondition: RegisterAll has been called
            assert(m_cached.IsValid() && "NativeTag used before RegisterAll");
            return m_cached;
        }

        constexpr std::string_view GetName() const
        {
            return m_name;
        }
        constexpr std::string_view GetComment() const
        {
            return m_comment;
        }

        /// Called once during engine init - walks the list and registers every native tag into the given registry.
        static void RegisterAll(TagRegistry& registry)
        {
            for (auto* tag = s_head; tag; tag = tag->m_next)
            {
                tag->m_cached = registry.RegisterTag(tag->m_name, tag->m_comment);
            }
        }

    private:
        const char* m_name;
        const char* m_comment;
        Tag m_cached{}; // filled by RegisterAll
        NativeTag* m_next = nullptr;

        static inline NativeTag* s_head = nullptr;
    };

} // namespace Wayfinder