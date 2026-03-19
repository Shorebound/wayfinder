#include "Mesh.h"
#include "../core/Log.h"

#include <array>

namespace Wayfinder
{
    bool Mesh::Create(RenderDevice& device,
                       const void* vertexData, uint32_t vertexDataSize, uint32_t vertexCount,
                       const void* indexData, uint32_t indexDataSize, uint32_t indexCount,
                       IndexElementSize indexElementSize)
    {
        if (!m_vertexBuffer.Create(device, BufferUsage::Vertex, vertexDataSize))
        {
            WAYFINDER_ERROR(LogRenderer, "Mesh: Failed to create vertex buffer");
            return false;
        }

        if (!m_indexBuffer.Create(device, BufferUsage::Index, indexDataSize))
        {
            WAYFINDER_ERROR(LogRenderer, "Mesh: Failed to create index buffer");
            m_vertexBuffer.Destroy();
            return false;
        }

        m_vertexBuffer.Upload(vertexData, vertexDataSize);
        m_indexBuffer.Upload(indexData, indexDataSize);

        m_vertexCount = vertexCount;
        m_indexCount = indexCount;
        m_indexElementSize = indexElementSize;

        WAYFINDER_INFO(LogRenderer, "Mesh: Created ({} verts, {} indices)", vertexCount, indexCount);
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
        device.BindVertexBuffer(m_vertexBuffer.GetHandle(), 0);
        device.BindIndexBuffer(m_indexBuffer.GetHandle(), m_indexElementSize);
    }

    void Mesh::Draw(RenderDevice& device, uint32_t instanceCount) const
    {
        device.DrawIndexed(m_indexCount, instanceCount);
    }

    // ── Built-in Primitives ──────────────────────────────────

    static Mesh CreateCube(RenderDevice& device, float size)
    {
        const float H = size * 0.5f;

        // Face normals
        constexpr Float3 nFront  = { 0.0f,  0.0f,  1.0f};
        constexpr Float3 nBack   = { 0.0f,  0.0f, -1.0f};
        constexpr Float3 nTop    = { 0.0f,  1.0f,  0.0f};
        constexpr Float3 nBottom = { 0.0f, -1.0f,  0.0f};
        constexpr Float3 nRight  = { 1.0f,  0.0f,  0.0f};
        constexpr Float3 nLeft   = {-1.0f,  0.0f,  0.0f};

        // Per-face vertex colors
        constexpr Float3 cFront  = {0.9f, 0.2f, 0.2f};
        constexpr Float3 cBack   = {0.2f, 0.8f, 0.2f};
        constexpr Float3 cTop    = {0.2f, 0.4f, 0.9f};
        constexpr Float3 cBottom = {0.9f, 0.9f, 0.2f};
        constexpr Float3 cRight  = {0.9f, 0.2f, 0.9f};
        constexpr Float3 cLeft   = {0.2f, 0.9f, 0.9f};

        std::array<VertexPosNormalColor, 24> vertices = {{
            // Front (+Z)
            {{-H, -H,  H}, nFront, cFront}, {{ H, -H,  H}, nFront, cFront}, {{ H,  H,  H}, nFront, cFront}, {{-H,  H,  H}, nFront, cFront},
            // Back (-Z)
            {{ H, -H, -H}, nBack, cBack}, {{-H, -H, -H}, nBack, cBack}, {{-H,  H, -H}, nBack, cBack}, {{ H,  H, -H}, nBack, cBack},
            // Top (+Y)
            {{-H,  H,  H}, nTop, cTop}, {{ H,  H,  H}, nTop, cTop}, {{ H,  H, -H}, nTop, cTop}, {{-H,  H, -H}, nTop, cTop},
            // Bottom (-Y)
            {{-H, -H, -H}, nBottom, cBottom}, {{ H, -H, -H}, nBottom, cBottom}, {{ H, -H,  H}, nBottom, cBottom}, {{-H, -H,  H}, nBottom, cBottom},
            // Right (+X)
            {{ H, -H,  H}, nRight, cRight}, {{ H, -H, -H}, nRight, cRight}, {{ H,  H, -H}, nRight, cRight}, {{ H,  H,  H}, nRight, cRight},
            // Left (-X)
            {{-H, -H, -H}, nLeft, cLeft}, {{-H, -H,  H}, nLeft, cLeft}, {{-H,  H,  H}, nLeft, cLeft}, {{-H,  H, -H}, nLeft, cLeft},
        }};

        std::array<uint16_t, 36> indices = {{
            0,  1,  2,  0,  2,  3,
            4,  5,  6,  4,  6,  7,
            8,  9,  10, 8,  10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23,
        }};

        Mesh mesh;
        if (!mesh.Create(device,
                          vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(VertexPosNormalColor)), static_cast<uint32_t>(vertices.size()),
                          indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t)), static_cast<uint32_t>(indices.size()),
                          IndexElementSize::Uint16))
        {
            WAYFINDER_ERROR(LogRenderer, "Mesh: Failed to create primitive cube");
        }

        return mesh;
    }

    Mesh Mesh::CreatePrimitive(RenderDevice& device, const PrimitiveDesc& desc)
    {
        switch (desc.Shape)
        {
        case PrimitiveShape::Cube: return CreateCube(device, desc.Size);
        default:
            WAYFINDER_ERROR(LogRenderer, "Mesh: Unknown primitive shape {}", static_cast<int>(desc.Shape));
            return {};
        }
    }

} // namespace Wayfinder
