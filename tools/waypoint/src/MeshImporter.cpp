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
            const auto& acc = asset.accessors[accessorIndex];
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
            const auto& acc = asset.accessors[accessorIndex];
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
            const auto& acc = asset.accessors[accessorIndex];
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
            const auto& acc = asset.accessors[accessorIndex];
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
                    out[i] = tmp[i];
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

            Float3 mn = positions[0];
            Float3 mx = positions[0];
            for (const Float3& p : positions)
            {
                mn = Maths::Min(mn, p);
                mx = Maths::Max(mx, p);
            }

            b.Min = mn;
            b.Max = mx;
            return b;
        }

        Result<void> BuildSubmeshFromPrimitive(const fastgltf::Asset& asset, const fastgltf::Primitive& prim, SubmeshCpuData& outMesh, AxisAlignedBounds& outBounds)
        {
            if (prim.type != fastgltf::PrimitiveType::Triangles)
            {
                return MakeError("Only triangle primitives are supported");
            }

            const auto* posAttr = prim.findAttribute("POSITION");
            if (posAttr == prim.attributes.end())
            {
                return MakeError("Primitive missing POSITION attribute");
            }

            std::vector<Float3> positions;
            if (const auto r = ReadVec3(asset, posAttr->accessorIndex, positions); !r)
            {
                return r;
            }

            const std::size_t vertexCount = positions.size();

            std::vector<Float3> normals(vertexCount, Float3{0.0f, 1.0f, 0.0f});
            const auto* nrmAttr = prim.findAttribute("NORMAL");
            if (nrmAttr != prim.attributes.end())
            {
                if (const auto r = ReadVec3(asset, nrmAttr->accessorIndex, normals); !r)
                {
                    return r;
                }
                if (normals.size() != vertexCount)
                {
                    return MakeError("NORMAL vertex count mismatch");
                }
            }

            std::vector<Float2> uvs(vertexCount, Float2{0.0f});
            const auto* uvAttr = prim.findAttribute("TEXCOORD_0");
            if (uvAttr != prim.attributes.end())
            {
                if (const auto r = ReadVec2(asset, uvAttr->accessorIndex, uvs); !r)
                {
                    return r;
                }
                if (uvs.size() != vertexCount)
                {
                    return MakeError("TEXCOORD_0 vertex count mismatch");
                }
            }

            std::vector<Float4> tangents(vertexCount);
            const auto* tanAttr = prim.findAttribute("TANGENT");
            if (tanAttr != prim.attributes.end())
            {
                if (const auto r = ReadVec4(asset, tanAttr->accessorIndex, tangents); !r)
                {
                    return r;
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
                    tangents[i] = ComputeFallbackTangent(Maths::Normalize(normals[i]));
                }
            }

            std::vector<VertexPosNormalUVTangent> vertices(vertexCount);
            for (std::size_t i = 0; i < vertexCount; ++i)
            {
                vertices[i].Position = positions[i];
                vertices[i].Normal = normals[i];
                vertices[i].UV = uvs[i];
                vertices[i].Tangent = tangents[i];
            }

            std::vector<std::uint32_t> indices;
            if (prim.indicesAccessor)
            {
                if (const auto r = ReadIndices(asset, *prim.indicesAccessor, indices); !r)
                {
                    return r;
                }
            }
            else
            {
                indices.resize(vertexCount);
                for (std::size_t i = 0; i < vertexCount; ++i)
                {
                    indices[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(i);
                }
            }

            if (indices.size() % 3 != 0)
            {
                return MakeError("Index count is not a multiple of 3");
            }

            meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertexCount);

            const float* posBase = reinterpret_cast<const float*>(vertices.data());
            meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), posBase, vertexCount, sizeof(VertexPosNormalUVTangent), 1.05f);

            outBounds = BoundsFromPositions(positions);

            outMesh.VertexFormat = MeshVertexFormat::PosNormalUVTangent;
            outMesh.VertexCount = static_cast<std::uint32_t>(vertexCount);
            outMesh.IndexCount = static_cast<std::uint32_t>(indices.size());
            outMesh.MaterialSlot = prim.materialIndex ? static_cast<std::uint32_t>(*prim.materialIndex) : 0u;
            outMesh.Bounds = outBounds;

            outMesh.VertexBytes.resize(vertices.size() * sizeof(VertexPosNormalUVTangent));
            std::memcpy(outMesh.VertexBytes.data(), vertices.data(), outMesh.VertexBytes.size());

            const bool use32 = vertexCount > 65535u;
            outMesh.IndexFormat = use32 ? MeshIndexFormat::Uint32 : MeshIndexFormat::Uint16;

            if (use32)
            {
                outMesh.IndexBytes.resize(indices.size() * sizeof(std::uint32_t));
                std::memcpy(outMesh.IndexBytes.data(), indices.data(), outMesh.IndexBytes.size());
            }
            else
            {
                std::vector<std::uint16_t> idx16(indices.size());
                for (std::size_t i = 0; i < indices.size(); ++i)
                {
                    idx16[i] = static_cast<std::uint16_t>(indices[i]);
                }
                outMesh.IndexBytes.resize(idx16.size() * sizeof(std::uint16_t));
                std::memcpy(outMesh.IndexBytes.data(), idx16.data(), outMesh.IndexBytes.size());
            }

            return {};
        }
    } // namespace

    Result<void> ImportMesh(const std::filesystem::path& gltfPath, const std::filesystem::path& outputDir, const std::string_view nameStem)
    {
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

        fastgltf::Asset& asset = assetExpected.get();
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
                const fastgltf::Primitive& prim = mesh.primitives[p];
                if (prim.type != fastgltf::PrimitiveType::Triangles)
                {
                    continue;
                }

                SubmeshCpuData sm{};
                AxisAlignedBounds subBounds{};
                if (const auto r = BuildSubmeshFromPrimitive(asset, prim, sm, subBounds); !r)
                {
                    return r;
                }

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

                parsed.Submeshes.push_back(std::move(sm));

                const std::string name = mesh.name.empty() ? ("submesh_" + std::to_string(submeshJsonEntries.size())) : (std::string{mesh.name} + "_" + std::to_string(p));

                nlohmann::json entry;
                entry["name"] = name;
                entry["material_slot"] = prim.materialIndex ? static_cast<std::uint32_t>(*prim.materialIndex) : 0u;
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
            parsed.SubmeshTable[i].VertexFormat = parsed.Submeshes[i].VertexFormat;
            parsed.SubmeshTable[i].IndexFormat = parsed.Submeshes[i].IndexFormat;
            parsed.SubmeshTable[i].VertexCount = parsed.Submeshes[i].VertexCount;
            parsed.SubmeshTable[i].IndexCount = parsed.Submeshes[i].IndexCount;
            parsed.SubmeshTable[i].Bounds = parsed.Submeshes[i].Bounds;
            parsed.SubmeshTable[i].MaterialSlot = parsed.Submeshes[i].MaterialSlot;
        }

        std::vector<std::byte> binary;
        std::string writeErr;
        if (!WriteMeshFileV1(parsed, binary, writeErr))
        {
            return MakeError(std::move(writeErr));
        }

        const std::string stem = std::string{nameStem.empty() ? gltfPath.stem().string() : std::string{nameStem}};
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
        json["asset_id"] = newId.ToString();
        json["asset_type"] = "mesh";
        json["name"] = stem;
        json["source"] = wfmeshName.generic_string();

        nlohmann::json submeshesJson = nlohmann::json::array();
        for (const auto& entry : submeshJsonEntries)
        {
            submeshesJson.push_back(entry);
        }
        json["submeshes"] = std::move(submeshesJson);

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
