#pragma once

#include "wayfinder_exports.h"

#include <compare>
#include <format>
#include <functional>
#include <string>
#include <string_view>

namespace Wayfinder
{
    /**
     * @class InternedString
     * @brief A lightweight, interned string value type with O(1) equality comparison.
     *
     * All InternedString instances point into a global string table. Two
     * InternedStrings that hold the same text compare equal by raw pointer
     * comparison, making equality checks and hashing trivially cheap.
     *
     * Typical usage:
     * @code
     *   auto a = InternedString::Intern("Status.Burning");
     *   auto b = InternedString::Intern("Status.Burning");
     *   assert(a == b);  // pointer comparison — O(1)
     * @endcode
     *
     * @note NOT thread-safe. The engine is single-threaded by design.
     *
     * @note Do not intern unbounded unique strings (e.g. per-frame timestamps).
     *       The table is append-only for the process lifetime; misuse only grows memory.
     */
    class WAYFINDER_API InternedString
    {
    public:
        /// Default-constructed InternedString points to the interned empty string.
        [[nodiscard]] InternedString();

        /// Intern a string. If the string is already in the table, returns
        /// a handle to the existing entry. Otherwise inserts it.
        [[nodiscard]] static InternedString Intern(std::string_view text);

        /// O(1) identity equality (same interned storage).
        [[nodiscard]] bool operator==(const InternedString& other) const noexcept
        {
            return m_ptr == other.m_ptr;
        }

        /// Compare with `std::string_view` without interning the rhs.
        [[nodiscard]] bool operator==(std::string_view sv) const noexcept
        {
            return AsStringView() == sv;
        }

        /// Lexicographic ordering (same as `GetString()`); same-text interned pairs compare equal.
        [[nodiscard]] std::strong_ordering operator<=>(const InternedString& other) const noexcept
        {
            return *m_ptr <=> *other.m_ptr;
        }

        /// Compare with `std::string_view` without interning the rhs.
        [[nodiscard]] std::strong_ordering operator<=>(std::string_view sv) const noexcept
        {
            return AsStringView() <=> sv;
        }

        /// Access the underlying string.
        [[nodiscard]] const std::string& GetString() const noexcept
        {
            return *m_ptr;
        }

        /// Borrowed view of the interned text (valid for the process lifetime).
        [[nodiscard]] std::string_view AsStringView() const noexcept
        {
            return *m_ptr;
        }

        /// Stable address of the unique `std::string` in the intern table (for hashing).
        [[nodiscard]] const std::string* GetStoragePointer() const noexcept
        {
            return m_ptr;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_ptr->empty();
        }

    private:
        explicit InternedString(const std::string* ptr) noexcept : m_ptr(ptr) {}

        const std::string* m_ptr;
    };

} // namespace Wayfinder

/// std::hash specialisation for use in unordered containers.
/// Relies on pointer stability from interning: all InternedString instances
/// pointing to the same logical string share the same address, so hashing
/// the storage pointer is both valid and trivially cheap.
template<>
struct std::hash<Wayfinder::InternedString>
{
    size_t operator()(const Wayfinder::InternedString& s) const noexcept
    {
        return std::hash<const std::string*>{}(s.GetStoragePointer());
    }
};

/// std::formatter specialisation so InternedString works with std::format / std::print.
template<>
struct std::formatter<Wayfinder::InternedString> : std::formatter<std::string_view>
{
    auto format(const Wayfinder::InternedString& s, auto& ctx) const
    {
        return std::formatter<std::string_view>::format(s.AsStringView(), ctx);
    }
};
