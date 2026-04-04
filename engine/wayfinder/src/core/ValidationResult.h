#pragma once

#include "Result.h"

#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Wayfinder
{
    /**
     * @brief A single validation error with source attribution.
     */
    struct ValidationError
    {
        std::string Source;  ///< Registrar or component that produced the error.
        std::string Message; ///< Human-readable description.
    };

    /**
     * @brief Compiler-style error accumulator for build-time validation.
     *
     * Collects multiple errors from different registrars during AppBuilder::Finalise(),
     * enabling batch reporting rather than fail-fast on the first issue.
     */
    class ValidationResult
    {
    public:
        void AddError(std::string_view source, std::string message)
        {
            m_errors.push_back({std::string(source), std::move(message)});
        }

        [[nodiscard]] auto HasErrors() const -> bool
        {
            return not m_errors.empty();
        }

        [[nodiscard]] auto GetErrors() const -> std::span<const ValidationError>
        {
            return m_errors;
        }

        [[nodiscard]] auto ErrorCount() const -> size_t
        {
            return m_errors.size();
        }

        /// Convert accumulated errors into a single Error for Result<T> usage.
        [[nodiscard]] auto ToError() const -> Error
        {
            std::string combined;
            for (const auto& err : m_errors)
            {
                combined += std::format("[{}] {}\n", err.Source, err.Message);
            }
            return Error(combined);
        }

    private:
        std::vector<ValidationError> m_errors;
    };

} // namespace Wayfinder
