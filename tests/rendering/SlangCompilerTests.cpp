#include "TestHelpers.h"
#include "rendering/materials/SlangCompiler.h"

#include <cstring>
#include <doctest/doctest.h>

namespace Wayfinder::Tests
{
    namespace
    {
        std::string ShaderFixturesDirStr()
        {
            return (Helpers::FixturesDir() / "shaders").string();
        }

        constexpr uint32_t SPIRV_MAGIC = 0x07230203;
    } // namespace

    TEST_SUITE("SlangCompiler")
    {
        TEST_CASE("initialises with valid source directory")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;

            auto result = compiler.Initialise(desc);
            CHECK(result.has_value());
            CHECK(compiler.IsInitialised());
        }

        TEST_CASE("fails gracefully with invalid source directory")
        {
            SlangCompiler compiler;
            SlangCompiler::InitDesc desc;
            const auto nonexistent = (Helpers::FixturesDir() / "nonexistent-path").string();
            desc.SourceDirectory = nonexistent;

            auto result = compiler.Initialise(desc);
            CHECK_FALSE(result.has_value());
            CHECK_FALSE(compiler.IsInitialised());
        }

        TEST_CASE("double initialise returns error")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;

            REQUIRE(compiler.Initialise(desc).has_value());
            auto second = compiler.Initialise(desc);
            CHECK_FALSE(second.has_value());
        }

        TEST_CASE("compile fails when not initialised")
        {
            SlangCompiler compiler;
            auto result = compiler.Compile("simple", "VSMain", ShaderStage::Vertex);
            CHECK_FALSE(result.has_value());
        }

        TEST_CASE("compiles a simple vertex shader to SPIR-V")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto result = compiler.Compile("simple", "VSMain", ShaderStage::Vertex);
            REQUIRE(result.has_value());
            CHECK(result->Bytecode.size() >= 4);

            // Verify SPIR-V magic number
            uint32_t magic = 0;
            std::memcpy(&magic, result->Bytecode.data(), sizeof(magic));
            CHECK(magic == SPIRV_MAGIC);
        }

        TEST_CASE("compiles a simple fragment shader to SPIR-V")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto result = compiler.Compile("simple", "PSMain", ShaderStage::Fragment);
            REQUIRE(result.has_value());
            CHECK(result->Bytecode.size() >= 4);

            uint32_t magic = 0;
            std::memcpy(&magic, result->Bytecode.data(), sizeof(magic));
            CHECK(magic == SPIRV_MAGIC);
        }

        TEST_CASE("compilation fails for non-existent source file")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto result = compiler.Compile("this_shader_does_not_exist", "VSMain", ShaderStage::Vertex);
            CHECK_FALSE(result.has_value());
        }

        TEST_CASE("compilation fails for invalid shader code")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto result = compiler.Compile("invalid_syntax", "VSMain", ShaderStage::Vertex);
            CHECK_FALSE(result.has_value());
        }

        TEST_CASE("compilation fails for non-existent entry point")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto result = compiler.Compile("simple", "NoSuchEntryPoint", ShaderStage::Vertex);
            CHECK_FALSE(result.has_value());
        }

        TEST_CASE("module imports resolve correctly")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto result = compiler.Compile("with_import", "VSMain", ShaderStage::Vertex);
            REQUIRE(result.has_value());
            CHECK(result->Bytecode.size() >= 4);

            uint32_t magic = 0;
            std::memcpy(&magic, result->Bytecode.data(), sizeof(magic));
            CHECK(magic == SPIRV_MAGIC);
        }

        TEST_CASE("multiple compilations from same session succeed")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto vertex = compiler.Compile("simple", "VSMain", ShaderStage::Vertex);
            REQUIRE(vertex.has_value());
            CHECK(vertex->Bytecode.size() >= 4);

            auto fragment = compiler.Compile("simple", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            CHECK(fragment->Bytecode.size() >= 4);

            // Both should be valid but different
            CHECK(vertex->Bytecode != fragment->Bytecode);
        }

        TEST_CASE("shutdown and reinitialise works")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;

            REQUIRE(compiler.Initialise(desc).has_value());
            CHECK(compiler.IsInitialised());

            compiler.Shutdown();
            CHECK_FALSE(compiler.IsInitialised());

            REQUIRE(compiler.Initialise(desc).has_value());
            CHECK(compiler.IsInitialised());

            auto result = compiler.Compile("simple", "VSMain", ShaderStage::Vertex);
            CHECK(result.has_value());
        }

        TEST_CASE("ResetSession allows recompilation with fresh state")
        {
            SlangCompiler compiler;
            const auto dir = ShaderFixturesDirStr();
            SlangCompiler::InitDesc desc;
            desc.SourceDirectory = dir;
            REQUIRE(compiler.Initialise(desc).has_value());

            auto first = compiler.Compile("simple", "VSMain", ShaderStage::Vertex);
            REQUIRE(first.has_value());
            CHECK(first->Bytecode.size() >= 4);

            auto resetResult = compiler.ResetSession();
            REQUIRE(resetResult.has_value());
            CHECK(compiler.IsInitialised());

            auto second = compiler.Compile("simple", "VSMain", ShaderStage::Vertex);
            REQUIRE(second.has_value());
            CHECK(second->Bytecode.size() >= 4);

            uint32_t magic = 0;
            std::memcpy(&magic, second->Bytecode.data(), sizeof(magic));
            CHECK(magic == SPIRV_MAGIC);
        }
    }

} // namespace Wayfinder::Tests
