#include "InternedString.h"

#include <unordered_set>

namespace Wayfinder
{
    /// The global intern table. Entries are never removed — once interned,
    /// the pointer is stable for the lifetime of the process.
    static std::unordered_set<std::string>& GetTable()
    {
        static std::unordered_set<std::string> s_table;
        return s_table;
    }

    InternedString::InternedString()
    {
        static const std::string* s_empty = &*GetTable().emplace("").first;
        m_ptr = s_empty;
    }

    InternedString InternedString::Intern(std::string_view text)
    {
        auto& table = GetTable();
        auto [it, _] = table.emplace(text);
        return InternedString{&*it};
    }

} // namespace Wayfinder
