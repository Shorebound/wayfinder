#include "Mesh.h"
#include "core/Log.h"
#include "maths/Maths.h"

#include <array>
#include <cmath>

namespace Wayfinder
{
    namespace
    {
        Mesh CreateCube(RenderDevice& device, float size)
        {
            const float h = size * 0.5f;

            constexpr Float3 NORMAL_FRONT = {0.0f, 0.0f, 1.0f};
            constexpr Float3 NORMAL_BACK = {0.0f, 0.0f, -1.0f};
            constexpr Float3 NORMAL_TOP = {0.0f, 1.0f, 0.0f};
            constexpr Float3 NORMAL_BOTTOM = {0.0f, -1.0f, 0.0f};
            constexpr Float3 NORMAL_RIGHT = {1.0f, 0.0f, 0.0f};
            constexpr Float3 NORMAL_LEFT = {-1.0f, 0.0f, 0.0f};

            constexpr Float3 COLOUR_FRONT = {0.9f, 0.2f, 0.2f};
            constexpr Float3 COLOUR_BACK = {0.2f, 0.8f, 0.2f};
            constexpr Float3 COLOUR_TOP = {0.2f, 0.4f, 0.9f};
            constexpr Float3 COLOUR_BOTTOM = {0.9f, 0.9f, 0.2f};
            constexpr Float3 COLOUR_RIGHT = {0.9f, 0.2f, 0.9f};
            constexpr Float3 COLOUR_LEFT = {0.2f, 0.9f, 0.9f};

            const std::array<VertexPositionNormalColour, 24> vertices = {{
                {.Position = {-h, -h, h}, .Normal = NORMAL_FRONT, .Colour = COLOUR_FRONT},
                {.Position = {h, -h, h}, .Normal = NORMAL_FRONT, .Colour = COLOUR_FRONT},
                {.Position = {h, h, h}, .Normal = NORMAL_FRONT, .Colour = COLOUR_FRONT},
                {.Position = {-h, h, h}, .Normal = NORMAL_FRONT, .Colour = COLOUR_FRONT},
                {.Position = {h, -h, -h}, .Normal = NORMAL_BACK, .Colour = COLOUR_BACK},
                {.Position = {-h, -h, -h}, .Normal = NORMAL_BACK, .Colour = COLOUR_BACK},
                {.Position = {-h, h, -h}, .Normal = NORMAL_BACK, .Colour = COLOUR_BACK},
                {.Position = {h, h, -h}, .Normal = NORMAL_BACK, .Colour = COLOUR_BACK},
                {.Position = {-h, h, h}, .Normal = NORMAL_TOP, .Colour = COLOUR_TOP},
                {.Position = {h, h, h}, .Normal = NORMAL_TOP, .Colour = COLOUR_TOP},
                {.Position = {h, h, -h}, .Normal = NORMAL_TOP, .Colour = COLOUR_TOP},
                {.Position = {-h, h, -h}, .Normal = NORMAL_TOP, .Colour = COLOUR_TOP},
                {.Position = {-h, -h, -h}, .Normal = NORMAL_BOTTOM, .Colour = COLOUR_BOTTOM},
                {.Position = {h, -h, -h}, .Normal = NORMAL_BOTTOM, .Colour = COLOUR_BOTTOM},
                {.Position = {h, -h, h}, .Normal = NORMAL_BOTTOM, .Colour = COLOUR_BOTTOM},
                {.Position = {-h, -h, h}, .Normal = NORMAL_BOTTOM, .Colour = COLOUR_BOTTOM},
                {.Position = {h, -h, h}, .Normal = NORMAL_RIGHT, .Colour = COLOUR_RIGHT},
                {.Position = {h, -h, -h}, .Normal = NORMAL_RIGHT, .Colour = COLOUR_RIGHT},
                {.Position = {h, h, -h}, .Normal = NORMAL_RIGHT, .Colour = COLOUR_RIGHT},
                {.Position = {h, h, h}, .Normal = NORMAL_RIGHT, .Colour = COLOUR_RIGHT},
                {.Position = {-h, -h, -h}, .Normal = NORMAL_LEFT, .Colour = COLOUR_LEFT},
                {.Position = {-h, -h, h}, .Normal = NORMAL_LEFT, .Colour = COLOUR_LEFT},
                {.Position = {-h, h, h}, .Normal = NORMAL_LEFT, .Colour = COLOUR_LEFT},
                {.Position = {-h, h, -h}, .Normal = NORMAL_LEFT, .Colour = COLOUR_LEFT},
            }};

            const std::array<uint16_t, 36> indices = {{
                0,
                1,
                2,
                0,
                2,
                3,
                4,
                5,
                6,
                4,
                6,
                7,
                8,
                9,
                10,
                8,
                10,
                11,
                12,
                13,
                14,
                12,
                14,
                15,
                16,
                17,
                18,
                16,
                18,
                19,
                20,
                21,
                22,
                20,
                22,
                23,
            }};

            Mesh mesh;
            if (!mesh.Create(device, {
                                         .VertexData = vertices.data(),
                                         .VertexDataSize = static_cast<uint32_t>(vertices.size() * sizeof(VertexPositionNormalColour)),
                                         .VertexCount = static_cast<uint32_t>(vertices.size()),
                                         .IndexData = indices.data(),
                                         .IndexDataSize = static_cast<uint32_t>(indices.size() * sizeof(uint16_t)),
                                         .IndexCount = static_cast<uint32_t>(indices.size()),
                                         .IndexElementType = IndexElementSize::Uint16,
                                     }))
            {
                Log::Error(LogRenderer, "Mesh: Failed to create primitive cube");
                return {};
            }

            return mesh;
        }

