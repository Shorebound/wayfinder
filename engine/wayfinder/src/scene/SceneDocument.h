#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Identifiers.h"
#include "wayfinder_exports.h"

namespace Wayfinder
{
    class AssetService;
    class RuntimeComponentRegistry;

    struct SceneDocumentEntity
    {
        SceneObjectId Id;
        std::string Name;
        std::optional<SceneObjectId> ParentId;
        std::optional<AssetId> PrefabAssetId;
        nlohmann::json ComponentData = nlohmann::json::object();
    };

    /// Current scene format version.
    inline constexpr int SCENE_FORMAT_VERSION = 1;

    struct SceneDocument
    {
        int Version = SCENE_FORMAT_VERSION;
        std::string Name;
        nlohmann::json Settings = nlohmann::json::object();
        std::vector<SceneDocumentEntity> Entities;
    };

    struct SceneDocumentLoadResult
    {
        std::optional<SceneDocument> Document;
        std::vector<std::string> Errors;
    };

    WAYFINDER_API SceneDocumentLoadResult LoadSceneDocument(
        const std::string& filePath, const RuntimeComponentRegistry& registry, AssetService* assetService = nullptr);

    WAYFINDER_API bool SaveSceneDocument(const SceneDocument& document, const std::string& filePath, std::string& error);
} // namespace Wayfinder