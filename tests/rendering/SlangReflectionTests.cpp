#include "TestHelpers.h"
#include "rendering/materials/SlangCompiler.h"

#include <doctest/doctest.h>

#include <string>

namespace Wayfinder::Tests
{
    namespace
    {
        std::string ShaderFixturesDirStr()
        {
            return (Helpers::FixturesDir() / "shaders").string();
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
            CHECK(vertex->Resources.UniformBuffers == 1);
            CHECK(vertex->Resources.Samplers == 0);
            CHECK(vertex->Resources.StorageTextures == 0);
            CHECK(vertex->Resources.StorageBuffers == 0);

            auto fragment = compiler.Compile("reflect_material_textured", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            CHECK(fragment->Resources.UniformBuffers == 2);
            CHECK(fragment->Resources.Samplers == 1);
            CHECK(fragment->Resources.StorageTextures == 0);
            CHECK(fragment->Resources.StorageBuffers == 0);
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
            CHECK(vertex->Resources.UniformBuffers == 0);
            CHECK(vertex->Resources.Samplers == 0);

            auto fragment = compiler.Compile("reflect_fullscreen_blit", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            CHECK(fragment->Resources.UniformBuffers == 0);
            CHECK(fragment->Resources.Samplers == 1);
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
            CHECK(vertex->Resources.UniformBuffers == 1);
            CHECK(vertex->Resources.Samplers == 1);
            CHECK(vertex->Resources.StorageTextures == 0);
            CHECK(vertex->Resources.StorageBuffers == 0);

            auto fragment = compiler.Compile("reflect_multi_binding", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            CHECK(fragment->Resources.UniformBuffers == 1);
            CHECK(fragment->Resources.Samplers == 1);
            CHECK(fragment->Resources.StorageTextures == 0);
            CHECK(fragment->Resources.StorageBuffers == 0);
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
            CHECK(vertex->Resources.UniformBuffers == 0);
            CHECK(vertex->Resources.Samplers == 0);

            auto fragment = compiler.Compile("simple", "PSMain", ShaderStage::Fragment);
            REQUIRE(fragment.has_value());
            CHECK(fragment->Resources.UniformBuffers == 0);
            CHECK(fragment->Resources.Samplers == 0);
        }
    }

} // namespace Wayfinder::Tests
