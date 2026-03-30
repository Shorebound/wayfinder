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

            constexpr Float3 normalFront = {0.0f, 0.0f, 1.0f};
            constexpr Float3 normalBack = {0.0f, 0.0f, -1.0f};
            constexpr Float3 normalTop = {0.0f, 1.0f, 0.0f};
            constexpr Float3 normalBottom = {0.0f, -1.0f, 0.0f};
            constexpr Float3 normalRight = {1.0f, 0.0f, 0.0f};
            constexpr Float3 normalLeft = {-1.0f, 0.0f, 0.0f};

            constexpr Float3 colourFront = {0.9f, 0.2f, 0.2f};
            constexpr Float3 colourBack = {0.2f, 0.8f, 0.2f};
            constexpr Float3 colourTop = {0.2f, 0.4f, 0.9f};
            constexpr Float3 colourBottom = {0.9f, 0.9f, 0.2f};
            constexpr Float3 colourRight = {0.9f, 0.2f, 0.9f};
            constexpr Float3 colourLeft = {0.2f, 0.9f, 0.9f};

            const std::array<VertexPositionNormalColour, 24> vertices = {{
                {.Position = {-h, -h, h}, .Normal = normalFront, .Colour = colourFront},
                {.Position = {h, -h, h}, .Normal = normalFront, .Colour = colourFront},
                {.Position = {h, h, h}, .Normal = normalFront, .Colour = colourFront},
                {.Position = {-h, h, h}, .Normal = normalFront, .Colour = colourFront},
                {.Position = {h, -h, -h}, .Normal = normalBack, .Colour = colourBack},
                {.Position = {-h, -h, -h}, .Normal = normalBack, .Colour = colourBack},
                {.Position = {-h, h, -h}, .Normal = normalBack, .Colour = colourBack},
                {.Position = {h, h, -h}, .Normal = normalBack, .Colour = colourBack},
                {.Position = {-h, h, h}, .Normal = normalTop, .Colour = colourTop},
                {.Position = {h, h, h}, .Normal = normalTop, .Colour = colourTop},
                {.Position = {h, h, -h}, .Normal = normalTop, .Colour = colourTop},
                {.Position = {-h, h, -h}, .Normal = normalTop, .Colour = colourTop},
                {.Position = {-h, -h, -h}, .Normal = normalBottom, .Colour = colourBottom},
                {.Position = {h, -h, -h}, .Normal = normalBottom, .Colour = colourBottom},
                {.Position = {h, -h, h}, .Normal = normalBottom, .Colour = colourBottom},
                {.Position = {-h, -h, h}, .Normal = normalBottom, .Colour = colourBottom},
                {.Position = {h, -h, h}, .Normal = normalRight, .Colour = colourRight},
                {.Position = {h, -h, -h}, .Normal = normalRight, .Colour = colourRight},
                {.Position = {h, h, -h}, .Normal = normalRight, .Colour = colourRight},
                {.Position = {h, h, h}, .Normal = normalRight, .Colour = colourRight},
                {.Position = {-h, -h, -h}, .Normal = normalLeft, .Colour = colourLeft},
                {.Position = {-h, -h, h}, .Normal = normalLeft, .Colour = colourLeft},
                {.Position = {-h, h, h}, .Normal = normalLeft, .Colour = colourLeft},
                {.Position = {-h, h, -h}, .Normal = normalLeft, .Colour = colourLeft},
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
                WAYFINDER_ERROR(LogRenderer, "Mesh: Failed to create primitive cube");
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

            constexpr Float3 normalFront = {0.0f, 0.0f, 1.0f};
            constexpr Float3 normalBack = {0.0f, 0.0f, -1.0f};
            constexpr Float3 normalTop = {0.0f, 1.0f, 0.0f};
            constexpr Float3 normalBottom = {0.0f, -1.0f, 0.0f};
            constexpr Float3 normalRight = {1.0f, 0.0f, 0.0f};
            constexpr Float3 normalLeft = {-1.0f, 0.0f, 0.0f};

            constexpr Float2 UV00 = {0.0f, 1.0f};
            constexpr Float2 UV10 = {1.0f, 1.0f};
            constexpr Float2 UV11 = {1.0f, 0.0f};
            constexpr Float2 UV01 = {0.0f, 0.0f};

            const std::array<VertexPositionNormalUVTangent, 24> vertices = {{
                {.Position = {-h, -h, h}, .Normal = normalFront, .UV = UV00, .Tangent = ApproxTangentForNormal(normalFront)},
                {.Position = {h, -h, h}, .Normal = normalFront, .UV = UV10, .Tangent = ApproxTangentForNormal(normalFront)},
                {.Position = {h, h, h}, .Normal = normalFront, .UV = UV11, .Tangent = ApproxTangentForNormal(normalFront)},
                {.Position = {-h, h, h}, .Normal = normalFront, .UV = UV01, .Tangent = ApproxTangentForNormal(normalFront)},
                {.Position = {h, -h, -h}, .Normal = normalBack, .UV = UV00, .Tangent = ApproxTangentForNormal(normalBack)},
                {.Position = {-h, -h, -h}, .Normal = normalBack, .UV = UV10, .Tangent = ApproxTangentForNormal(normalBack)},
                {.Position = {-h, h, -h}, .Normal = normalBack, .UV = UV11, .Tangent = ApproxTangentForNormal(normalBack)},
                {.Position = {h, h, -h}, .Normal = normalBack, .UV = UV01, .Tangent = ApproxTangentForNormal(normalBack)},
                {.Position = {-h, h, h}, .Normal = normalTop, .UV = UV00, .Tangent = ApproxTangentForNormal(normalTop)},
                {.Position = {h, h, h}, .Normal = normalTop, .UV = UV10, .Tangent = ApproxTangentForNormal(normalTop)},
                {.Position = {h, h, -h}, .Normal = normalTop, .UV = UV11, .Tangent = ApproxTangentForNormal(normalTop)},
                {.Position = {-h, h, -h}, .Normal = normalTop, .UV = UV01, .Tangent = ApproxTangentForNormal(normalTop)},
                {.Position = {-h, -h, -h}, .Normal = normalBottom, .UV = UV00, .Tangent = ApproxTangentForNormal(normalBottom)},
                {.Position = {h, -h, -h}, .Normal = normalBottom, .UV = UV10, .Tangent = ApproxTangentForNormal(normalBottom)},
                {.Position = {h, -h, h}, .Normal = normalBottom, .UV = UV11, .Tangent = ApproxTangentForNormal(normalBottom)},
                {.Position = {-h, -h, h}, .Normal = normalBottom, .UV = UV01, .Tangent = ApproxTangentForNormal(normalBottom)},
                {.Position = {h, -h, h}, .Normal = normalRight, .UV = UV00, .Tangent = ApproxTangentForNormal(normalRight)},
                {.Position = {h, -h, -h}, .Normal = normalRight, .UV = UV10, .Tangent = ApproxTangentForNormal(normalRight)},
                {.Position = {h, h, -h}, .Normal = normalRight, .UV = UV11, .Tangent = ApproxTangentForNormal(normalRight)},
                {.Position = {h, h, h}, .Normal = normalRight, .UV = UV01, .Tangent = ApproxTangentForNormal(normalRight)},
                {.Position = {-h, -h, -h}, .Normal = normalLeft, .UV = UV00, .Tangent = ApproxTangentForNormal(normalLeft)},
                {.Position = {-h, -h, h}, .Normal = normalLeft, .UV = UV10, .Tangent = ApproxTangentForNormal(normalLeft)},
                {.Position = {-h, h, h}, .Normal = normalLeft, .UV = UV11, .Tangent = ApproxTangentForNormal(normalLeft)},
                {.Position = {-h, h, -h}, .Normal = normalLeft, .UV = UV01, .Tangent = ApproxTangentForNormal(normalLeft)},
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
                WAYFINDER_ERROR(LogRenderer, "Mesh: Failed to create textured primitive cube");
                return {};
            }

            return mesh;
        }

    } // namespace

    bool Mesh::Create(RenderDevice& device, const MeshCreateDesc& desc)
    {
        if (!m_vertexBuffer.Create(device, BufferUsage::Vertex, desc.VertexDataSize))
        {
            WAYFINDER_ERROR(LogRenderer, "Mesh: Failed to create vertex buffer");
            return false;
        }

        if (!m_indexBuffer.Create(device, BufferUsage::Index, desc.IndexDataSize))
        {
            WAYFINDER_ERROR(LogRenderer, "Mesh: Failed to create index buffer");
            m_vertexBuffer.Destroy();
            return false;
        }

        m_vertexBuffer.Upload(desc.VertexData, desc.VertexDataSize);
        m_indexBuffer.Upload(desc.IndexData, desc.IndexDataSize);

        m_vertexCount = desc.VertexCount;
        m_indexCount = desc.IndexCount;
        m_indexElementSize = desc.IndexElementType;

        WAYFINDER_INFO(LogRenderer, "Mesh: Created ({} verts, {} indices)", desc.VertexCount, desc.IndexCount);
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
            WAYFINDER_ERROR(LogRenderer, "Mesh: Unknown primitive shape {}", static_cast<int>(desc.Shape));
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
            WAYFINDER_ERROR(LogRenderer, "Mesh: Unknown primitive shape {}", static_cast<int>(desc.Shape));
            return {};
        }
    }

} // namespace Wayfinder