        Float4 ApproxTangentForNormal(const Float3& normal)
        {
            Float3 tangent = Maths::Normalize(Maths::Cross(Up, normal));
            if (std::abs(Maths::Dot(normal, Up)) > 0.99f)
            {
                tangent = Maths::Normalize(Maths::Cross(Right, normal));
            }

            return Float4{tangent, 1.0f};
        }

        Mesh CreateTexturedCube(RenderDevice& device, float size)
        {
            const float h = size * 0.5f;

            constexpr Float3 NORMAL_FRONT = {0.0f, 0.0f, 1.0f};
            constexpr Float3 NORMAL_BACK = {0.0f, 0.0f, -1.0f};
            constexpr Float3 NORMAL_TOP = {0.0f, 1.0f, 0.0f};
            constexpr Float3 NORMAL_BOTTOM = {0.0f, -1.0f, 0.0f};
            constexpr Float3 NORMAL_RIGHT = {1.0f, 0.0f, 0.0f};
            constexpr Float3 NORMAL_LEFT = {-1.0f, 0.0f, 0.0f};

            constexpr Float2 UV00 = {0.0f, 1.0f};
            constexpr Float2 UV10 = {1.0f, 1.0f};
            constexpr Float2 UV11 = {1.0f, 0.0f};
            constexpr Float2 UV01 = {0.0f, 0.0f};

            const std::array<VertexPositionNormalUVTangent, 24> vertices = {{
                {.Position = {-h, -h, h}, .Normal = NORMAL_FRONT, .UV = UV00, .Tangent = ApproxTangentForNormal(NORMAL_FRONT)},
                {.Position = {h, -h, h}, .Normal = NORMAL_FRONT, .UV = UV10, .Tangent = ApproxTangentForNormal(NORMAL_FRONT)},
                {.Position = {h, h, h}, .Normal = NORMAL_FRONT, .UV = UV11, .Tangent = ApproxTangentForNormal(NORMAL_FRONT)},
                {.Position = {-h, h, h}, .Normal = NORMAL_FRONT, .UV = UV01, .Tangent = ApproxTangentForNormal(NORMAL_FRONT)},
                {.Position = {h, -h, -h}, .Normal = NORMAL_BACK, .UV = UV00, .Tangent = ApproxTangentForNormal(NORMAL_BACK)},
                {.Position = {-h, -h, -h}, .Normal = NORMAL_BACK, .UV = UV10, .Tangent = ApproxTangentForNormal(NORMAL_BACK)},
                {.Position = {-h, h, -h}, .Normal = NORMAL_BACK, .UV = UV11, .Tangent = ApproxTangentForNormal(NORMAL_BACK)},
                {.Position = {h, h, -h}, .Normal = NORMAL_BACK, .UV = UV01, .Tangent = ApproxTangentForNormal(NORMAL_BACK)},
                {.Position = {-h, h, h}, .Normal = NORMAL_TOP, .UV = UV00, .Tangent = ApproxTangentForNormal(NORMAL_TOP)},
                {.Position = {h, h, h}, .Normal = NORMAL_TOP, .UV = UV10, .Tangent = ApproxTangentForNormal(NORMAL_TOP)},
                {.Position = {h, h, -h}, .Normal = NORMAL_TOP, .UV = UV11, .Tangent = ApproxTangentForNormal(NORMAL_TOP)},
                {.Position = {-h, h, -h}, .Normal = NORMAL_TOP, .UV = UV01, .Tangent = ApproxTangentForNormal(NORMAL_TOP)},
                {.Position = {-h, -h, -h}, .Normal = NORMAL_BOTTOM, .UV = UV00, .Tangent = ApproxTangentForNormal(NORMAL_BOTTOM)},
                {.Position = {h, -h, -h}, .Normal = NORMAL_BOTTOM, .UV = UV10, .Tangent = ApproxTangentForNormal(NORMAL_BOTTOM)},
                {.Position = {h, -h, h}, .Normal = NORMAL_BOTTOM, .UV = UV11, .Tangent = ApproxTangentForNormal(NORMAL_BOTTOM)},
                {.Position = {-h, -h, h}, .Normal = NORMAL_BOTTOM, .UV = UV01, .Tangent = ApproxTangentForNormal(NORMAL_BOTTOM)},
                {.Position = {h, -h, h}, .Normal = NORMAL_RIGHT, .UV = UV00, .Tangent = ApproxTangentForNormal(NORMAL_RIGHT)},
                {.Position = {h, -h, -h}, .Normal = NORMAL_RIGHT, .UV = UV10, .Tangent = ApproxTangentForNormal(NORMAL_RIGHT)},
                {.Position = {h, h, -h}, .Normal = NORMAL_RIGHT, .UV = UV11, .Tangent = ApproxTangentForNormal(NORMAL_RIGHT)},
                {.Position = {h, h, h}, .Normal = NORMAL_RIGHT, .UV = UV01, .Tangent = ApproxTangentForNormal(NORMAL_RIGHT)},
                {.Position = {-h, -h, -h}, .Normal = NORMAL_LEFT, .UV = UV00, .Tangent = ApproxTangentForNormal(NORMAL_LEFT)},
                {.Position = {-h, -h, h}, .Normal = NORMAL_LEFT, .UV = UV10, .Tangent = ApproxTangentForNormal(NORMAL_LEFT)},
                {.Position = {-h, h, h}, .Normal = NORMAL_LEFT, .UV = UV11, .Tangent = ApproxTangentForNormal(NORMAL_LEFT)},
                {.Position = {-h, h, -h}, .Normal = NORMAL_LEFT, .UV = UV01, .Tangent = ApproxTangentForNormal(NORMAL_LEFT)},
            }};

            const std::array<uint16_t, 36> indices = {{
                0,
                1,
                2,
                0,
                2,
                3,
                4,
                5,
                6,
                4,
                6,
                7,
                8,
                9,
                10,
                8,
                10,
                11,
                12,
                13,
                14,
                12,
                14,
                15,
                16,
                17,
                18,
                16,
                18,
                19,
                20,
                21,
                22,
                20,
                22,
                23,
            }};

            Mesh mesh;
            if (!mesh.Create(device, {
                                         .VertexData = vertices.data(),
                                         .VertexDataSize = static_cast<uint32_t>(vertices.size() * sizeof(VertexPositionNormalUVTangent)),
                                         .VertexCount = static_cast<uint32_t>(vertices.size()),
                                         .IndexData = indices.data(),
                                         .IndexDataSize = static_cast<uint32_t>(indices.size() * sizeof(uint16_t)),
                                         .IndexCount = static_cast<uint32_t>(indices.size()),
                                         .IndexElementType = IndexElementSize::Uint16,
                                     }))
            {
                Log::Error(LogRenderer, "Mesh: Failed to create textured primitive cube");
                return {};
            }

            return mesh;
        }

    } // namespace

