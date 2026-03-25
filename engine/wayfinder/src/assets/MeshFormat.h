#pragma once

#include "core/Types.h"
#include "maths/Bounds.h"
#include "wayfinder_exports.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Wayfinder
{
    /** @brief Magic bytes for .wfmesh files ("WFMH" little-endian). */
    inline constexpr uint32_t MESH_FILE_MAGIC = 0x484D4657u;

    inline constexpr uint16_t MESH_FILE_VERSION = 1;

    /**
     * @brief Vertex layout stored in the mesh binary (single stream per submesh).
     */
    enum class MeshVertexFormat : uint8_t
    {
        PosNormalUV = 0,
        PosNormalUVTangent = 1,
    };

    /**
     * @brief Index element width in the mesh binary.
     */
    enum class MeshIndexFormat : uint8_t
    {
        Uint16 = 0,
        Uint32 = 1,
    };

#pragma pack(push, 1)
    struct MeshFileHeader
    {
        uint32_t Magic = MESH_FILE_MAGIC;
        uint16_t Version = MESH_FILE_VERSION;
        uint16_t Flags = 0;
        uint32_t SubmeshCount = 0;
        AxisAlignedBounds Bounds{};
    };

    struct SubmeshTableEntry
    {
        MeshVertexFormat VertexFormat = MeshVertexFormat::PosNormalUV;
        MeshIndexFormat IndexFormat = MeshIndexFormat::Uint16;
        uint16_t Padding = 0;
        uint32_t VertexCount = 0;
        uint32_t IndexCount = 0;
        uint32_t VertexDataOffset = 0;
        uint32_t VertexDataSize = 0;
        uint32_t IndexDataOffset = 0;
        uint32_t IndexDataSize = 0;
        AxisAlignedBounds Bounds{};
        uint32_t MaterialSlot = 0;
    };
#pragma pack(pop)

    struct SubmeshCpuData
    {
        MeshVertexFormat VertexFormat = MeshVertexFormat::PosNormalUV;
        MeshIndexFormat IndexFormat = MeshIndexFormat::Uint16;
        uint32_t VertexCount = 0;
        uint32_t IndexCount = 0;
        uint32_t MaterialSlot = 0;
        AxisAlignedBounds Bounds{};
        std::vector<std::byte> VertexBytes;
        std::vector<std::byte> IndexBytes;
    };

    struct ParsedMeshFile
    {
        MeshFileHeader Header{};
        std::vector<SubmeshTableEntry> SubmeshTable;
        std::vector<SubmeshCpuData> Submeshes;
    };

    /** @brief Return byte size of one vertex for the given format. */
    WAYFINDER_API uint32_t GetVertexStride(MeshVertexFormat format);

    /** @brief Parse a .wfmesh file from memory. */
    WAYFINDER_API bool ParseMeshFile(std::span<const std::byte> fileBytes, ParsedMeshFile& out, std::string& error);

    /** @brief Serialise a mesh file (v1) — used by tests and the import tool. */
    WAYFINDER_API bool WriteMeshFileV1(const ParsedMeshFile& data, std::vector<std::byte>& outBytes, std::string& error);

    /** @brief Validate raw bytes are a plausible v1 mesh file (header + table + blob ranges). */
    WAYFINDER_API bool ValidateMeshBinaryLayout(std::span<const std::byte> fileBytes, std::string& error);

} // namespace Wayfinder
