target_sources(${ENGINE_NAME}
    PRIVATE
        # Generic logging interfaces
        LogTypes.h
        ILogOutput.h
        ILogger.h
        
        # spdlog implementation
        spdlog/SpdLogOutput.h
        spdlog/SpdLogOutput.cpp
        spdlog/SpdLogger.h
        spdlog/SpdLogManager.h
)
