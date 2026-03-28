#include "BuiltInShaderPrograms.h"

#include "rendering/backend/VertexFormats.h"
#include "rendering/materials/ShaderProgram.h"
#include "rendering/pipeline/BuiltInUBOs.h"

namespace Wayfinder
{
    void RegisterBuiltInShaderPrograms(ShaderProgramRegistry& registry)
    {
        {
            ShaderProgramDesc desc;
            desc.Name = "unlit";
            desc.VertexShaderName = "unlit";
            desc.FragmentShaderName = "unlit";
            desc.VertexResources = {.numUniformBuffers = 1};
            desc.FragmentResources = {.numUniformBuffers = 1};
            desc.VertexLayout = VertexLayouts::PosNormalColour;
            desc.Cull = CullMode::Back;
            desc.DepthTest = true;
            desc.DepthWrite = true;
            desc.MaterialParams =
            {
                {.Name = "base_colour", .Type = MaterialParamType::Colour, .Offset = 0, .Default = LinearColour::White()},
            };
            desc.MaterialUBOSize = 16;
            desc.VertexUBOSize = sizeof(UnlitTransformUBO);
            desc.NeedsSceneGlobals = false;

            registry.Register(desc);
        }

        {
            ShaderProgramDesc desc;
            desc.Name = "unlit_blended";
            desc.VertexShaderName = "unlit";
            desc.FragmentShaderName = "unlit";
            desc.VertexResources = {.numUniformBuffers = 1};
            desc.FragmentResources = {.numUniformBuffers = 1};
            desc.VertexLayout = VertexLayouts::PosNormalColour;
            desc.Cull = CullMode::Back;
            desc.DepthTest = true;
            desc.DepthWrite = false;
            desc.Blend = BlendPresets::AlphaBlend();
            desc.MaterialParams =
            {
                {.Name = "base_colour", .Type = MaterialParamType::Colour, .Offset = 0, .Default = LinearColour::White()},
            };
            desc.MaterialUBOSize = 16;
            desc.VertexUBOSize = sizeof(UnlitTransformUBO);
            desc.NeedsSceneGlobals = false;

            registry.Register(desc);
        }

        {
            ShaderProgramDesc desc;
            desc.Name = "basic_lit";
            desc.VertexShaderName = "basic_lit";
            desc.FragmentShaderName = "basic_lit";
            desc.VertexResources = {.numUniformBuffers = 1};
            desc.FragmentResources = {.numUniformBuffers = 2};
            desc.VertexLayout = VertexLayouts::PosNormalColour;
            desc.Cull = CullMode::Back;
            desc.DepthTest = true;
            desc.DepthWrite = true;
            desc.MaterialParams =
            {
                {.Name = "base_colour", .Type = MaterialParamType::Colour, .Offset = 0, .Default = LinearColour::White()},
            };
            desc.MaterialUBOSize = 16;
            desc.VertexUBOSize = sizeof(TransformUBO);
            desc.NeedsSceneGlobals = true;

            registry.Register(desc);
        }

        {
            ShaderProgramDesc desc;
            desc.Name = "textured_lit";
            desc.VertexShaderName = "textured_lit";
            desc.FragmentShaderName = "textured_lit";
            desc.VertexResources = {.numUniformBuffers = 1};
            desc.FragmentResources = {.numUniformBuffers = 2, .numSamplers = 1};
            desc.VertexLayout = VertexLayouts::PosNormalUVTangent;
            desc.Cull = CullMode::Back;
            desc.DepthTest = true;
            desc.DepthWrite = true;
            desc.MaterialParams =
            {
                {.Name = "base_colour", .Type = MaterialParamType::Colour, .Offset = 0, .Default = LinearColour::White()},
            };
            desc.MaterialUBOSize = 16;
            desc.VertexUBOSize = sizeof(TransformUBO);
            desc.NeedsSceneGlobals = true;
            desc.TextureSlots = {{.Name = "diffuse", .BindingSlot = 0}};

            registry.Register(desc);
        }
    }

} // namespace Wayfinder
