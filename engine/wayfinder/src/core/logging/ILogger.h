#pragma once
#include "ILogOutput.h"
#include "LogTypes.h"

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Wayfinder
{

    /**
     * @brief Abstract logger interface.
     *
     * Implementations handle output routing and backend-specific formatting.
     * Prefer the free functions in the Log namespace (Log::Info, Log::Warn, etc.)
     * over calling these methods directly.
     */
    class ILogger
    {
    public:
        virtual ~ILogger() = default;

        virtual const std::string& GetName() const = 0;
        virtual LogVerbosity GetVerbosity() const = 0;
        virtual void SetVerbosity(LogVerbosity level) = 0;

        virtual void AddOutput(std::shared_ptr<ILogOutput> output) = 0;
        virtual void ClearOutputs() = 0;
        virtual const std::vector<std::shared_ptr<ILogOutput>>& GetOutputs() const = 0;

        /// Log a pre-formatted message at the given verbosity.
        virtual void Log(LogVerbosity level, std::string_view message) = 0;

        /// Log with type-erased std::format args. Caller is responsible for the verbosity check.
        virtual void LogFormatted(LogVerbosity level, std::string_view format, std::format_args args) = 0;
    };

    /// Factory function for creating loggers (backend-specific, defined in SpdLogOutput.cpp).
    std::shared_ptr<ILogger> CreateLogger(const std::string& name, LogVerbosity defaultVerbosity = LogVerbosity::Info);

} // namespace Wayfinder
