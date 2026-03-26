#pragma once
#include "core/logging/ILogOutput.h"

#include <spdlog/spdlog.h>

namespace Wayfinder
{
    // Forward declaration
    class SpdLogMessage;

    // Base class for spdlog-based outputs
    class SpdLogOutput : public ILogOutput
    {
    public:
        SpdLogOutput(const std::shared_ptr<spdlog::sinks::sink>& sink) : m_sink(sink) {}

        virtual ~SpdLogOutput() override = default;

        // ILogOutput implementation
        void ProcessMessage(const ILogMessage& message) override;
        void Flush() override;
        void SetPattern(const std::string& pattern) override;

        // Get the underlying spdlog sink
        std::shared_ptr<spdlog::sinks::sink> GetSink() const
        {
            return m_sink;
        }

    protected:
        std::shared_ptr<spdlog::sinks::sink> m_sink;
    };

    // Console output implementation
    class SpdConsoleOutput : public SpdLogOutput
    {
    public:
        SpdConsoleOutput();
    };

    // File output implementation
    class SpdFileOutput : public SpdLogOutput
    {
    public:
        SpdFileOutput(const LogFileConfig& config);
    };

    // Factory function declarations - implementations are in SpdLogOutput.cpp

} // namespace Wayfinder
