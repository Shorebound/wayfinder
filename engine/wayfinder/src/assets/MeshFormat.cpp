#include "MeshFormat.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace Wayfinder
{
    namespace
    {
        constexpr uint32_t K_STRIDE_POS_NORMAL_UV = 32u;         // 3+3+2 floats
        constexpr uint32_t K_STRIDE_POS_NORMAL_UV_TANGENT = 48u; // 3+3+2+4 floats

        template<typename T>
        bool ReadPod(std::span<const std::byte>& remaining, T& out)
        {
            if (remaining.size() < sizeof(T))
            {
                return false;
            }
            std::memcpy(&out, remaining.data(), sizeof(T));
            remaining = remaining.subspan(sizeof(T));
            return true;
        }
    } // namespace

    uint32_t GetVertexStride(const MeshVertexFormat format)
    {
        switch (format)
        {
        case MeshVertexFormat::PosNormalUV:
            return K_STRIDE_POS_NORMAL_UV;
        case MeshVertexFormat::PosNormalUVTangent:
            return K_STRIDE_POS_NORMAL_UV_TANGENT;
        }
        return K_STRIDE_POS_NORMAL_UV;
    }

    bool ParseMeshFile(const std::span<const std::byte> fileBytes, ParsedMeshFile& out, std::string& error)
    {
        out = {};
        std::string layoutError;
        if (!ValidateMeshBinaryLayout(fileBytes, layoutError))
        {
            error = std::move(layoutError);
            return false;
        }

        std::span<const std::byte> cursor = fileBytes;
        MeshFileHeader header{};
        if (!ReadPod(cursor, header))
        {
            error = "Mesh file: truncated header";
            return false;
        }

        out.Header = header;

        if (header.Magic != MESH_FILE_MAGIC)
        {
            error = "Mesh file: invalid magic";
            return false;
        }

        if (header.Version != MESH_FILE_VERSION)
        {
            error = "Mesh file: unsupported version";
            return false;
        }

        if (header.SubmeshCount == 0)
        {
            error = "Mesh file: submesh_count is zero";
            return false;
        }

        out.SubmeshTable.resize(header.SubmeshCount);
        for (uint32_t i = 0; i < header.SubmeshCount; ++i)
        {
            if (!ReadPod(cursor, out.SubmeshTable[i]))
            {
                error = "Mesh file: truncated submesh table";
                return false;
            }
        }

        out.Submeshes.resize(header.SubmeshCount);

        for (uint32_t i = 0; i < header.SubmeshCount; ++i)
        {
            const SubmeshTableEntry& entry = out.SubmeshTable[i];
            SubmeshCpuData& sm = out.Submeshes[i];

            sm.VertexFormat = entry.VertexFormat;
            sm.IndexFormat = entry.IndexFormat;
            sm.VertexCount = entry.VertexCount;
            sm.IndexCount = entry.IndexCount;
            sm.MaterialSlot = entry.MaterialSlot;
            sm.Bounds = entry.Bounds;

            const uint32_t stride = GetVertexStride(entry.VertexFormat);
            if (entry.VertexDataSize != entry.VertexCount * stride)
            {
                error = "Mesh file: vertex data size mismatch for submesh " + std::to_string(i);
                return false;
            }

            const size_t indexElementBytes = entry.IndexFormat == MeshIndexFormat::Uint32 ? 4u : 2u;
            if (entry.IndexDataSize != entry.IndexCount * indexElementBytes)
            {
                error = "Mesh file: index data size mismatch for submesh " + std::to_string(i);
                return false;
            }

            if (entry.VertexDataOffset > fileBytes.size() || entry.VertexDataSize > fileBytes.size() - entry.VertexDataOffset)
            {
                error = "Mesh file: vertex blob out of range for submesh " + std::to_string(i);
                return false;
            }

            if (entry.IndexDataOffset > fileBytes.size() || entry.IndexDataSize > fileBytes.size() - entry.IndexDataOffset)
            {
                error = "Mesh file: index blob out of range for submesh " + std::to_string(i);
                return false;
            }

            sm.VertexBytes.resize(entry.VertexDataSize);
            std::memcpy(sm.VertexBytes.data(), fileBytes.data() + entry.VertexDataOffset, entry.VertexDataSize);

            sm.IndexBytes.resize(entry.IndexDataSize);
            std::memcpy(sm.IndexBytes.data(), fileBytes.data() + entry.IndexDataOffset, entry.IndexDataSize);
        }

        return true;
    }

    bool WriteMeshFileV1(const ParsedMeshFile& data, std::vector<std::byte>& outBytes, std::string& error)
    {
        if (data.Header.SubmeshCount == 0)
        {
            error = "WriteMeshFileV1: no submeshes";
            return false;
        }

        if (data.SubmeshTable.size() != data.Header.SubmeshCount || data.Submeshes.size() != data.Header.SubmeshCount)
        {
            error = "WriteMeshFileV1: table/submesh count mismatch";
            return false;
        }

        uint32_t vertexOffset = static_cast<uint32_t>(sizeof(MeshFileHeader) + static_cast<size_t>(data.Header.SubmeshCount) * sizeof(SubmeshTableEntry));
        uint32_t indexOffset = vertexOffset;

        for (uint32_t i = 0; i < data.Header.SubmeshCount; ++i)
        {
            indexOffset += static_cast<uint32_t>(data.Submeshes[i].VertexBytes.size());
        }

        std::vector<SubmeshTableEntry> table = data.SubmeshTable;

        uint32_t runningVertex = vertexOffset;
        uint32_t runningIndex = indexOffset;

        for (uint32_t i = 0; i < data.Header.SubmeshCount; ++i)
        {
            const SubmeshCpuData& sm = data.Submeshes[i];
            const uint32_t stride = GetVertexStride(sm.VertexFormat);

            if (sm.VertexBytes.size() != static_cast<size_t>(sm.VertexCount) * stride)
            {
                error = "WriteMeshFileV1: vertex byte count mismatch for submesh " + std::to_string(i);
                return false;
            }

            const size_t idxSize = sm.IndexFormat == MeshIndexFormat::Uint32 ? 4u : 2u;
            if (sm.IndexBytes.size() != static_cast<size_t>(sm.IndexCount) * idxSize)
            {
                error = "WriteMeshFileV1: index byte count mismatch for submesh " + std::to_string(i);
                return false;
            }

            table[i].VertexFormat = sm.VertexFormat;
            table[i].IndexFormat = sm.IndexFormat;
            table[i].Padding = 0;
            table[i].VertexCount = sm.VertexCount;
            table[i].IndexCount = sm.IndexCount;
            table[i].VertexDataOffset = runningVertex;
            table[i].VertexDataSize = static_cast<uint32_t>(sm.VertexBytes.size());
            table[i].IndexDataOffset = runningIndex;
            table[i].IndexDataSize = static_cast<uint32_t>(sm.IndexBytes.size());
            table[i].Bounds = sm.Bounds;
            table[i].MaterialSlot = sm.MaterialSlot;

            runningVertex += table[i].VertexDataSize;
            runningIndex += table[i].IndexDataSize;
        }

        outBytes.clear();
        outBytes.resize(static_cast<size_t>(runningIndex));

        const MeshFileHeader header = data.Header;
        std::memcpy(outBytes.data(), &header, sizeof(MeshFileHeader));

        std::byte* dst = outBytes.data() + sizeof(MeshFileHeader);
        for (uint32_t i = 0; i < data.Header.SubmeshCount; ++i)
        {
            std::memcpy(dst, &table[i], sizeof(SubmeshTableEntry));
            dst += sizeof(SubmeshTableEntry);
        }

        for (uint32_t i = 0; i < data.Header.SubmeshCount; ++i)
        {
            const auto& sm = data.Submeshes[i];
            std::memcpy(outBytes.data() + table[i].VertexDataOffset, sm.VertexBytes.data(), sm.VertexBytes.size());
        }

        for (uint32_t i = 0; i < data.Header.SubmeshCount; ++i)
        {
            const auto& sm = data.Submeshes[i];
            std::memcpy(outBytes.data() + table[i].IndexDataOffset, sm.IndexBytes.data(), sm.IndexBytes.size());
        }

        return true;
    }

    bool ValidateMeshBinaryLayout(const std::span<const std::byte> fileBytes, std::string& error)
    {
        if (fileBytes.size() < sizeof(MeshFileHeader))
        {
            error = "Mesh file: truncated (header)";
            return false;
        }

        MeshFileHeader header{};
        std::memcpy(&header, fileBytes.data(), sizeof(MeshFileHeader));

        if (header.Magic != MESH_FILE_MAGIC)
        {
            error = "Mesh file: invalid magic";
            return false;
        }

        if (header.Version != MESH_FILE_VERSION)
        {
            error = "Mesh file: unsupported version";
            return false;
        }

        if (header.SubmeshCount == 0)
        {
            error = "Mesh file: submesh_count is zero";
            return false;
        }

        const size_t tableBytes = sizeof(MeshFileHeader) + static_cast<size_t>(header.SubmeshCount) * sizeof(SubmeshTableEntry);
        if (fileBytes.size() < tableBytes)
        {
            error = "Mesh file: truncated (submesh table)";
            return false;
        }

        for (uint32_t i = 0; i < header.SubmeshCount; ++i)
        {
            SubmeshTableEntry entry{};
            const size_t offset = sizeof(MeshFileHeader) + static_cast<size_t>(i) * sizeof(SubmeshTableEntry);
            std::memcpy(&entry, fileBytes.data() + offset, sizeof(SubmeshTableEntry));

            if (static_cast<uint32_t>(entry.VertexFormat) > 1u)
            {
                error = "Mesh file: invalid vertex_format in submesh " + std::to_string(i);
                return false;
            }

            if (static_cast<uint32_t>(entry.IndexFormat) > 1u)
            {
                error = "Mesh file: invalid index_format in submesh " + std::to_string(i);
                return false;
            }

            const uint32_t stride = GetVertexStride(entry.VertexFormat);
            if (entry.VertexCount > std::numeric_limits<uint32_t>::max() / std::max<uint32_t>(stride, 1u))
            {
                error = "Mesh file: vertex count overflow";
                return false;
            }

            if (entry.VertexDataSize != entry.VertexCount * stride)
            {
                error = "Mesh file: vertex data size mismatch for submesh " + std::to_string(i);
                return false;
            }

            const size_t indexElementBytes = entry.IndexFormat == MeshIndexFormat::Uint32 ? 4u : 2u;
            if (entry.IndexDataSize != entry.IndexCount * indexElementBytes)
            {
                error = "Mesh file: index data size mismatch for submesh " + std::to_string(i);
                return false;
            }

            if (entry.VertexDataOffset > fileBytes.size() || entry.VertexDataSize > fileBytes.size() - entry.VertexDataOffset)
            {
                error = "Mesh file: vertex blob out of range for submesh " + std::to_string(i);
                return false;
            }

            if (entry.IndexDataOffset > fileBytes.size() || entry.IndexDataSize > fileBytes.size() - entry.IndexDataOffset)
            {
                error = "Mesh file: index blob out of range for submesh " + std::to_string(i);
                return false;
            }
        }

        return true;
    }

} // namespace Wayfinder
