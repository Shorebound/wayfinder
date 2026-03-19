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

    Mesh Mesh::CreateUnitCube(RenderDevice& device)
    {
        // 24 vertices (4 per face), VertexPosColor layout
        // Each face has a distinct color for visual debugging.
        constexpr float P = 0.5f;
        constexpr float N = -0.5f;

        // Colors per face (RGB)
        constexpr Float3 cFront  = {0.9f, 0.2f, 0.2f}; // Red
        constexpr Float3 cBack   = {0.2f, 0.8f, 0.2f}; // Green
        constexpr Float3 cTop    = {0.2f, 0.4f, 0.9f}; // Blue
        constexpr Float3 cBottom = {0.9f, 0.9f, 0.2f}; // Yellow
        constexpr Float3 cRight  = {0.9f, 0.2f, 0.9f}; // Magenta
        constexpr Float3 cLeft   = {0.2f, 0.9f, 0.9f}; // Cyan

        std::array<VertexPosColor, 24> vertices = {{
            // Front face (z = +0.5)
            {{N, N, P}, cFront}, {{P, N, P}, cFront}, {{P, P, P}, cFront}, {{N, P, P}, cFront},
            // Back face (z = -0.5)
            {{P, N, N}, cBack}, {{N, N, N}, cBack}, {{N, P, N}, cBack}, {{P, P, N}, cBack},
            // Top face (y = +0.5)
            {{N, P, P}, cTop}, {{P, P, P}, cTop}, {{P, P, N}, cTop}, {{N, P, N}, cTop},
            // Bottom face (y = -0.5)
            {{N, N, N}, cBottom}, {{P, N, N}, cBottom}, {{P, N, P}, cBottom}, {{N, N, P}, cBottom},
            // Right face (x = +0.5)
            {{P, N, P}, cRight}, {{P, N, N}, cRight}, {{P, P, N}, cRight}, {{P, P, P}, cRight},
            // Left face (x = -0.5)
            {{N, N, N}, cLeft}, {{N, N, P}, cLeft}, {{N, P, P}, cLeft}, {{N, P, N}, cLeft},
        }};

        std::array<uint16_t, 36> indices = {{
            0,  1,  2,  0,  2,  3,   // Front
            4,  5,  6,  4,  6,  7,   // Back
            8,  9,  10, 8,  10, 11,  // Top
            12, 13, 14, 12, 14, 15,  // Bottom
            16, 17, 18, 16, 18, 19,  // Right
            20, 21, 22, 20, 22, 23,  // Left
        }};

        Mesh mesh;
        if (!mesh.Create(device,
                          vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(VertexPosColor)), static_cast<uint32_t>(vertices.size()),
                          indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t)), static_cast<uint32_t>(indices.size()),
                          IndexElementSize::Uint16))
        {
            WAYFINDER_ERROR(LogRenderer, "Mesh: Failed to create unit cube");
        }

        return mesh;
    }

    Mesh Mesh::CreateUnitCubeWithNormals(RenderDevice& device)
    {
        constexpr float P = 0.5f;
        constexpr float N = -0.5f;

        // Face normals
        constexpr Float3 nFront  = { 0.0f,  0.0f,  1.0f};
        constexpr Float3 nBack   = { 0.0f,  0.0f, -1.0f};
        constexpr Float3 nTop    = { 0.0f,  1.0f,  0.0f};
        constexpr Float3 nBottom = { 0.0f, -1.0f,  0.0f};
        constexpr Float3 nRight  = { 1.0f,  0.0f,  0.0f};
        constexpr Float3 nLeft   = {-1.0f,  0.0f,  0.0f};

        // Colors per face (same as unlit cube for visual consistency)
        constexpr Float3 cFront  = {0.9f, 0.2f, 0.2f};
        constexpr Float3 cBack   = {0.2f, 0.8f, 0.2f};
        constexpr Float3 cTop    = {0.2f, 0.4f, 0.9f};
        constexpr Float3 cBottom = {0.9f, 0.9f, 0.2f};
        constexpr Float3 cRight  = {0.9f, 0.2f, 0.9f};
        constexpr Float3 cLeft   = {0.2f, 0.9f, 0.9f};

        std::array<VertexPosNormalColor, 24> vertices = {{
            // Front face (z = +0.5)
            {{N, N, P}, nFront, cFront}, {{P, N, P}, nFront, cFront}, {{P, P, P}, nFront, cFront}, {{N, P, P}, nFront, cFront},
            // Back face (z = -0.5)
            {{P, N, N}, nBack, cBack}, {{N, N, N}, nBack, cBack}, {{N, P, N}, nBack, cBack}, {{P, P, N}, nBack, cBack},
            // Top face (y = +0.5)
            {{N, P, P}, nTop, cTop}, {{P, P, P}, nTop, cTop}, {{P, P, N}, nTop, cTop}, {{N, P, N}, nTop, cTop},
            // Bottom face (y = -0.5)
            {{N, N, N}, nBottom, cBottom}, {{P, N, N}, nBottom, cBottom}, {{P, N, P}, nBottom, cBottom}, {{N, N, P}, nBottom, cBottom},
            // Right face (x = +0.5)
            {{P, N, P}, nRight, cRight}, {{P, N, N}, nRight, cRight}, {{P, P, N}, nRight, cRight}, {{P, P, P}, nRight, cRight},
            // Left face (x = -0.5)
            {{N, N, N}, nLeft, cLeft}, {{N, N, P}, nLeft, cLeft}, {{N, P, P}, nLeft, cLeft}, {{N, P, N}, nLeft, cLeft},
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
            WAYFINDER_ERROR(LogRenderer, "Mesh: Failed to create unit cube with normals");
        }

        return mesh;
    }

} // namespace Wayfinder
