#include "SlangCompiler.h"
#include "core/Log.h"

#include <slang-com-ptr.h>
#include <slang.h>

#include <filesystem>
#include <format>

namespace Wayfinder
{

    struct SlangCompiler::Impl
    {
        Slang::ComPtr<slang::IGlobalSession> GlobalSession;
        Slang::ComPtr<slang::ISession> Session;
    };

    SlangCompiler::SlangCompiler() = default;
    SlangCompiler::~SlangCompiler()
    {
        Shutdown();
    }

    SlangCompiler::SlangCompiler(SlangCompiler&&) noexcept = default;
    SlangCompiler& SlangCompiler::operator=(SlangCompiler&&) noexcept = default;

    Result<void> SlangCompiler::Initialise(const InitDesc& desc)
    {
        if (m_impl)
        {
            return MakeError("SlangCompiler already initialised");
        }

        m_sourceDirectory = std::string(desc.sourceDirectory);

        if (!std::filesystem::exists(m_sourceDirectory))
        {
            return MakeError(std::format("Slang source directory does not exist: {}", m_sourceDirectory));
        }

        auto impl = std::make_unique<Impl>();

        // Create the global Slang session
        SlangResult result = slang::createGlobalSession(impl->GlobalSession.writeRef());
        if (SLANG_FAILED(result))
        {
            return MakeError("Failed to create Slang global session");
        }

        // Configure the compilation target for SPIR-V
        slang::TargetDesc targetDesc = {};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = impl->GlobalSession->findProfile("spirv_1_5");

        // Match the offline slangc flags: -emit-spirv-directly -fvk-use-entrypoint-name
        slang::CompilerOptionEntry options[] =
        {
            {slang::CompilerOptionName::EmitSpirvDirectly, {.intValue0 = 1}},
            {slang::CompilerOptionName::VulkanUseEntryPointName, {.intValue0 = 1}},
        };

        slang::SessionDesc sessionDesc = {};
        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;
        sessionDesc.compilerOptionEntries = options;
        sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(std::size(options));

        // Build search paths: source directory first, then any additional paths
        std::vector<const char*> searchPathPtrs;
        searchPathPtrs.reserve(1 + desc.searchPaths.size());
        searchPathPtrs.push_back(m_sourceDirectory.c_str());
        // Pin additional path strings so pointers remain valid during session creation
        std::vector<std::string> searchPathStorage(desc.searchPaths.begin(), desc.searchPaths.end());
        for (const auto& path : searchPathStorage)
        {
            searchPathPtrs.push_back(path.c_str());
        }
        sessionDesc.searchPaths = searchPathPtrs.data();
        sessionDesc.searchPathCount = static_cast<SlangInt>(searchPathPtrs.size());

        result = impl->GlobalSession->createSession(sessionDesc, impl->Session.writeRef());
        if (SLANG_FAILED(result))
        {
            return MakeError("Failed to create Slang compilation session");
        }

        m_impl = std::move(impl);
        WAYFINDER_INFO(LogRenderer, "SlangCompiler: initialised with source directory '{}'", m_sourceDirectory);
        return {};
    }

    void SlangCompiler::Shutdown()
    {
        if (m_impl)
        {
            m_impl.reset();
            WAYFINDER_INFO(LogRenderer, "SlangCompiler: shut down");
        }
        m_sourceDirectory.clear();
    }

    bool SlangCompiler::IsInitialised() const
    {
        return m_impl != nullptr;
    }

    namespace
    {
        void LogSlangDiagnostics(slang::IBlob* diagnostics)
        {
            if (!diagnostics || diagnostics->getBufferSize() == 0)
            {
                return;
            }

            const std::string_view text(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize());

            // Split by newlines and log each non-empty line
            size_t pos = 0;
            while (pos < text.size())
            {
                const size_t end = text.find('\n', pos);
                const size_t lineEnd = (end == std::string_view::npos) ? text.size() : end;
                std::string_view line = text.substr(pos, lineEnd - pos);

                // Trim trailing \r
                if (!line.empty() && line.back() == '\r')
                {
                    line.remove_suffix(1);
                }

                if (!line.empty())
                {
                    if (line.find("error") != std::string_view::npos)
                    {
                        WAYFINDER_ERROR(LogRenderer, "  slang: {}", line);
                    }
                    else if (line.find("warning") != std::string_view::npos)
                    {
                        WAYFINDER_WARN(LogRenderer, "  slang: {}", line);
                    }
                    else
                    {
                        WAYFINDER_INFO(LogRenderer, "  slang: {}", line);
                    }
                }

                pos = (end == std::string_view::npos) ? text.size() : end + 1;
            }
        }
    } // namespace

