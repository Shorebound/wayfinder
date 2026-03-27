#include "InternedString.h"

#include "core/TransparentStringHash.h"

#include <unordered_set>

namespace Wayfinder
{
    namespace
    {
        std::unordered_set<std::string, TransparentStringHash, std::equal_to<>>& GetTable()
        {
            static std::unordered_set<std::string, TransparentStringHash, std::equal_to<>> sTable = []
            {
                std::unordered_set<std::string, TransparentStringHash, std::equal_to<>> table;
                table.reserve(2048);
                return table;
            }();
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
        if (const auto it = table.find(text); it != table.end())
        {
            return InternedString{&*it};
        }
        const auto [inserted, _] = table.emplace(text);
        return InternedString{&*inserted};
    }

} // namespace Wayfinder
