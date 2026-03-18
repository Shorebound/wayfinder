#pragma once

#include "GPUBuffer.h"
#include "VertexFormats.h"

#include <vector>

namespace Wayfinder
{
    // GPU-backed mesh: owns vertex and index buffers on the device.
    // Distinct from the authored MeshComponent — this is the GPU-side representation.
    class WAYFINDER_API Mesh
    {
    public:
        Mesh() = default;
        ~Mesh() = default;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&&) noexcept = default;
        Mesh& operator=(Mesh&&) noexcept = default;

        bool Create(RenderDevice& device,
                     const void* vertexData, uint32_t vertexDataSize, uint32_t vertexCount,
                     const void* indexData, uint32_t indexDataSize, uint32_t indexCount,
                     IndexElementSize indexElementSize = IndexElementSize::Uint16);

        void Destroy();

        void Bind(RenderDevice& device) const;
        void Draw(RenderDevice& device, uint32_t instanceCount = 1) const;

        bool IsValid() const { return m_vertexBuffer.IsValid() && m_indexBuffer.IsValid(); }
        uint32_t GetIndexCount() const { return m_indexCount; }
        uint32_t GetVertexCount() const { return m_vertexCount; }

        // ── Built-in Primitives ──────────────────────────────

        static Mesh CreateUnitCube(RenderDevice& device);

    private:
        GPUBuffer m_vertexBuffer;
        GPUBuffer m_indexBuffer;
        uint32_t m_indexCount = 0;
        uint32_t m_vertexCount = 0;
        IndexElementSize m_indexElementSize = IndexElementSize::Uint16;
    };

} // namespace Wayfinder
