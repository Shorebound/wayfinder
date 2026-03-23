#include "InternedString.h"

#include <unordered_set>

namespace Wayfinder
{
    namespace
    {
        /// The global intern table. Entries are never removed — once interned,
        /// the pointer is stable for the lifetime of the process.
        std::unordered_set<std::string>& GetTable()
        {
            static std::unordered_set<std::string> sTable;
            return sTable;
        }

    } // namespace

    InternedString::InternedString()
    {
        static const std::string* sEmpty = &*GetTable().emplace("").first;
        m_ptr = sEmpty;
    }

    InternedString InternedString::Intern(std::string_view text)
    {
        auto& table = GetTable();
        auto [it, _] = table.emplace(text);
        return InternedString{&*it};
    }

} // namespace Wayfinder