    bool Mesh::Create(RenderDevice& device, const MeshCreateDesc& desc)
    {
        if (!m_vertexBuffer.Create(device, BufferUsage::Vertex, desc.VertexDataSize))
        {
            Log::Error(LogRenderer, "Mesh: Failed to create vertex buffer");
            return false;
        }

        if (!m_indexBuffer.Create(device, BufferUsage::Index, desc.IndexDataSize))
        {
            Log::Error(LogRenderer, "Mesh: Failed to create index buffer");
            m_vertexBuffer.Destroy();
            return false;
        }

        m_vertexBuffer.Upload(desc.VertexData, desc.VertexDataSize);
        m_indexBuffer.Upload(desc.IndexData, desc.IndexDataSize);

        m_vertexCount = desc.VertexCount;
        m_indexCount = desc.IndexCount;
        m_indexElementSize = desc.IndexElementType;

        Log::Info(LogRenderer, "Mesh: Created ({} verts, {} indices)", desc.VertexCount, desc.IndexCount);
        return true;
    }

    void Mesh::Destroy()
    {
        m_vertexBuffer.Destroy();
        m_indexBuffer.Destroy();
        m_indexCount = 0;
        m_vertexCount = 0;
    }

    void Mesh::Bind(RenderDevice& device) const
    {
        device.BindVertexBuffer(m_vertexBuffer.GetHandle());
        device.BindIndexBuffer(m_indexBuffer.GetHandle(), m_indexElementSize);
    }

    void Mesh::Draw(RenderDevice& device, uint32_t instanceCount) const
    {
        device.DrawIndexed(m_indexCount, instanceCount);
    }

    Mesh Mesh::CreatePrimitive(RenderDevice& device, const PrimitiveDesc& desc)
    {
        switch (desc.Shape)
        {
        case PrimitiveShape::Cube:
            return CreateCube(device, desc.Size);
        default:
            Log::Error(LogRenderer, "Mesh: Unknown primitive shape {}", static_cast<int>(desc.Shape));
            return {};
        }
    }

    Mesh Mesh::CreateTexturedPrimitive(RenderDevice& device, const PrimitiveDesc& desc)
    {
        switch (desc.Shape)
        {
        case PrimitiveShape::Cube:
            return CreateTexturedCube(device, desc.Size);
        default:
            Log::Error(LogRenderer, "Mesh: Unknown primitive shape {}", static_cast<int>(desc.Shape));
            return {};
        }
    }

} // namespace Wayfinder
