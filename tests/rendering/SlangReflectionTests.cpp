#include "TestHelpers.h"
#include "rendering/materials/ShaderManager.h"
#include "rendering/materials/SlangCompiler.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace Wayfinder::Tests
{
    namespace
    {
        std::string ShaderFixturesDirStr()
        {
            return (Helpers::FixturesDir() / "shaders").string();
        }

        class CaptureRenderDevice final : public RenderDevice
        {
        public:
            Result<void> Initialise(Window&) override
            {
                return {};
            }

            void Shutdown() override {}

            bool BeginFrame() override
            {
                return true;
            }

            void EndFrame() override {}

            void PushDebugGroup(std::string_view) override {}
            void PopDebugGroup() override {}

            bool BeginRenderPass(const RenderPassDescriptor&) override
            {
                return true;
            }

            void EndRenderPass() override {}

            GPUShaderHandle CreateShader(const ShaderCreateDesc& desc) override
            {
                CreatedShaders.push_back(desc);
                return GPUShaderHandle{.Index = ++m_nextShaderId, .Generation = 1};
            }

            void DestroyShader(GPUShaderHandle handle) override
            {
                DestroyedShaders.push_back(handle);
            }

            GPUPipelineHandle CreatePipeline(const PipelineCreateDesc&) override
            {
                return {};
            }

            void DestroyPipeline(GPUPipelineHandle) override {}
            void BindPipeline(GPUPipelineHandle) override {}

            GPUBufferHandle CreateBuffer(const BufferCreateDesc&) override
            {
                return {};
            }

            void DestroyBuffer(GPUBufferHandle) override {}
            void UploadToBuffer(GPUBufferHandle, const void*, BufferUploadRegion) override {}
            void BindVertexBuffer(GPUBufferHandle, VertexBufferBindingDesc) override {}
            void BindIndexBuffer(GPUBufferHandle, IndexElementSize, uint32_t) override {}
            void DrawIndexed(uint32_t, uint32_t, uint32_t, int32_t) override {}
            void DrawPrimitives(uint32_t, uint32_t, uint32_t) override {}
            void PushVertexUniform(uint32_t, const void*, uint32_t) override {}
            void PushFragmentUniform(uint32_t, const void*, uint32_t) override {}

            GPUComputePipelineHandle CreateComputePipeline(const ComputePipelineCreateDesc&) override
            {
                return {};
            }

            void DestroyComputePipeline(GPUComputePipelineHandle) override {}
            void BeginComputePass() override {}
            void EndComputePass() override {}
            void BindComputePipeline(GPUComputePipelineHandle) override {}
            void DispatchCompute(uint32_t, uint32_t, uint32_t) override {}

            GPUTextureHandle CreateTexture(const TextureCreateDesc&) override
            {
                return GPUTextureHandle{.Index = ++m_nextTextureId, .Generation = 1};
            }

            void DestroyTexture(GPUTextureHandle) override {}
            void UploadToTexture(GPUTextureHandle, const void*, uint32_t, uint32_t, uint32_t, uint32_t) override {}
            void GenerateMipmaps(GPUTextureHandle) override {}

            GPUSamplerHandle CreateSampler(const SamplerCreateDesc&) override
            {
                return GPUSamplerHandle{.Index = ++m_nextSamplerId, .Generation = 1};
            }

            void DestroySampler(GPUSamplerHandle) override {}
            void BindFragmentSampler(uint32_t, GPUTextureHandle, GPUSamplerHandle) override {}

            [[nodiscard]] Extent2D GetSwapchainDimensions() const override
            {
                return {};
            }

            const RenderDeviceInfo& GetDeviceInfo() const override
            {
                return m_info;
            }

            std::vector<ShaderCreateDesc> CreatedShaders;
            std::vector<GPUShaderHandle> DestroyedShaders;

        private:
            RenderDeviceInfo m_info{.BackendName = "CaptureRenderDevice"};
            uint32_t m_nextShaderId = 0;
            uint32_t m_nextTextureId = 0;
            uint32_t m_nextSamplerId = 0;
        };

        auto EnsureCleanDirectory(const std::filesystem::path& directory) -> bool
        {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            if (error)
            {
                return false;
            }

            std::filesystem::create_directories(directory, error);
            return not error;
        }

        auto WriteBinaryFile(const std::filesystem::path& path, const std::vector<uint8_t>& contents) -> bool
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (not file.is_open())
            {
                return false;
            }

            file.write(reinterpret_cast<const char*>(contents.data()), static_cast<std::streamsize>(contents.size()));
            return static_cast<bool>(file);
        }

        auto WriteTextFile(const std::filesystem::path& path, const std::string_view contents) -> bool
        {
            std::ofstream file(path, std::ios::trunc);
            if (not file.is_open())
            {
                return false;
            }

            file << contents;
            return static_cast<bool>(file);
        }

        auto MakeManifestJson(const ShaderResourceCounts& vertexCounts, const ShaderResourceCounts& fragmentCounts) -> std::string
        {
            return std::format(
                R"({{"simple":{{"vertex":{{"uniformBuffers":{},"samplers":{},"storageTextures":{},"storageBuffers":{}}},"fragment":{{"uniformBuffers":{},"samplers":{},"storageTextures":{},"storageBuffers":{}}}}}}})",
                vertexCounts.UniformBuffers, vertexCounts.Samplers, vertexCounts.StorageTextures, vertexCounts.StorageBuffers, fragmentCounts.UniformBuffers, fragmentCounts.Samplers, fragmentCounts.StorageTextures,
                fragmentCounts.StorageBuffers);
        }
    } // namespace

    TEST_SUITE("SlangCompilerReflection")
    {
        TEST_CASE("reflection extracts expected resource counts for textured-style fixture")
        {
            const std::string fixturesDir = ShaderFixturesDirStr();
            SlangCompiler compiler;
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = fixturesDir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto vertex = compiler.Compile("reflect_material_textured", "VSMain", ShaderStage::Vertex);
            REQUIRE(vertex.has_value());
            REQUIRE(vertex->Resources.has_value());
            CHECK(vertex->Resources->UniformBuffers == 1);
            CHECK(vertex->Resources->Samplers == 0);
            CHECK(vertex->Resources->StorageTextures == 0);
            CHECK(vertex->Resources->StorageBuffers == 0);

            auto fragment = compiler.Compile("reflect_material_textured", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            REQUIRE(fragment->Resources.has_value());
            CHECK(fragment->Resources->UniformBuffers == 2);
            CHECK(fragment->Resources->Samplers == 1);
            CHECK(fragment->Resources->StorageTextures == 0);
            CHECK(fragment->Resources->StorageBuffers == 0);
        }

        TEST_CASE("reflection extracts expected resource counts for fullscreen blit fixture")
        {
            const std::string fixturesDir = ShaderFixturesDirStr();
            SlangCompiler compiler;
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = fixturesDir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto vertex = compiler.Compile("reflect_fullscreen_blit", "VSMain", ShaderStage::Vertex);
            REQUIRE(vertex.has_value());
            REQUIRE(vertex->Resources.has_value());
            CHECK(vertex->Resources->UniformBuffers == 0);
            CHECK(vertex->Resources->Samplers == 0);

            auto fragment = compiler.Compile("reflect_fullscreen_blit", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            REQUIRE(fragment->Resources.has_value());
            CHECK(fragment->Resources->UniformBuffers == 0);
            CHECK(fragment->Resources->Samplers == 1);
        }

        TEST_CASE("reflection counts every binding range for parameter-block fixtures")
        {
            const std::string fixturesDir = ShaderFixturesDirStr();
            SlangCompiler compiler;
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = fixturesDir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto vertex = compiler.Compile("reflect_multi_binding", "VSMain", ShaderStage::Vertex);
            REQUIRE(vertex.has_value());
            REQUIRE(vertex->Resources.has_value());
            CHECK(vertex->Resources->UniformBuffers == 1);
            CHECK(vertex->Resources->Samplers == 1);
            CHECK(vertex->Resources->StorageTextures == 0);
            CHECK(vertex->Resources->StorageBuffers == 0);

            auto fragment = compiler.Compile("reflect_multi_binding", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            REQUIRE(fragment->Resources.has_value());
            CHECK(fragment->Resources->UniformBuffers == 1);
            CHECK(fragment->Resources->Samplers == 1);
            CHECK(fragment->Resources->StorageTextures == 0);
            CHECK(fragment->Resources->StorageBuffers == 0);
        }

        TEST_CASE("reflection extracts storage resource counts")
        {
            const std::string fixturesDir = ShaderFixturesDirStr();
            SlangCompiler compiler;
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = fixturesDir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto vertex = compiler.Compile("reflect_storage_resources", "VSMain", ShaderStage::Vertex);
            REQUIRE(vertex.has_value());
            REQUIRE(vertex->Resources.has_value());
            CHECK(vertex->Resources->UniformBuffers == 0);
            CHECK(vertex->Resources->Samplers == 0);
            CHECK(vertex->Resources->StorageTextures > 0);
            CHECK(vertex->Resources->StorageBuffers > 0);

            auto fragment = compiler.Compile("reflect_storage_resources", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            REQUIRE(fragment->Resources.has_value());
            CHECK(fragment->Resources->UniformBuffers == 0);
            CHECK(fragment->Resources->Samplers == 0);
            CHECK(fragment->Resources->StorageTextures > 0);
            CHECK(fragment->Resources->StorageBuffers > 0);
        }

        TEST_CASE("shader manager reloads manifest-backed resource counts")
        {
            const std::string fixturesDir = ShaderFixturesDirStr();
            SlangCompiler compiler;
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = fixturesDir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto vertex = compiler.Compile("simple", "VSMain", ShaderStage::Vertex);
            REQUIRE(vertex.has_value());

            auto fragment = compiler.Compile("simple", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());

            const auto shaderDir = Helpers::FixturesDir() / "temp" / "shader_manager_manifest_reload";
            REQUIRE(EnsureCleanDirectory(shaderDir));
            REQUIRE(WriteBinaryFile(shaderDir / "simple.vert.spv", vertex->Bytecode));
            REQUIRE(WriteBinaryFile(shaderDir / "simple.frag.spv", fragment->Bytecode));

            const ShaderResourceCounts initialVertexCounts{
                .UniformBuffers = 1,
                .Samplers = 2,
                .StorageTextures = 3,
                .StorageBuffers = 4,
            };
            const ShaderResourceCounts initialFragmentCounts{
                .UniformBuffers = 5,
                .Samplers = 6,
                .StorageTextures = 7,
                .StorageBuffers = 8,
            };
            REQUIRE(WriteTextFile(shaderDir / "shader_manifest.json", MakeManifestJson(initialVertexCounts, initialFragmentCounts)));

            CaptureRenderDevice device;
            ShaderManager manager;
            manager.Initialise(device, shaderDir.string(), nullptr);

            auto firstVertexHandle = manager.GetShader("simple", ShaderStage::Vertex);
            REQUIRE(firstVertexHandle.IsValid());
            REQUIRE(device.CreatedShaders.size() == 1);
            CHECK(device.CreatedShaders.back().UniformBuffers == initialVertexCounts.UniformBuffers);
            CHECK(device.CreatedShaders.back().Samplers == initialVertexCounts.Samplers);
            CHECK(device.CreatedShaders.back().StorageTextures == initialVertexCounts.StorageTextures);
            CHECK(device.CreatedShaders.back().StorageBuffers == initialVertexCounts.StorageBuffers);

            const ShaderResourceCounts reloadedVertexCounts{
                .UniformBuffers = 9,
                .Samplers = 10,
                .StorageTextures = 11,
                .StorageBuffers = 12,
            };
            const ShaderResourceCounts reloadedFragmentCounts{
                .UniformBuffers = 13,
                .Samplers = 14,
                .StorageTextures = 15,
                .StorageBuffers = 16,
            };
            REQUIRE(WriteTextFile(shaderDir / "shader_manifest.json", MakeManifestJson(reloadedVertexCounts, reloadedFragmentCounts)));

            device.CreatedShaders.clear();
            manager.ReloadShaders();

            auto reloadedVertexHandle = manager.GetShader("simple", ShaderStage::Vertex);
            REQUIRE(reloadedVertexHandle.IsValid());
            REQUIRE(device.CreatedShaders.size() == 1);
            CHECK(device.CreatedShaders.back().UniformBuffers == reloadedVertexCounts.UniformBuffers);
            CHECK(device.CreatedShaders.back().Samplers == reloadedVertexCounts.Samplers);
            CHECK(device.CreatedShaders.back().StorageTextures == reloadedVertexCounts.StorageTextures);
            CHECK(device.CreatedShaders.back().StorageBuffers == reloadedVertexCounts.StorageBuffers);

            manager.Shutdown();
        }

        TEST_CASE("shader manager rejects manifests with missing stage fields")
        {
            const std::string fixturesDir = ShaderFixturesDirStr();
            SlangCompiler compiler;
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = fixturesDir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto vertex = compiler.Compile("simple", "VSMain", ShaderStage::Vertex);
            REQUIRE(vertex.has_value());

            auto fragment = compiler.Compile("simple", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());

            const auto shaderDir = Helpers::FixturesDir() / "temp" / "shader_manager_manifest_validation";
            REQUIRE(EnsureCleanDirectory(shaderDir));
            REQUIRE(WriteBinaryFile(shaderDir / "simple.vert.spv", vertex->Bytecode));
            REQUIRE(WriteBinaryFile(shaderDir / "simple.frag.spv", fragment->Bytecode));
            REQUIRE(WriteTextFile(
                shaderDir / "shader_manifest.json", R"({"simple":{"vertex":{"uniformBuffers":1,"samplers":2,"storageTextures":3},"fragment":{"uniformBuffers":0,"samplers":0,"storageTextures":0,"storageBuffers":0}}})"));

            CaptureRenderDevice device;
            ShaderManager manager;
            manager.Initialise(device, shaderDir.string(), nullptr);

            CHECK_FALSE(manager.LoadManifest((shaderDir / "shader_manifest.json").string()));
            CHECK_FALSE(manager.GetShader("simple", ShaderStage::Vertex).IsValid());
            CHECK(device.CreatedShaders.empty());

            manager.Shutdown();
        }

        TEST_CASE("simple shader fixture has no resource bindings")
        {
            const std::string fixturesDir = ShaderFixturesDirStr();
            SlangCompiler compiler;
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = fixturesDir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto vertex = compiler.Compile("simple", "VSMain", ShaderStage::Vertex);
            REQUIRE(vertex.has_value());
            REQUIRE(vertex->Resources.has_value());
            CHECK(vertex->Resources->UniformBuffers == 0);
            CHECK(vertex->Resources->Samplers == 0);

            auto fragment = compiler.Compile("simple", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            REQUIRE(fragment->Resources.has_value());
            CHECK(fragment->Resources->UniformBuffers == 0);
            CHECK(fragment->Resources->Samplers == 0);
        }
    }

} // namespace Wayfinder::Tests
