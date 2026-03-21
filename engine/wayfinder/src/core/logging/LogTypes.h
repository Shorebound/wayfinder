#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

namespace Wayfinder
{
    // Log verbosity levels
    enum class LogVerbosity : uint8_t
    {
        Fatal,      // The program cannot continue. Logs and crashes.
        Error,      // The feature is disabled. Logs and continues.
        Warning,    // Something is wrong but the feature can still work.
        Info,       // Standard log messages
        Verbose,    // Detailed information for debugging
        VeryVerbose // Very detailed information for debugging specific issues
    };

    // Log output types (formerly "sinks")
    enum class LogOutputType : uint8_t
    {
        None = 0,
        Console = 1 << 0, // 0b0001
        File = 1 << 1,    // 0b0010

        Count = 2 // Keep track of how many flags we have, excluding None
    };

    // Add operator overloads for enum class bitwise operations
    inline LogOutputType operator|(LogOutputType a, LogOutputType b)
    {
        return static_cast<LogOutputType>(static_cast<std::underlying_type_t<LogOutputType>>(a) | static_cast<std::underlying_type_t<LogOutputType>>(b));
    }

    inline LogOutputType operator&(LogOutputType a, LogOutputType b)
    {
        return static_cast<LogOutputType>(static_cast<std::underlying_type_t<LogOutputType>>(a) & static_cast<std::underlying_type_t<LogOutputType>>(b));
    }

    inline LogOutputType& operator|=(LogOutputType& a, LogOutputType b)
    {
        a = a | b;
        return a;
    }

    inline LogOutputType& operator&=(LogOutputType& a, LogOutputType b)
    {
        a = a & b;
        return a;
    }

    // Configuration for file output
    struct LogFileConfig
    {
        std::string FilePath = "logs/wayfinder.log"; // Default log file path
        size_t MaxFileSize = 5 * 1024 * 1024;        // 5 MB default
        size_t MaxFiles = 3;                         // Keep 3 rotated files by default
    };

    // Global logger configuration
    struct LogConfig
    {
        // Enabled outputs using the enum directly
        LogOutputType EnabledOutputs = LogOutputType::Console;

        // File output specific configuration
        LogFileConfig FileOutputConfig;

        // Helper methods to enable/disable specific outputs
        void EnableOutput(LogOutputType output, const bool enable = true)
        {
            if (enable)
            {
                EnabledOutputs = EnabledOutputs | output;
            }
            else
            {
                // Clear the specific bit using AND with inverted bits
                EnabledOutputs = EnabledOutputs & static_cast<LogOutputType>(~static_cast<std::underlying_type_t<LogOutputType>>(output));
            }
        }

        bool IsOutputEnabled(const LogOutputType output) const
        {
            return static_cast<bool>(EnabledOutputs & output);
        }
    };
} // namespace Wayfinder
