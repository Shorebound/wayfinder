#include "SlangCompiler.h"
#include "core/Log.h"

#include <slang-com-ptr.h>
#include <slang.h>

#include <array>
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

        m_sourceDirectory = std::string(desc.SourceDirectory);
        m_searchPaths.assign(desc.SearchPaths.begin(), desc.SearchPaths.end());

        std::error_code ec;
        if (!std::filesystem::exists(m_sourceDirectory, ec))
        {
            if (ec)
            {
                return MakeError(std::format("Cannot inspect Slang source directory '{}': {}", m_sourceDirectory, ec.message()));
            }
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
        std::array options =
        {
            slang::CompilerOptionEntry{.name = slang::CompilerOptionName::EmitSpirvDirectly, .value = {.intValue0 = 1}},
            slang::CompilerOptionEntry{.name = slang::CompilerOptionName::VulkanUseEntryPointName, .value = {.intValue0 = 1}},
        };

        slang::SessionDesc sessionDesc = {};
        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;
        sessionDesc.compilerOptionEntries = options.data();
        sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(options.size());

        // Build search paths: source directory first, then any additional paths
        std::vector<const char*> searchPathPtrs;
        searchPathPtrs.reserve(1 + desc.SearchPaths.size());
        searchPathPtrs.push_back(m_sourceDirectory.c_str());
        // Pin additional path strings so pointers remain valid during session creation
        const std::vector<std::string> searchPathStorage(desc.SearchPaths.begin(), desc.SearchPaths.end());
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
        Log::Info(LogRenderer, "SlangCompiler: initialised with source directory '{}'", m_sourceDirectory);
        return {};
    }

    void SlangCompiler::Shutdown()
    {
        if (m_impl)
        {
            m_impl.reset();
            Log::Info(LogRenderer, "SlangCompiler: shut down");
        }
        m_sourceDirectory.clear();
        m_searchPaths.clear();
    }

    bool SlangCompiler::IsInitialised() const
    {
        return m_impl != nullptr;
    }

    Result<void> SlangCompiler::ResetSession()
    {
        if (!m_impl)
        {
            return MakeError("SlangCompiler not initialised");
        }

        // Build a fresh session into a temporary so the original survives on failure.
        slang::TargetDesc targetDesc = {};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = m_impl->GlobalSession->findProfile("spirv_1_5");

        std::array options =
        {
            slang::CompilerOptionEntry{.name = slang::CompilerOptionName::EmitSpirvDirectly, .value = {.intValue0 = 1}},
            slang::CompilerOptionEntry{.name = slang::CompilerOptionName::VulkanUseEntryPointName, .value = {.intValue0 = 1}},
        };

        slang::SessionDesc sessionDesc = {};
        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;
        sessionDesc.compilerOptionEntries = options.data();
        sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(options.size());

        std::vector<const char*> searchPathPtrs;
        searchPathPtrs.reserve(1 + m_searchPaths.size());
        searchPathPtrs.push_back(m_sourceDirectory.c_str());
        for (const auto& path : m_searchPaths)
        {
            searchPathPtrs.push_back(path.c_str());
        }
        sessionDesc.searchPaths = searchPathPtrs.data();
        sessionDesc.searchPathCount = static_cast<SlangInt>(searchPathPtrs.size());

        Slang::ComPtr<slang::ISession> newSession;
        const SlangResult result = m_impl->GlobalSession->createSession(sessionDesc, newSession.writeRef());
        if (SLANG_FAILED(result))
        {
            return MakeError("Failed to recreate Slang compilation session");
        }

        m_impl->Session = std::move(newSession);

        Log::Info(LogRenderer, "SlangCompiler: session reset (module cache cleared)");
        return {};
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
                        Log::Error(LogRenderer, "  slang: {}", line);
                    }
                    else if (line.find("warning") != std::string_view::npos)
                    {
                        Log::Warn(LogRenderer, "  slang: {}", line);
                    }
                    else
                    {
                        Log::Info(LogRenderer, "  slang: {}", line);
                    }
                }

                pos = (end == std::string_view::npos) ? text.size() : end + 1;
            }
        }

        struct AccessPathNode
        {
            slang::VariableLayoutReflection* VarLayout = nullptr;
            const AccessPathNode* Outer = nullptr;
        };

        struct AccessPath
        {
            const AccessPathNode* Leaf = nullptr;
            const AccessPathNode* DeepestConstantBuffer = nullptr;
            const AccessPathNode* DeepestParameterBlock = nullptr;
        };

        class ExtendedAccessPath
        {
        public:
            ExtendedAccessPath(const AccessPath& outer, slang::VariableLayoutReflection* varLayout) : m_node{.VarLayout = varLayout, .Outer = outer.Leaf}, m_accessPath{outer}
            {
                m_accessPath.Leaf = &m_node;
            }

            [[nodiscard]] auto Get() const -> const AccessPath&
            {
                return m_accessPath;
            }

        private:
            AccessPathNode m_node;
            AccessPath m_accessPath;
        };

        struct CumulativeOffset
        {
            SlangUInt Space = 0;
            SlangUInt Binding = 0;
        };

        auto IsTrackedUsageCategory(const slang::ParameterCategory category) -> bool
        {
            switch (category)
            {
            case slang::ParameterCategory::Uniform:
            case slang::ParameterCategory::ConstantBuffer:
            case slang::ParameterCategory::ShaderResource:
            case slang::ParameterCategory::UnorderedAccess:
            case slang::ParameterCategory::SamplerState:
            case slang::ParameterCategory::DescriptorTableSlot:
                return true;
            default:
                return false;
            }
        }

        auto AccumulateLayoutValue(SlangUInt& total, const size_t value) -> bool
        {
            if (value == SLANG_UNKNOWN_SIZE or value == SLANG_UNBOUNDED_SIZE)
            {
                return false;
            }

            total += static_cast<SlangUInt>(value);
            return true;
        }

        auto TryCalculateCumulativeOffset(const slang::ParameterCategory layoutUnit, const AccessPath& accessPath, CumulativeOffset& outOffset) -> bool
        {
            outOffset = {};

            switch (layoutUnit)
            {
            case slang::ParameterCategory::Uniform:
                for (auto* node = accessPath.Leaf; node != accessPath.DeepestConstantBuffer; node = node->Outer)
                {
                    if (not node or not node->VarLayout or not AccumulateLayoutValue(outOffset.Binding, node->VarLayout->getOffset(layoutUnit)))
                    {
                        return false;
                    }
                }
                return true;

            case slang::ParameterCategory::ConstantBuffer:
            case slang::ParameterCategory::ShaderResource:
            case slang::ParameterCategory::UnorderedAccess:
            case slang::ParameterCategory::SamplerState:
            case slang::ParameterCategory::DescriptorTableSlot:
                for (auto* node = accessPath.Leaf; node != accessPath.DeepestParameterBlock; node = node->Outer)
                {
                    if (not node or not node->VarLayout)
                    {
                        return false;
                    }

                    if (not AccumulateLayoutValue(outOffset.Binding, node->VarLayout->getOffset(layoutUnit)) or not AccumulateLayoutValue(outOffset.Space, node->VarLayout->getBindingSpace(layoutUnit)))
                    {
                        return false;
                    }
                }

                for (auto* node = accessPath.DeepestParameterBlock; node != nullptr; node = node->Outer)
                {
                    if (not node->VarLayout)
                    {
                        return false;
                    }

                    if (not AccumulateLayoutValue(outOffset.Space, node->VarLayout->getOffset(slang::ParameterCategory::SubElementRegisterSpace)))
                    {
                        return false;
                    }
                }
                return true;

            default:
                for (auto* node = accessPath.Leaf; node != nullptr; node = node->Outer)
                {
                    if (not node->VarLayout or not AccumulateLayoutValue(outOffset.Binding, node->VarLayout->getOffset(layoutUnit)))
                    {
                        return false;
                    }
                }
                return true;
            }
        }

        auto IsVarUsed(slang::VariableLayoutReflection* varLayout, const AccessPath& accessPath, slang::IMetadata* metadata) -> bool
        {
            if (not varLayout or not metadata)
            {
                return false;
            }

            const unsigned categoryCount = varLayout->getCategoryCount();
            for (unsigned categoryIndex = 0; categoryIndex < categoryCount; ++categoryIndex)
            {
                const auto category = varLayout->getCategoryByIndex(categoryIndex);
                if (not IsTrackedUsageCategory(category))
                {
                    continue;
                }

                CumulativeOffset offset{};
                if (not TryCalculateCumulativeOffset(category, accessPath, offset))
                {
                    continue;
                }

                bool used = false;
                if (SLANG_SUCCEEDED(metadata->isParameterLocationUsed(static_cast<SlangParameterCategory>(category), offset.Space, offset.Binding, used)) and used)
                {
                    return true;
                }
            }

            return false;
        }

        void CountBindingType(const slang::BindingType type, ShaderResourceCounts& counts)
        {
            switch (type)
            {
            case slang::BindingType::ConstantBuffer:
            case slang::BindingType::ParameterBlock:
                counts.UniformBuffers += 1;
                break;
            case slang::BindingType::Sampler:
            case slang::BindingType::Texture:
            case slang::BindingType::CombinedTextureSampler:
                counts.Samplers += 1;
                break;
            case slang::BindingType::MutableTexture:
                counts.StorageTextures += 1;
                break;
            case slang::BindingType::RawBuffer:
            case slang::BindingType::MutableRawBuffer:
            case slang::BindingType::TypedBuffer:
            case slang::BindingType::MutableTypedBuffer:
                counts.StorageBuffers += 1;
                break;
            default:
                break;
            }
        }

        auto CountVarResources(slang::VariableLayoutReflection* varLayout, const AccessPath& accessPath, slang::IMetadata* metadata, ShaderResourceCounts& counts) -> bool
        {
            if (not varLayout)
            {
                return false;
            }

            auto* typeLayout = varLayout->getTypeLayout();
            if (not typeLayout)
            {
                return false;
            }

            const ExtendedAccessPath extendedPath(accessPath, varLayout);
            const AccessPath& varAccessPath = extendedPath.Get();

            switch (typeLayout->getKind())
            {
            case slang::TypeReflection::Kind::ConstantBuffer:
            case slang::TypeReflection::Kind::ParameterBlock:
            {
                const bool containerUsed = IsVarUsed(varLayout, varAccessPath, metadata);

                bool childUsed = false;
                auto* elementVarLayout = typeLayout->getElementVarLayout();
                if (elementVarLayout)
                {
                    AccessPath innerAccessPath = varAccessPath;
                    innerAccessPath.DeepestConstantBuffer = innerAccessPath.Leaf;

                    if (typeLayout->getSize(slang::ParameterCategory::SubElementRegisterSpace) != 0)
                    {
                        innerAccessPath.DeepestParameterBlock = innerAccessPath.Leaf;
                    }

                    childUsed = CountVarResources(elementVarLayout, innerAccessPath, metadata, counts);
                }

                const bool used = containerUsed or childUsed;
                if (used)
                {
                    counts.UniformBuffers += 1;
                }

                return used;
            }

            case slang::TypeReflection::Kind::Struct:
            {
                bool anyFieldUsed = false;
                const unsigned fieldCount = typeLayout->getFieldCount();
                for (unsigned fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
                {
                    anyFieldUsed = CountVarResources(typeLayout->getFieldByIndex(fieldIndex), varAccessPath, metadata, counts) or anyFieldUsed;
                }
                return anyFieldUsed;
            }

            default:
                break;
            }

            if (not IsVarUsed(varLayout, varAccessPath, metadata))
            {
                return false;
            }

            const SlangInt bindingRangeCount = typeLayout->getBindingRangeCount();
            for (SlangInt rangeIndex = 0; rangeIndex < bindingRangeCount; ++rangeIndex)
            {
                CountBindingType(typeLayout->getBindingRangeType(rangeIndex), counts);
            }

            return true;
        }

        /// Extract per-entry-point resource counts using Slang reflection + IMetadata.
        ///
        /// ProgramLayout reports all globals declared in the module. Entry-point
        /// metadata only answers usage for leaf parameters at their cumulative
        /// absolute binding location, so we recursively traverse variable layouts,
        /// accumulate offsets through parameter blocks/constant buffers, and count
        /// only the bindings the compiled entry point actually references.
        auto ExtractResourceCounts(slang::IComponentType* linkedProgram) -> ShaderResourceCounts
        {
            ShaderResourceCounts counts{};

            auto* layout = linkedProgram->getLayout(0);
            if (not layout)
            {
                return counts;
            }

            Slang::ComPtr<slang::IMetadata> metadata;
            if (SLANG_FAILED(linkedProgram->getEntryPointMetadata(0, 0, metadata.writeRef(), nullptr)) or not metadata)
            {
                return counts;
            }

            const unsigned paramCount = layout->getParameterCount();
            for (unsigned i = 0; i < paramCount; ++i)
            {
                CountVarResources(layout->getParameterByIndex(i), {}, metadata, counts);
            }

            return counts;
        }
    } // namespace

    Result<SlangCompiler::CompileResult> SlangCompiler::Compile(const std::string_view sourceName, const std::string_view entryPoint, ShaderStage stage)
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

        Log::Info(LogRenderer, "SlangCompiler: compiling '{}' entry '{}' ({})", sourceName, entryPoint, stageLabel);

        // 1. Load the module
        Slang::ComPtr<slang::IBlob> diagnostics;
        slang::IModule* module = m_impl->Session->loadModule(sourcePath.c_str(), diagnostics.writeRef());
        if (!module)
        {
            Log::Error(LogRenderer, "SlangCompiler: failed to load module '{}'", sourcePath);
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
            Log::Error(LogRenderer, "SlangCompiler: entry point '{}' not found in '{}'", entryPoint, sourcePath);
            return MakeError(std::format("Entry point '{}' not found in '{}'", entryPoint, sourcePath));
        }

        // 3. Compose the program (module + entry point)
        // NOLINTBEGIN(misc-const-correctness): Slang API expects non-const IComponentType*
        std::array<slang::IComponentType*, 2> components = {module, entryPointObj.get()};
        // NOLINTEND(misc-const-correctness)
        Slang::ComPtr<slang::IComponentType> composedProgram;
        diagnostics = nullptr;
        result = m_impl->Session->createCompositeComponentType(components.data(), static_cast<SlangInt>(components.size()), composedProgram.writeRef(), diagnostics.writeRef());
        if (SLANG_FAILED(result))
        {
            Log::Error(LogRenderer, "SlangCompiler: failed to compose program for '{}':'{}'", sourcePath, entryPoint);
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
            Log::Error(LogRenderer, "SlangCompiler: linking failed for '{}':'{}'", sourcePath, entryPoint);
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
            Log::Error(LogRenderer, "SlangCompiler: SPIR-V code generation failed for '{}':'{}'", sourcePath, entryPoint);
            LogSlangDiagnostics(diagnostics);
            return MakeError(std::format("SPIR-V code generation failed for '{}':'{}'", sourcePath, entryPoint));
        }
        LogSlangDiagnostics(diagnostics);

        // 6. Copy SPIR-V to output and extract per-entry-point resource counts
        //    via Slang reflection + IMetadata (filters dead-stripped bindings).
        const auto* data = static_cast<const uint8_t*>(spirvBlob->getBufferPointer());
        const size_t size = spirvBlob->getBufferSize();

        CompileResult compileResult;
        compileResult.Bytecode.assign(data, data + size);
        compileResult.Resources = ExtractResourceCounts(linkedProgram.get());

        Log::Info(LogRenderer, "SlangCompiler: compiled '{}' entry '{}' ({}) - {} bytes SPIR-V", sourceName, entryPoint, stageLabel, size);

        return compileResult;
    }

} // namespace Wayfinder
