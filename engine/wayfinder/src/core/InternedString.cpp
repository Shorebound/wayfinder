#include "InternedString.h"

#include <unordered_set>

namespace Wayfinder
{
    namespace
    {
        /// Transparent hash enables heterogeneous `find` / `contains` with `std::string_view`
        /// without constructing a temporary `std::string` on lookup hits.
        struct InternTableHash
        {
            // NOLINTNEXTLINE(readability-identifier-naming) — required name for transparent `unordered_set` lookup
            using is_transparent = void;

            std::size_t operator()(std::string_view sv) const noexcept
            {
                return std::hash<std::string_view>{}(sv);
            }
        };

        void ReserveInternTableOnce(std::unordered_set<std::string, InternTableHash, std::equal_to<>>& table)
        {
            static bool done = false;
            if (!done)
            {
                done = true;
                table.reserve(2048);
            }
        }

        std::unordered_set<std::string, InternTableHash, std::equal_to<>>& GetTable()
        {
            static std::unordered_set<std::string, InternTableHash, std::equal_to<>> sTable;
            ReserveInternTableOnce(sTable);
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