    Result<SlangCompiler::CompileResult> SlangCompiler::Compile(const std::string_view sourceName, const std::string_view entryPoint, ShaderStage stage) const
    {
        if (!m_impl)
        {
            return MakeError("SlangCompiler not initialised");
        }

        // Build the full source path: sourceDirectory / sourceName.slang
        const std::string sourcePath = (std::filesystem::path(m_sourceDirectory) / std::format("{}.slang", sourceName)).string();

        const char* stageLabel = [stage]
        {
            switch (stage)
            {
            case ShaderStage::Vertex:
                return "vertex";
            case ShaderStage::Fragment:
                return "fragment";
            case ShaderStage::Compute:
                return "compute";
            default:
                return "unknown";
            }
        }();

        WAYFINDER_INFO(LogRenderer, "SlangCompiler: compiling '{}' entry '{}' ({})", sourceName, entryPoint, stageLabel);

        // 1. Load the module
        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* module = m_impl->Session->loadModule(sourcePath.c_str(), diagnostics.writeRef());
        if (!module)
        {
            WAYFINDER_ERROR(LogRenderer, "SlangCompiler: failed to load module '{}'", sourcePath);
            LogSlangDiagnostics(diagnostics);
            return MakeError(std::format("Failed to load Slang module '{}'", sourcePath));
        }
        LogSlangDiagnostics(diagnostics);

        // 2. Find the entry point
        Slang::ComPtr<slang::IEntryPoint> entryPointObj;
        const std::string entryPointStr(entryPoint);
        SlangResult result = module->findEntryPointByName(entryPointStr.c_str(), entryPointObj.writeRef());
        if (SLANG_FAILED(result) || !entryPointObj)
        {
            WAYFINDER_ERROR(LogRenderer, "SlangCompiler: entry point '{}' not found in '{}'", entryPoint, sourcePath);
            return MakeError(std::format("Entry point '{}' not found in '{}'", entryPoint, sourcePath));
        }

        // 3. Compose the program (module + entry point)
        slang::IComponentType* components[] = {module, entryPointObj.get()};
        Slang::ComPtr<slang::IComponentType> composedProgram;
        diagnostics = nullptr;
        result = m_impl->Session->createCompositeComponentType(components, static_cast<SlangInt>(std::size(components)), composedProgram.writeRef(), diagnostics.writeRef());
        if (SLANG_FAILED(result))
        {
            WAYFINDER_ERROR(LogRenderer, "SlangCompiler: failed to compose program for '{}':'{}'", sourcePath, entryPoint);
            LogSlangDiagnostics(diagnostics);
            return MakeError(std::format("Failed to compose Slang program for '{}':'{}'", sourcePath, entryPoint));
        }
        LogSlangDiagnostics(diagnostics);

        // 4. Link the composed program (resolves cross-module references)
        Slang::ComPtr<slang::IComponentType> linkedProgram;
        diagnostics = nullptr;
        result = composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef());
        if (SLANG_FAILED(result))
        {
            WAYFINDER_ERROR(LogRenderer, "SlangCompiler: linking failed for '{}':'{}'", sourcePath, entryPoint);
            LogSlangDiagnostics(diagnostics);
            return MakeError(std::format("Linking failed for '{}':'{}'", sourcePath, entryPoint));
        }
        LogSlangDiagnostics(diagnostics);

        // 5. Emit SPIR-V bytecode
        Slang::ComPtr<slang::IBlob> spirvBlob;
        diagnostics = nullptr;
        result = linkedProgram->getEntryPointCode(0, 0, spirvBlob.writeRef(), diagnostics.writeRef());
        if (SLANG_FAILED(result) || !spirvBlob || spirvBlob->getBufferSize() == 0)
        {
            WAYFINDER_ERROR(LogRenderer, "SlangCompiler: SPIR-V code generation failed for '{}':'{}'", sourcePath, entryPoint);
            LogSlangDiagnostics(diagnostics);
            return MakeError(std::format("SPIR-V code generation failed for '{}':'{}'", sourcePath, entryPoint));
        }
        LogSlangDiagnostics(diagnostics);

        // 6. Copy to output
        const auto* data = static_cast<const uint8_t*>(spirvBlob->getBufferPointer());
        const size_t size = spirvBlob->getBufferSize();

        CompileResult compileResult;
        compileResult.Bytecode.assign(data, data + size);

        WAYFINDER_INFO(LogRenderer, "SlangCompiler: compiled '{}' entry '{}' ({}) - {} bytes SPIR-V", sourceName, entryPoint, stageLabel, size);

        return compileResult;
    }

} // namespace Wayfinder
