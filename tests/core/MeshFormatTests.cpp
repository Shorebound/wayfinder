#include "assets/MeshFormat.h"
#include "rendering/backend/VertexFormats.h"

#include <doctest/doctest.h>

#include <cstring>
#include <vector>

namespace Wayfinder::Tests
{
    TEST_CASE("MeshFormat write/read round-trip preserves vertex and index bytes")
    {
        ParsedMeshFile src{};
        src.Header.Magic = MESH_FILE_MAGIC;
        src.Header.Version = MESH_FILE_VERSION;
        src.Header.SubmeshCount = 1;
        src.Header.Bounds = AxisAlignedBounds{.Min = Float3{-1, -1, 0}, .Max = Float3{1, 1, 0}};

        SubmeshCpuData sm{};
        sm.VertexFormat = MeshVertexFormat::PosNormalUVTangent;
        sm.IndexFormat = MeshIndexFormat::Uint16;
        sm.VertexCount = 3;
        sm.IndexCount = 3;
        sm.MaterialSlot = 0;
        sm.Bounds = src.Header.Bounds;

        const std::array<VertexPosNormalUVTangent, 3> verts{};
        sm.VertexBytes.resize(sizeof(verts));
        std::memcpy(sm.VertexBytes.data(), verts.data(), sm.VertexBytes.size());

        const std::array<std::uint16_t, 3> idx{0, 1, 2};
        sm.IndexBytes.resize(sizeof(idx));
        std::memcpy(sm.IndexBytes.data(), idx.data(), sm.IndexBytes.size());

        src.Submeshes.push_back(std::move(sm));
        src.SubmeshTable.assign(1, SubmeshTableEntry{});

        std::vector<std::byte> bytes;
        std::string err;
        REQUIRE(WriteMeshFileV1(src, bytes, err));
        CHECK(err.empty());

        ParsedMeshFile dst;
        std::span<const std::byte> span(bytes.data(), bytes.size());
        REQUIRE(ParseMeshFile(span, dst, err));
        CHECK(err.empty());
        REQUIRE(dst.Submeshes.size() == 1);
        CHECK(dst.Submeshes[0].VertexBytes == src.Submeshes[0].VertexBytes);
        CHECK(dst.Submeshes[0].IndexBytes == src.Submeshes[0].IndexBytes);
    }

    TEST_CASE("ValidateMeshBinaryLayout rejects truncated file")
    {
        std::vector<std::byte> tiny(sizeof(MeshFileHeader) - 1);
        std::string err;
        CHECK_FALSE(ValidateMeshBinaryLayout(std::span(tiny.data(), tiny.size()), err));
        CHECK(!err.empty());
    }

} // namespace Wayfinder::Tests
