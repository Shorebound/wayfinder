#pragma once

#include "wayfinder_exports.h"

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
     */
    class WAYFINDER_API InternedString
    {
    public:
        /// Default-constructed InternedString points to the interned empty string.
        InternedString();

        /// Intern a string. If the string is already in the table, returns
        /// a handle to the existing entry. Otherwise inserts it.
        static InternedString Intern(std::string_view text);

        /// O(1) pointer equality.
        bool operator==(const InternedString& other) const { return m_ptr == other.m_ptr; }
        bool operator!=(const InternedString& other) const { return m_ptr != other.m_ptr; }

        /// Content-based ordering (for sorted containers).
        bool operator<(const InternedString& other) const { return *m_ptr < *other.m_ptr; }

        /// Access the underlying string.
        const std::string& GetString() const { return *m_ptr; }

        /// Convenience implicit conversion for logging, formatting, etc.
        operator const std::string&() const { return *m_ptr; }

        bool IsEmpty() const { return m_ptr->empty(); }

    private:
        explicit InternedString(const std::string* ptr) : m_ptr(ptr) {}

        const std::string* m_ptr;
    };

} // namespace Wayfinder

/// std::hash specialisation for use in unordered containers.
template <>
struct std::hash<Wayfinder::InternedString>
{
    size_t operator()(const Wayfinder::InternedString& s) const noexcept
    {
        return std::hash<const void*>{}(&s.GetString());
    }
};
