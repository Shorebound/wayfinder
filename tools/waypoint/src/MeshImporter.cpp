#include "MeshImporter.h"

#include "assets/MeshFormat.h"
#include "core/Identifiers.h"
#include "core/Types.h"
#include "maths/Maths.h"
#include "rendering/backend/VertexFormats.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include <meshoptimizer.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace Wayfinder::Waypoint
{
    namespace
    {
        struct PrimitiveBuildInput
        {
            const fastgltf::Asset* Asset = nullptr;
            const fastgltf::Primitive* Primitive = nullptr;
        };

        struct BuiltSubmesh
        {
            SubmeshCpuData Mesh;
            AxisAlignedBounds Bounds;
        };

        Float4 ComputeFallbackTangent(const Float3& normal)
        {
            Float3 tangent = Maths::Normalize(Maths::Cross(Up, normal));
            if (std::abs(Maths::Dot(normal, Up)) > 0.99f)
            {
                tangent = Maths::Normalize(Maths::Cross(Right, normal));
            }

            return Float4{tangent, 1.0f};
        }

        Result<void> ReadVec3(const fastgltf::Asset& asset, const std::size_t accessorIndex, std::vector<Float3>& out)
        {
            const auto& acc = asset.accessors.at(accessorIndex);
            if (acc.type != fastgltf::AccessorType::Vec3)
            {
                return MakeError("Accessor is not VEC3");
            }

            out.resize(acc.count);
            fastgltf::copyFromAccessor<Float3>(asset, acc, out.data());
            return {};
        }

        Result<void> ReadVec2(const fastgltf::Asset& asset, const std::size_t accessorIndex, std::vector<Float2>& out)
        {
            const auto& acc = asset.accessors.at(accessorIndex);
            if (acc.type != fastgltf::AccessorType::Vec2)
            {
                return MakeError("Accessor is not VEC2");
            }

            out.resize(acc.count);
            fastgltf::copyFromAccessor<Float2>(asset, acc, out.data());
            return {};
        }

        Result<void> ReadVec4(const fastgltf::Asset& asset, const std::size_t accessorIndex, std::vector<Float4>& out)
        {
            const auto& acc = asset.accessors.at(accessorIndex);
            if (acc.type != fastgltf::AccessorType::Vec4)
            {
                return MakeError("Accessor is not VEC4");
            }

            out.resize(acc.count);
            fastgltf::copyFromAccessor<Float4>(asset, acc, out.data());
            return {};
        }

        Result<void> ReadIndices(const fastgltf::Asset& asset, const std::size_t accessorIndex, std::vector<std::uint32_t>& out)
        {
            const auto& acc = asset.accessors.at(accessorIndex);
            if (acc.type != fastgltf::AccessorType::Scalar)
            {
                return MakeError("Index accessor is not scalar");
            }

            out.resize(acc.count);

            if (acc.componentType == fastgltf::ComponentType::UnsignedShort)
            {
                std::vector<std::uint16_t> tmp(acc.count);
                fastgltf::copyFromAccessor<std::uint16_t>(asset, acc, tmp.data());
                for (std::size_t i = 0; i < acc.count; ++i)
                {
                    out.at(i) = tmp.at(i);
                }
            }
            else if (acc.componentType == fastgltf::ComponentType::UnsignedInt)
            {
                fastgltf::copyFromAccessor<std::uint32_t>(asset, acc, out.data());
            }
            else
            {
                return MakeError("Unsupported index component type");
            }

            return {};
        }

        AxisAlignedBounds BoundsFromPositions(const std::vector<Float3>& positions)
        {
            AxisAlignedBounds b{};
            if (positions.empty())
            {
                return b;
            }

            Float3 mn = positions.front();
            Float3 mx = positions.front();
            for (const Float3& p : positions)
            {
                mn = Maths::Min(mn, p);
                mx = Maths::Max(mx, p);
            }

            b.Min = mn;
            b.Max = mx;
            return b;
        }

        Result<BuiltSubmesh> BuildSubmeshFromPrimitive(const PrimitiveBuildInput& input)
        {
            const fastgltf::Asset& asset = *input.Asset;
            const fastgltf::Primitive& primitive = *input.Primitive;

            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                return MakeError("Only triangle primitives are supported");
            }

            const auto* posAttr = primitive.findAttribute("POSITION");
            if (posAttr == primitive.attributes.end())
            {
                return MakeError("Primitive missing POSITION attribute");
            }

            std::vector<Float3> positions;
            if (const auto r = ReadVec3(asset, posAttr->accessorIndex, positions); !r)
            {
                return MakeError(r.error().GetMessage());
            }

            const std::size_t vertexCount = positions.size();

            std::vector<Float3> normals(vertexCount, Float3{0.0f, 1.0f, 0.0f});
            const auto* nrmAttr = primitive.findAttribute("NORMAL");
            if (nrmAttr != primitive.attributes.end())
            {
                if (const auto r = ReadVec3(asset, nrmAttr->accessorIndex, normals); !r)
                {
                    return MakeError(r.error().GetMessage());
                }
                if (normals.size() != vertexCount)
                {
                    return MakeError("NORMAL vertex count mismatch");
                }
            }

            std::vector<Float2> uvs(vertexCount, Float2{0.0f});
            const auto* uvAttr = primitive.findAttribute("TEXCOORD_0");
            if (uvAttr != primitive.attributes.end())
            {
                if (const auto r = ReadVec2(asset, uvAttr->accessorIndex, uvs); !r)
                {
                    return MakeError(r.error().GetMessage());
                }
                if (uvs.size() != vertexCount)
                {
                    return MakeError("TEXCOORD_0 vertex count mismatch");
                }
            }

            std::vector<Float4> tangents(vertexCount);
            const auto* tanAttr = primitive.findAttribute("TANGENT");
            if (tanAttr != primitive.attributes.end())
            {
                if (const auto r = ReadVec4(asset, tanAttr->accessorIndex, tangents); !r)
                {
                    return MakeError(r.error().GetMessage());
                }
                if (tangents.size() != vertexCount)
                {
                    return MakeError("TANGENT vertex count mismatch");
                }
            }
            else
            {
                for (std::size_t i = 0; i < vertexCount; ++i)
                {
                    tangents.at(i) = ComputeFallbackTangent(Maths::Normalize(normals.at(i)));
                }
            }

            std::vector<VertexPosNormalUVTangent> vertices(vertexCount);
            for (std::size_t i = 0; i < vertexCount; ++i)
            {
                auto& vertex = vertices.at(i);
                vertex.Position = positions.at(i);
                vertex.Normal = normals.at(i);
                vertex.UV = uvs.at(i);
                vertex.Tangent = tangents.at(i);
            }

            std::vector<std::uint32_t> indices;
            if (primitive.indicesAccessor)
            {
                if (const auto r = ReadIndices(asset, *primitive.indicesAccessor, indices); !r)
                {
                    return MakeError(r.error().GetMessage());
                }
            }
            else
            {
                indices.resize(vertexCount);
                for (std::size_t i = 0; i < vertexCount; ++i)
                {
                    indices.at(i) = static_cast<std::uint32_t>(i);
                }
            }

            if (indices.size() % 3 != 0)
            {
                return MakeError("Index count is not a multiple of 3");
            }

            meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertexCount);

            const auto* posBase = reinterpret_cast<const float*>(vertices.data());
            meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), posBase, vertexCount, sizeof(VertexPosNormalUVTangent), 1.05f);

            BuiltSubmesh builtSubmesh{};
            builtSubmesh.Bounds = BoundsFromPositions(positions);

            builtSubmesh.Mesh.VertexFormat = MeshVertexFormat::PosNormalUVTangent;
            builtSubmesh.Mesh.VertexCount = static_cast<std::uint32_t>(vertexCount);
            builtSubmesh.Mesh.IndexCount = static_cast<std::uint32_t>(indices.size());
            builtSubmesh.Mesh.MaterialSlot = primitive.materialIndex ? static_cast<std::uint32_t>(*primitive.materialIndex) : 0u;
            builtSubmesh.Mesh.Bounds = builtSubmesh.Bounds;

            builtSubmesh.Mesh.VertexBytes.resize(vertices.size() * sizeof(VertexPosNormalUVTangent));
            std::memcpy(builtSubmesh.Mesh.VertexBytes.data(), vertices.data(), builtSubmesh.Mesh.VertexBytes.size());

            const bool use32 = vertexCount > 65535u;
            builtSubmesh.Mesh.IndexFormat = use32 ? MeshIndexFormat::Uint32 : MeshIndexFormat::Uint16;

            if (use32)
            {
                builtSubmesh.Mesh.IndexBytes.resize(indices.size() * sizeof(std::uint32_t));
                std::memcpy(builtSubmesh.Mesh.IndexBytes.data(), indices.data(), builtSubmesh.Mesh.IndexBytes.size());
            }
            else
            {
                std::vector<std::uint16_t> idx16(indices.size());
                for (std::size_t i = 0; i < indices.size(); ++i)
                {
                    idx16.at(i) = static_cast<std::uint16_t>(indices.at(i));
                }
                builtSubmesh.Mesh.IndexBytes.resize(idx16.size() * sizeof(std::uint16_t));
                std::memcpy(builtSubmesh.Mesh.IndexBytes.data(), idx16.data(), builtSubmesh.Mesh.IndexBytes.size());
            }

            return builtSubmesh;
        }
    } // namespace

    Result<void> ImportMesh(const MeshImportRequest& request)
    {
        const std::filesystem::path& gltfPath = request.SourcePath;
        const std::filesystem::path& outputDir = request.OutputDirectory;

        if (!std::filesystem::exists(gltfPath))
        {
            return MakeError("Input file does not exist: " + gltfPath.generic_string());
        }

        std::filesystem::create_directories(outputDir);

        auto bufferExpected = fastgltf::GltfDataBuffer::FromPath(gltfPath);
        if (bufferExpected.error() != fastgltf::Error::None)
        {
            return MakeError("Failed to read glTF file: " + std::string{fastgltf::getErrorMessage(bufferExpected.error())});
        }

        fastgltf::Parser parser;
        const fastgltf::Options options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices;
        fastgltf::GltfDataBuffer& buffer = bufferExpected.get();
        fastgltf::Expected<fastgltf::Asset> assetExpected = parser.loadGltf(buffer, gltfPath.parent_path(), options);
        if (assetExpected.error() != fastgltf::Error::None)
        {
            return MakeError("Failed to parse glTF: " + std::string{fastgltf::getErrorMessage(assetExpected.error())});
        }

        const fastgltf::Asset& asset = assetExpected.get();
        if (asset.meshes.empty())
        {
            return MakeError("glTF file contains no meshes");
        }

        ParsedMeshFile parsed{};
        parsed.Header.Magic = MESH_FILE_MAGIC;
        parsed.Header.Version = MESH_FILE_VERSION;
        parsed.Header.Flags = 0;

        AxisAlignedBounds wholeBounds{};
        bool haveBounds = false;

        std::vector<nlohmann::json> submeshJsonEntries;

        for (const fastgltf::Mesh& mesh : asset.meshes)
        {
            for (std::size_t p = 0; p < mesh.primitives.size(); ++p)
            {
                const fastgltf::Primitive& prim = mesh.primitives.at(p);
                if (prim.type != fastgltf::PrimitiveType::Triangles)
                {
                    continue;
                }

                auto builtSubmesh = BuildSubmeshFromPrimitive({.Asset = &asset, .Primitive = &prim});
                if (!builtSubmesh)
                {
                    return MakeError(builtSubmesh.error().GetMessage());
                }

                const AxisAlignedBounds& subBounds = builtSubmesh->Bounds;

                if (!haveBounds)
                {
                    wholeBounds = subBounds;
                    haveBounds = true;
                }
                else
                {
                    wholeBounds.Min = Maths::Min(wholeBounds.Min, subBounds.Min);
                    wholeBounds.Max = Maths::Max(wholeBounds.Max, subBounds.Max);
                }

                parsed.Submeshes.push_back(std::move(builtSubmesh->Mesh));

                const std::string name = mesh.name.empty() ? ("submesh_" + std::to_string(submeshJsonEntries.size())) : (std::string{mesh.name} + "_" + std::to_string(p));

                nlohmann::json entry;
                entry.emplace("name", name);
                entry.emplace("material_slot", prim.materialIndex ? static_cast<std::uint32_t>(*prim.materialIndex) : 0u);
                submeshJsonEntries.push_back(std::move(entry));
            }
        }

        if (parsed.Submeshes.empty())
        {
            return MakeError("No triangle primitives were imported");
        }

        parsed.Header.SubmeshCount = static_cast<std::uint32_t>(parsed.Submeshes.size());
        parsed.Header.Bounds = wholeBounds;

        parsed.SubmeshTable.assign(parsed.Header.SubmeshCount, SubmeshTableEntry{});
        for (std::uint32_t i = 0; i < parsed.Header.SubmeshCount; ++i)
        {
            SubmeshTableEntry& tableEntry = parsed.SubmeshTable.at(i);
            const SubmeshCpuData& submesh = parsed.Submeshes.at(i);
            tableEntry.VertexFormat = submesh.VertexFormat;
            tableEntry.IndexFormat = submesh.IndexFormat;
            tableEntry.VertexCount = submesh.VertexCount;
            tableEntry.IndexCount = submesh.IndexCount;
            tableEntry.Bounds = submesh.Bounds;
            tableEntry.MaterialSlot = submesh.MaterialSlot;
        }

        std::vector<std::byte> binary;
        std::string writeErr;
        if (!WriteMeshFileV1(parsed, binary, writeErr))
        {
            return MakeError(std::move(writeErr));
        }

        const std::string stem = std::string{request.NameStem.empty() ? gltfPath.stem().string() : std::string{request.NameStem}};
        const std::filesystem::path wfmeshName = stem + ".wfmesh";
        const std::filesystem::path jsonName = stem + ".json";
        const std::filesystem::path wfmeshPath = outputDir / wfmeshName;

        {
            std::ofstream out(wfmeshPath.string(), std::ios::binary | std::ios::trunc);
            if (!out.is_open())
            {
                return MakeError("Failed to write " + wfmeshPath.generic_string());
            }
            out.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
        }

        const AssetId newId = AssetId::Generate();

        nlohmann::json json;
        json.emplace("asset_id", newId.ToString());
        json.emplace("asset_type", "mesh");
        json.emplace("name", stem);
        json.emplace("source", wfmeshName.generic_string());

        nlohmann::json submeshesJson = nlohmann::json::array();
        for (const auto& entry : submeshJsonEntries)
        {
            submeshesJson.push_back(entry);
        }
        json.emplace("submeshes", std::move(submeshesJson));

        const std::filesystem::path jsonPath = outputDir / jsonName;
        {
            std::ofstream out(jsonPath.string(), std::ios::trunc);
            if (!out.is_open())
            {
                return MakeError("Failed to write " + jsonPath.generic_string());
            }
            out << json.dump(4);
        }

        return {};
    }

} // namespace Wayfinder::Waypoint
