#include "assets/MeshFormat.h"
#include "rendering/backend/VertexFormats.h"

#include <doctest/doctest.h>

#include <cstring>
#include <vector>

namespace Wayfinder::Tests
{
    namespace
    {
        /// Helper: build a minimal valid ParsedMeshFile with the given submesh count.
        ParsedMeshFile MakeTestMeshFile(uint32_t submeshCount)
        {
            ParsedMeshFile src{};
            src.Header.Magic = MESH_FILE_MAGIC;
            src.Header.Version = MESH_FILE_VERSION;
            src.Header.SubmeshCount = submeshCount;
            src.Header.Bounds = AxisAlignedBounds{.Min = Float3{-1, -1, -1}, .Max = Float3{1, 1, 1}};

            for (uint32_t i = 0; i < submeshCount; ++i)
            {
                SubmeshCpuData sm{};
                sm.VertexFormat = MeshVertexFormat::PosNormalUVTangent;
                sm.IndexFormat = MeshIndexFormat::Uint16;
                sm.VertexCount = 3;
                sm.IndexCount = 3;
                sm.MaterialSlot = i;
                sm.Bounds = src.Header.Bounds;

                const std::array<VertexPosNormalUVTangent, 3> verts{};
                sm.VertexBytes.resize(sizeof(verts));
                std::memcpy(sm.VertexBytes.data(), verts.data(), sm.VertexBytes.size());

                const std::array<std::uint16_t, 3> idx{0, 1, 2};
                sm.IndexBytes.resize(sizeof(idx));
                std::memcpy(sm.IndexBytes.data(), idx.data(), sm.IndexBytes.size());

                src.Submeshes.push_back(std::move(sm));
            }

            src.SubmeshTable.assign(submeshCount, SubmeshTableEntry{});
            return src;
        }
    } // namespace

    TEST_CASE("MeshFormat write/read round-trip preserves vertex and index bytes")
    {
        auto src = MakeTestMeshFile(1);

        std::vector<std::byte> bytes;
        std::string err;
        REQUIRE(WriteMeshFileV1(src, bytes, err));
        CHECK(err.empty());

        ParsedMeshFile dst;
        const std::span<const std::byte> span(bytes.data(), bytes.size());
        REQUIRE(ParseMeshFile(span, dst, err));
        CHECK(err.empty());
        REQUIRE(dst.Submeshes.size() == 1);
        CHECK(dst.Submeshes.at(0).VertexBytes == src.Submeshes.at(0).VertexBytes);
        CHECK(dst.Submeshes.at(0).IndexBytes == src.Submeshes.at(0).IndexBytes);
    }

    TEST_CASE("MeshFormat multi-submesh round-trip preserves all submeshes")
    {
        constexpr uint32_t submeshCount = 3;
        auto src = MakeTestMeshFile(submeshCount);

        std::vector<std::byte> bytes;
        std::string err;
        REQUIRE(WriteMeshFileV1(src, bytes, err));

        ParsedMeshFile dst;
        REQUIRE(ParseMeshFile(std::span(bytes.data(), bytes.size()), dst, err));

        REQUIRE(dst.Header.SubmeshCount == submeshCount);
        REQUIRE(dst.Submeshes.size() == submeshCount);

        for (uint32_t i = 0; i < submeshCount; ++i)
        {
            CHECK(dst.Submeshes.at(i).VertexBytes == src.Submeshes.at(i).VertexBytes);
            CHECK(dst.Submeshes.at(i).IndexBytes == src.Submeshes.at(i).IndexBytes);
            CHECK(dst.Submeshes.at(i).MaterialSlot == i);
            CHECK(dst.Submeshes.at(i).VertexCount == 3);
            CHECK(dst.Submeshes.at(i).IndexCount == 3);
        }
    }

    TEST_CASE("ValidateMeshBinaryLayout rejects truncated file")
    {
        std::vector<std::byte> tiny(sizeof(MeshFileHeader) - 1);
        std::string err;
        CHECK_FALSE(ValidateMeshBinaryLayout(std::span(tiny.data(), tiny.size()), err));
        CHECK(!err.empty());
    }

    TEST_CASE("ValidateMeshBinaryLayout rejects bad magic")
    {
        auto src = MakeTestMeshFile(1);
        std::vector<std::byte> bytes;
        std::string err;
        REQUIRE(WriteMeshFileV1(src, bytes, err));

        // Corrupt magic bytes
        bytes[0] = std::byte{0xFF};
        bytes[1] = std::byte{0xFF};

        CHECK_FALSE(ValidateMeshBinaryLayout(std::span(bytes.data(), bytes.size()), err));
        CHECK(err.find("magic") != std::string::npos);
    }

    TEST_CASE("ValidateMeshBinaryLayout rejects bad version")
    {
        auto src = MakeTestMeshFile(1);
        std::vector<std::byte> bytes;
        std::string err;
        REQUIRE(WriteMeshFileV1(src, bytes, err));

        // Corrupt version (bytes 4-5 in the header)
        constexpr uint16_t badVersion = 99;
        std::memcpy(bytes.data() + offsetof(MeshFileHeader, Version), &badVersion, sizeof(badVersion));

        CHECK_FALSE(ValidateMeshBinaryLayout(std::span(bytes.data(), bytes.size()), err));
        CHECK(err.find("version") != std::string::npos);
    }

    TEST_CASE("ValidateMeshBinaryLayout detects overlapping blob regions")
    {
        // Build a valid 2-submesh file, then manually corrupt offsets to overlap
        auto src = MakeTestMeshFile(2);
        std::vector<std::byte> bytes;
        std::string err;
        REQUIRE(WriteMeshFileV1(src, bytes, err));

        // Read the first submesh table entry to find its vertex offset
        SubmeshTableEntry entry0{};
        const size_t entry0Offset = sizeof(MeshFileHeader);
        std::memcpy(&entry0, bytes.data() + entry0Offset, sizeof(SubmeshTableEntry));

        // Read the second submesh table entry
        SubmeshTableEntry entry1{};
        const size_t entry1Offset = sizeof(MeshFileHeader) + sizeof(SubmeshTableEntry);
        std::memcpy(&entry1, bytes.data() + entry1Offset, sizeof(SubmeshTableEntry));

        // Corrupt: make entry1's vertex data start inside entry0's vertex data
        entry1.VertexDataOffset = entry0.VertexDataOffset + 1;
        std::memcpy(bytes.data() + entry1Offset, &entry1, sizeof(SubmeshTableEntry));

        CHECK_FALSE(ValidateMeshBinaryLayout(std::span(bytes.data(), bytes.size()), err));
        CHECK(err.find("overlap") != std::string::npos);
    }

} // namespace Wayfinder::Tests
