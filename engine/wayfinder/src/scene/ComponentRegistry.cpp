#include "ComponentRegistry.h"

#include "Components.h"
#include "app/Subsystem.h"
#include "core/Log.h"
#include "gameplay/GameplayTag.h"
#include "gameplay/GameplayTagRegistry.h"
#include "scene/entity/Entity.h"

#include "rendering/materials/PostProcessRegistry.h"
#include "rendering/materials/PostProcessVolume.h"

#include <array>
#include <charconv>
#include <format>
#include <sstream>
#include <string>
#include <string_view>

namespace Wayfinder
{
    namespace
    {
        std::optional<Wayfinder::AssetId> ReadOptionalAssetId(const nlohmann::json& data, std::string_view key)
        {
            if (!data.contains(key) || !data.at(key).is_string())
            {
                return std::nullopt;
            }

            return Wayfinder::AssetId::Parse(data.at(key).get<std::string>());
        }

        template<typename T_>
        void RegisterComponent(flecs::world& world)
        {
            world.component<T_>();
        }

        float ReadFloat(const nlohmann::json& data, std::string_view key, float fallback)
        {
            if (!data.contains(key))
            {
                return fallback;
            }
            const auto& val = data.at(key);
            if (val.is_number())
            {
                return val.get<float>();
            }
            return fallback;
        }

        float ReadArrayFloat(const nlohmann::json& arr, size_t index, float fallback) // NOLINT(bugprone-easily-swappable-parameters)
        {
            if (index >= arr.size())
            {
                return fallback;
            }
            const auto& val = arr.at(index);
            if (val.is_number())
            {
                return val.get<float>();
            }
            return fallback;
        }

        Wayfinder::Float3 ReadVector3(const nlohmann::json& data, std::string_view key, const Wayfinder::Float3& fallback)
        {
            if (!data.contains(key))
            {
                return fallback;
            }
            const auto& arr = data.at(key);
            if (!arr.is_array() || arr.size() != 3)
            {
                return fallback;
            }

            Wayfinder::Float3 result = fallback;
            result.x = ReadArrayFloat(arr, 0, result.x); // NOLINT(cppcoreguidelines-pro-type-union-access)
            result.y = ReadArrayFloat(arr, 1, result.y); // NOLINT(cppcoreguidelines-pro-type-union-access)
            result.z = ReadArrayFloat(arr, 2, result.z); // NOLINT(cppcoreguidelines-pro-type-union-access)
            return result;
        }

        nlohmann::json WriteVector3(const Wayfinder::Float3& value)
        {
            return nlohmann::json::array({value.x, value.y, value.z}); // NOLINT(cppcoreguidelines-pro-type-union-access)
        }

        uint8_t ClampColourChannel(int64_t value)
        {
            if (value < 0)
            {
                return 0;
            }
            if (value > 255)
            {
                return 255;
            }
            return static_cast<uint8_t>(value);
        }

        Wayfinder::Colour ReadColour(const nlohmann::json& data, std::string_view key, const Wayfinder::Colour& fallback)
        {
            if (!data.contains(key))
            {
                return fallback;
            }
            const auto& arr = data.at(key);
            if (!arr.is_array() || (arr.size() != 3 && arr.size() != 4))
            {
                return fallback;
            }

            Wayfinder::Colour result = fallback;
            result.r = arr.at(0).is_number_integer() ? ClampColourChannel(arr.at(0).get<int64_t>()) : result.r;
            result.g = arr.at(1).is_number_integer() ? ClampColourChannel(arr.at(1).get<int64_t>()) : result.g;
            result.b = arr.at(2).is_number_integer() ? ClampColourChannel(arr.at(2).get<int64_t>()) : result.b;
            result.a = (arr.size() == 4 && arr.at(3).is_number_integer()) ? ClampColourChannel(arr.at(3).get<int64_t>()) : result.a;
            return result;
        }

        nlohmann::json WriteColour(const Wayfinder::Colour& value)
        {
            return nlohmann::json::array({static_cast<int64_t>(value.r), static_cast<int64_t>(value.g), static_cast<int64_t>(value.b), static_cast<int64_t>(value.a)});
        }

        std::string_view ToString(Wayfinder::MeshPrimitive primitive)
        {
            switch (primitive)
            {
            case Wayfinder::MeshPrimitive::Cube:
                return "cube";
            }

            return "cube";
        }

        std::string_view ToString(Wayfinder::ProjectionMode projection)
        {
            switch (projection)
            {
            case Wayfinder::ProjectionMode::Perspective:
                return "perspective";
            case Wayfinder::ProjectionMode::Orthographic:
                return "orthographic";
            }

            return "perspective";
        }

        std::string_view ToString(Wayfinder::LightType type)
        {
            switch (type)
            {
            case Wayfinder::LightType::Point:
                return "point";
            case Wayfinder::LightType::Directional:
                return "directional";
            }

            return "point";
        }

        Wayfinder::InternedString ReadRenderLayer(const nlohmann::json& data, std::string_view key, const Wayfinder::InternedString& fallback)
        {
            if (!data.contains(key) || !data.at(key).is_string())
            {
                return fallback;
            }

            const auto layer = data.at(key).get<std::string>();
            if (layer.empty())
            {
                return fallback;
            }

            return Wayfinder::InternedString::Intern(layer);
        }

        Wayfinder::MeshPrimitive ReadPrimitive(const nlohmann::json& data, std::string_view key, Wayfinder::MeshPrimitive fallback)
        {
            if (!data.contains(key) || !data.at(key).is_string())
            {
                return fallback;
            }

            const auto primitive = data.at(key).get<std::string>();
            if (primitive == "cube")
            {
                return Wayfinder::MeshPrimitive::Cube;
            }

            return fallback;
        }

        Wayfinder::ProjectionMode ReadProjection(const nlohmann::json& data, std::string_view key, Wayfinder::ProjectionMode fallback)
        {
            if (!data.contains(key) || !data.at(key).is_string())
            {
                return fallback;
            }

            const auto projection = data.at(key).get<std::string>();
            if (projection == "orthographic")
            {
                return Wayfinder::ProjectionMode::Orthographic;
            }

            return fallback;
        }

        Wayfinder::LightType ReadLightType(const nlohmann::json& data, std::string_view key, Wayfinder::LightType fallback)
        {
            if (!data.contains(key) || !data.at(key).is_string())
            {
                return fallback;
            }

            const auto type = data.at(key).get<std::string>();
            if (type == "directional")
            {
                return Wayfinder::LightType::Directional;
            }

            if (type == "point")
            {
                return Wayfinder::LightType::Point;
            }

            return fallback;
        }

        bool ValidateOptionalNonEmptyString(const nlohmann::json& data, std::string_view key, std::string& error)
        {
            if (!data.contains(key))
            {
                return true;
            }

            const auto& node = data.at(key);
            if (!node.is_string())
            {
                error = std::format("'{}' must be a string", key);
                return false;
            }

            if (node.get<std::string>().empty())
            {
                error = std::format("'{}' must not be empty", key);
                return false;
            }

            return true;
        }

        uint8_t ClampToByte(const int64_t value)
        {
            if (value < 0)
            {
                return 0;
            }

            if (value > 255)
            {
                return 255;
            }

            return static_cast<uint8_t>(value);
        }

        bool IsNumberNode(const nlohmann::json& node)
        {
            return node.is_number();
        }

        bool ValidateOptionalBool(const nlohmann::json& data, std::string_view key, std::string& error)
        {
            if (!data.contains(key))
            {
                return true;
            }

            if (!data.at(key).is_boolean())
            {
                error = std::format("field '{}' must be a boolean", key);
                return false;
            }

            return true;
        }

        bool ValidateOptionalAssetId(const nlohmann::json& data, std::string_view key, std::string& error)
        {
            if (!data.contains(key))
            {
                return true;
            }

            const auto& node = data.at(key);
            if (!node.is_string())
            {
                error = std::format("field '{}' must be a UUID string", key);
                return false;
            }

            const auto assetId = Wayfinder::AssetId::Parse(node.get<std::string>());
            if (!assetId)
            {
                error = std::format("field '{}' must be a valid UUID", key);
                return false;
            }

            return true;
        }

        bool ValidateOptionalNumber(const nlohmann::json& data, std::string_view key, std::string& error)
        {
            if (!data.contains(key))
            {
                return true;
            }

            if (!IsNumberNode(data.at(key)))
            {
                error = std::format("field '{}' must be numeric", key);
                return false;
            }

            return true;
        }

        bool ValidateOptionalInteger(const nlohmann::json& data, std::string_view key, std::string& error)
        {
            if (!data.contains(key))
            {
                return true;
            }

            if (!data.at(key).is_number_integer())
            {
                error = std::format("field '{}' must be an integer", key);
                return false;
            }

            return true;
        }

        bool ValidateOptionalVector3(const nlohmann::json& data, std::string_view key, std::string& error)
        {
            if (!data.contains(key))
            {
                return true;
            }

            const auto& node = data.at(key);
            if (!node.is_array() || node.size() != 3)
            {
                error = std::format("field '{}' must be an array of 3 numbers", key);
                return false;
            }

            for (const auto& index : node)
            {
                if (!IsNumberNode(index))
                {
                    error = std::format("field '{}' must be an array of 3 numbers", key);
                    return false;
                }
            }

            return true;
        }

        bool ValidateOptionalColour(const nlohmann::json& data, std::string_view key, std::string& error)
        {
            if (!data.contains(key))
            {
                return true;
            }

            const auto& node = data.at(key);
            if (!node.is_array() || (node.size() != 3 && node.size() != 4))
            {
                error = std::format("field '{}' must be an array of 3 or 4 integers", key);
                return false;
            }

            for (size_t index = 0; index < node.size(); ++index)
            {
                if (!node.at(index).is_number_integer())
                {
                    error = std::format("field '{}' must be an array of 3 or 4 integers", key);
                    return false;
                }

                const int64_t channelValue = node.at(index).get<int64_t>();
                if (channelValue < 0 || channelValue > 255)
                {
                    error = std::format("field '{}' channel {} value {} is out of range (0-255)", key, index, channelValue);
                    return false;
                }
            }

            return true;
        }

        bool ValidateOptionalEnumValue(const nlohmann::json& data, std::string_view key, std::initializer_list<std::string_view> acceptedValues, std::string& error)
        {
            if (!data.contains(key))
            {
                return true;
            }

            const auto& node = data.at(key);
            if (!node.is_string())
            {
                error = std::format("field '{}' must be a string", key);
                return false;
            }

            const std::string value = node.get<std::string>();
            for (const std::string_view acceptedValue : acceptedValues)
            {
                if (value == acceptedValue)
                {
                    return true;
                }
            }

            std::ostringstream stream;
            stream << "field '" << key << "' must be one of ";
            bool first = true;
            for (const std::string_view acceptedValue : acceptedValues)
            {
                if (!first)
                {
                    stream << ", ";
                }
                stream << '\'' << acceptedValue << '\'';
                first = false;
            }
            error = stream.str();
            return false;
        }

        bool ValidateTransform(const nlohmann::json& data, std::string& error)
        {
            return ValidateOptionalVector3(data, "position", error) && ValidateOptionalVector3(data, "rotation", error) && ValidateOptionalVector3(data, "scale", error);
        }

        bool ValidateMesh(const nlohmann::json& data, std::string& error)
        {
            if (!ValidateOptionalEnumValue(data, "primitive", {"cube"}, error) || !ValidateOptionalVector3(data, "dimensions", error))
            {
                return false;
            }

            if (data.contains("material_slots"))
            {
                if (!data.at("material_slots").is_object())
                {
                    error = "'material_slots' must be an object mapping slot indices to asset IDs";
                    return false;
                }
                for (const auto& [key, value] : data.at("material_slots").items())
                {
                    if (!value.is_string())
                    {
                        error = "material_slots['" + key + "'] must be a string asset ID";
                        return false;
                    }
                    uint32_t unused = 0;
                    const auto [ptr, ec] = std::from_chars(key.data(), key.data() + key.size(), unused);
                    if (ec != std::errc{} || ptr != key.data() + key.size())
                    {
                        error = "material_slots key '" + key + "' must be an unsigned integer";
                        return false;
                    }
                }
            }

            return true;
        }

        bool ValidateCamera(const nlohmann::json& data, std::string& error)
        {
            return ValidateOptionalBool(data, "primary", error) && ValidateOptionalVector3(data, "target", error) && ValidateOptionalVector3(data, "up", error) && ValidateOptionalNumber(data, "fov", error) &&
                   ValidateOptionalEnumValue(data, "projection", {"perspective", "orthographic"}, error);
        }

        bool ValidateLight(const nlohmann::json& data, std::string& error)
        {
            return ValidateOptionalEnumValue(data, "type", {"point", "directional"}, error) && ValidateOptionalColour(data, "colour", error) && ValidateOptionalNumber(data, "intensity", error) &&
                   ValidateOptionalNumber(data, "range", error) && ValidateOptionalBool(data, "debug_draw", error);
        }

        bool ValidateMaterial(const nlohmann::json& data, std::string& error)
        {
            return ValidateOptionalAssetId(data, "material_id", error) && ValidateOptionalColour(data, "base_colour", error);
        }

        bool ValidateRenderOverride(const nlohmann::json& data, std::string& error)
        {
            return ValidateOptionalBool(data, "wireframe", error);
        }

        bool ValidateRenderable(const nlohmann::json& data, std::string& error)
        {
            return ValidateOptionalBool(data, "visible", error) && ValidateOptionalNonEmptyString(data, "layer", error) && ValidateOptionalInteger(data, "sort_priority", error);
        }

        bool ValidateEffectParameter(std::string_view key, const nlohmann::json& node, std::string& error)
        {
            if (node.is_number_integer() || node.is_number_float())
            {
                return true;
            }

            if (node.is_array())
            {
                if (node.size() < 3 || node.size() > 4)
                {
                    error = std::string("effect parameter '") + std::string(key) + "' array must have 3 or 4 elements";
                    return false;
                }

                bool allInts = true;
                for (const auto& i : node)
                {
                    if (!i.is_number_integer() && !i.is_number_float())
                    {
                        error = std::string("effect parameter '") + std::string(key) + "' array elements must be numbers";
                        return false;
                    }
                    if (!i.is_number_integer())
                    {
                        allInts = false;
                    }
                }

                // 4-element arrays are only valid as Colour (all integers r,g,b,a).
                // Float3 only reads 3 elements, so a 4-element float array would silently lose data.
                if (node.size() == 4 && !allInts)
                {
                    error = std::string("effect parameter '") + std::string(key) + "' 4-element arrays must be all integers (Colour r,g,b,a)";
                    return false;
                }

                // Validate Colour channel ranges (0-255) for all-integer arrays.
                if (allInts)
                {
                    for (size_t i = 0; i < node.size(); ++i)
                    {
                        const int64_t channelValue = node.at(i).get<int64_t>();
                        if (channelValue < 0 || channelValue > 255)
                        {
                            error = std::string("effect parameter '") + std::string(key) + "' channel " + std::to_string(i) + " value " + std::to_string(channelValue) + " is out of range (0-255)";
                            return false;
                        }
                    }
                }

                // 3-element all-integer arrays are ambiguous: ReadEffectParam treats them
                // as Colour (r,g,b with a=255), not Float3. Require at least one float
                // for Float3 values (e.g. [1.0, 2.0, 3.0]).
                if (node.size() == 3 && allInts)
                {
                    error = std::string("effect parameter '") + std::string(key) + "' 3-element all-integer arrays are interpreted as Colour, not Float3; use floats for Float3 (e.g. [1.0, 2.0, 3.0])";
                    return false;
                }

                return true;
            }

            error = std::string("effect parameter '") + std::string(key) + "' must be a number or array of numbers";
            return false;
        }

        bool ValidatePostProcessVolume(const nlohmann::json& data, std::string& error)
        {
            if (!ValidateOptionalEnumValue(data, "shape", {"global", "box", "sphere"}, error))
            {
                return false;
            }

            if (!ValidateOptionalInteger(data, "priority", error))
            {
                return false;
            }

            if (!ValidateOptionalNumber(data, "blend_distance", error))
            {
                return false;
            }

            if (!ValidateOptionalVector3(data, "dimensions", error))
            {
                return false;
            }

            if (!ValidateOptionalNumber(data, "radius", error))
            {
                return false;
            }

            // Validate the effects array if present
            if (data.contains("effects"))
            {
                const auto& effectsNode = data.at("effects");
                if (!effectsNode.is_array())
                {
                    error = "'effects' must be an array of tables";
                    return false;
                }

                for (const auto& effectEntry : effectsNode)
                {
                    if (!effectEntry.is_object())
                    {
                        error = "each entry in 'effects' must be a table";
                        return false;
                    }

                    if (!effectEntry.contains("type") || !effectEntry.at("type").is_string() || effectEntry.at("type").get<std::string>().empty())
                    {
                        error = "each effect must have a non-empty 'type' string";
                        return false;
                    }

                    const std::string effectTypeStr = effectEntry.at("type").get<std::string>();
                    const std::string normalised = Wayfinder::NormalisePostProcessEffectTypeString(effectTypeStr);
                    if (!Wayfinder::IsValidPostProcessEffectTypeName(normalised))
                    {
                        error = "unknown post-process effect type: " + effectTypeStr;
                        return false;
                    }

                    for (const auto& [key, value] : effectEntry.items())
                    {
                        if (key == "type")
                        {
                            continue;
                        }
                        if (key == "enabled")
                        {
                            if (!value.is_boolean())
                            {
                                error = "effect 'enabled' must be a boolean";
                                return false;
                            }
                            continue;
                        }
                        if (!ValidateEffectParameter(key, value, error))
                        {
                            return false;
                        }
                    }
                }
            }

            return true;
        }

        void ApplyTransform(const nlohmann::json& data, Wayfinder::Entity& entity)
        {
            Wayfinder::TransformComponent transform;
            transform.Local.Position = ReadVector3(data, "position", transform.Local.Position);
            transform.Local.RotationDegrees = ReadVector3(data, "rotation", transform.Local.RotationDegrees);
            transform.Local.Scale = ReadVector3(data, "scale", transform.Local.Scale);
            entity.AddComponent<Wayfinder::TransformComponent>(transform);
        }

        void ApplyMesh(const nlohmann::json& data, Wayfinder::Entity& entity)
        {
            Wayfinder::MeshComponent mesh;
            mesh.Primitive = ReadPrimitive(data, "primitive", mesh.Primitive);
            mesh.Dimensions = ReadVector3(data, "dimensions", mesh.Dimensions);
            mesh.MeshAssetId = ReadOptionalAssetId(data, "mesh_id");

            if (data.contains("material_slots") && data.at("material_slots").is_object())
            {
                for (const auto& [slotKey, assetIdValue] : data.at("material_slots").items())
                {
                    if (!assetIdValue.is_string())
                    {
                        continue;
                    }
                    uint32_t slotIndex = 0;
                    const auto [ptr, ec] = std::from_chars(slotKey.data(), slotKey.data() + slotKey.size(), slotIndex);
                    if (ec != std::errc{} || ptr != slotKey.data() + slotKey.size())
                    {
                        continue;
                    }
                    auto parsed = Wayfinder::AssetId::Parse(assetIdValue.get<std::string>());
                    if (parsed)
                    {
                        mesh.MaterialSlotBindings[slotIndex] = *parsed;
                    }
                }
            }

            entity.AddComponent<Wayfinder::MeshComponent>(mesh);
        }

        void ApplyCamera(const nlohmann::json& data, Wayfinder::Entity& entity)
        {
            Wayfinder::CameraComponent camera;
            camera.Primary = data.value("primary", camera.Primary);
            camera.Target = ReadVector3(data, "target", camera.Target);
            camera.Up = ReadVector3(data, "up", camera.Up);
            camera.FieldOfView = ReadFloat(data, "fov", camera.FieldOfView);
            camera.Projection = ReadProjection(data, "projection", camera.Projection);
            entity.AddComponent<Wayfinder::CameraComponent>(camera);
        }

        void ApplyLight(const nlohmann::json& data, Wayfinder::Entity& entity)
        {
            Wayfinder::LightComponent light;
            light.Type = ReadLightType(data, "type", light.Type);
            light.Tint = ReadColour(data, "colour", light.Tint);
            light.Intensity = ReadFloat(data, "intensity", light.Intensity);
            light.Range = ReadFloat(data, "range", light.Range);
            light.DebugDraw = data.value("debug_draw", light.DebugDraw);
            entity.AddComponent<Wayfinder::LightComponent>(light);
        }

        void ApplyMaterial(const nlohmann::json& data, Wayfinder::Entity& entity)
        {
            Wayfinder::MaterialComponent material;
            material.MaterialAssetId = ReadOptionalAssetId(data, "material_id");
            material.HasBaseColourOverride = data.contains("base_colour");
            if (material.HasBaseColourOverride)
            {
                material.BaseColour = ReadColour(data, "base_colour", material.BaseColour);
            }

            entity.AddComponent<Wayfinder::MaterialComponent>(material);
        }

        void ApplyRenderOverride(const nlohmann::json& data, Wayfinder::Entity& entity)
        {
            Wayfinder::RenderOverrideComponent renderOverride;
            if (data.contains("wireframe"))
            {
                renderOverride.Wireframe = data.value("wireframe", false);
            }

            entity.AddComponent<Wayfinder::RenderOverrideComponent>(renderOverride);
        }

        void ApplyRenderable(const nlohmann::json& data, Wayfinder::Entity& entity)
        {
            Wayfinder::RenderableComponent renderable;
            renderable.Visible = data.value("visible", renderable.Visible);
            renderable.Layer = ReadRenderLayer(data, "layer", renderable.Layer);

            const int64_t sortPriority = data.value("sort_priority", static_cast<int64_t>(renderable.SortPriority));
            renderable.SortPriority = ClampToByte(sortPriority);
            entity.AddComponent<Wayfinder::RenderableComponent>(renderable);
        }

        Wayfinder::PostProcessVolumeShape ReadVolumeShape(const nlohmann::json& data, std::string_view key, Wayfinder::PostProcessVolumeShape fallback)
        {
            if (!data.contains(key) || !data.at(key).is_string())
            {
                return fallback;
            }
            const auto value = data.at(key).get<std::string>();
            if (value == "global")
            {
                return Wayfinder::PostProcessVolumeShape::Global;
            }
            if (value == "box")
            {
                return Wayfinder::PostProcessVolumeShape::Box;
            }
            if (value == "sphere")
            {
                return Wayfinder::PostProcessVolumeShape::Sphere;
            }
            return fallback;
        }

        // ── GameplayTagContainer ────────────────────────────────

        bool ValidateTags(const nlohmann::json& data, std::string& error)
        {
            if (!data.contains("tags"))
            {
                return true;
            }

            const auto& node = data.at("tags");
            if (!node.is_array())
            {
                error = "'tags' must be an array of strings";
                return false;
            }

            for (size_t i = 0; i < node.size(); ++i)
            {
                if (!node.at(i).is_string())
                {
                    error = "'tags' array element #" + std::to_string(i) + " must be a string";
                    return false;
                }
            }

            return true;
        }

        void ApplyTags(const nlohmann::json& data, Wayfinder::Entity& entity)
        {
            // In non-Game contexts (e.g. waypoint, tests) the subsystem collection may not be bound.
            // Use Find() to avoid asserting and simply skip tag application if no registry is available.
            auto* registry = Wayfinder::GameSubsystems::Find<Wayfinder::GameplayTagRegistry>();

            Wayfinder::GameplayTagContainer container;
            if (registry)
            {
                if (data.contains("tags") && data.at("tags").is_array())
                {
                    for (const auto& node : data.at("tags"))
                    {
                        if (node.is_string())
                        {
                            container.AddTag(registry->RequestTag(node.get<std::string>()));
                        }
                    }
                }
            }
            else if (data.contains("tags") && data.at("tags").is_array() && !data.at("tags").empty())
            {
                Wayfinder::LogScene.GetLogger()->LogFormat(Wayfinder::LogVerbosity::Warning, "Entity specifies {0} tag(s) but no GameplayTagRegistry is available — tags will be ignored.", data.at("tags").size());
            }
            entity.AddComponent<Wayfinder::GameplayTagContainer>(container);
        }

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        void SerialiseTags(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
        {
            if (!entity.HasComponent<Wayfinder::GameplayTagContainer>())
            {
                return;
            }

            const auto& container = entity.GetComponent<Wayfinder::GameplayTagContainer>();
            if (container.IsEmpty())
            {
                return;
            }

            nlohmann::json arr = nlohmann::json::array();
            for (const auto& tag : container)
            {
                arr.push_back(tag.GetName());
            }

            nlohmann::json t;
            t["tags"] = std::move(arr);
            componentTables["gameplay_tags"] = std::move(t);
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

        // ── PostProcessVolumeComponent ──────────────────────────

        std::string_view ToString(Wayfinder::PostProcessVolumeShape shape)
        {
            switch (shape)
            {
            case Wayfinder::PostProcessVolumeShape::Global:
                return "global";
            case Wayfinder::PostProcessVolumeShape::Box:
                return "box";
            case Wayfinder::PostProcessVolumeShape::Sphere:
                return "sphere";
            }
            return "global";
        }

        Wayfinder::PostProcessEffect ReadEffect(const nlohmann::json& effectData)
        {
            Wayfinder::PostProcessEffect effect{};
            const Wayfinder::PostProcessRegistry* registry = Wayfinder::PostProcessRegistry::GetActiveInstance();
            if (registry == nullptr)
            {
                WAYFINDER_WARN(LogScene, "ReadEffect: PostProcessRegistry not active — post-process effect skipped");
                return effect;
            }

            const std::string typeStr = effectData.value("type", std::string{});
            const std::string normalised = Wayfinder::NormalisePostProcessEffectTypeString(typeStr);
            const std::optional<Wayfinder::PostProcessEffectId> idOpt = registry->FindIdByName(normalised);
            if (!idOpt.has_value())
            {
                WAYFINDER_WARN(LogScene, "ReadEffect: unknown post-process effect type '{}' — skipped", typeStr);
                return effect;
            }

            const Wayfinder::PostProcessEffectDesc* desc = registry->Find(*idOpt);
            if (desc == nullptr || desc->Deserialise == nullptr)
            {
                return effect;
            }

            effect.TypeId = *idOpt;
            effect.Enabled = effectData.value("enabled", true);
            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            desc->Deserialise(effect.Payload, effectData);
            // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            return effect;
        }

        void ApplyPostProcessVolume(const nlohmann::json& data, Wayfinder::Entity& entity)
        {
            Wayfinder::PostProcessVolumeComponent volume;
            volume.Shape = ReadVolumeShape(data, "shape", volume.Shape);
            volume.Priority = static_cast<int>(data.value("priority", static_cast<int64_t>(volume.Priority)));
            volume.BlendDistance = ReadFloat(data, "blend_distance", volume.BlendDistance);
            volume.Dimensions = ReadVector3(data, "dimensions", volume.Dimensions);
            volume.Radius = ReadFloat(data, "radius", volume.Radius);

            if (data.contains("effects") && data.at("effects").is_array())
            {
                const auto& effectsArray = data.at("effects");
                for (const auto& i : effectsArray)
                {
                    if (i.is_object())
                    {
                        volume.Effects.push_back(ReadEffect(i));
                    }
                }
            }

            entity.AddComponent<Wayfinder::PostProcessVolumeComponent>(volume);
        }

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        void SerialiseTransform(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
        {
            if (!entity.HasComponent<Wayfinder::TransformComponent>())
            {
                return;
            }

            const auto& transform = entity.GetComponent<Wayfinder::TransformComponent>();
            nlohmann::json componentTable;
            componentTable["position"] = WriteVector3(transform.Local.Position);
            componentTable["rotation"] = WriteVector3(transform.Local.RotationDegrees);
            componentTable["scale"] = WriteVector3(transform.Local.Scale);
            componentTables["transform"] = std::move(componentTable);
        }

        void SerialiseMesh(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
        {
            if (!entity.HasComponent<Wayfinder::MeshComponent>())
            {
                return;
            }

            const auto& mesh = entity.GetComponent<Wayfinder::MeshComponent>();
            nlohmann::json componentTable;
            componentTable["primitive"] = std::string{ToString(mesh.Primitive)};
            componentTable["dimensions"] = WriteVector3(mesh.Dimensions);
            if (mesh.MeshAssetId)
            {
                componentTable["mesh_id"] = mesh.MeshAssetId->ToString();
            }
            if (!mesh.MaterialSlotBindings.empty())
            {
                nlohmann::json slotsJson = nlohmann::json::object();
                for (const auto& [slot, assetId] : mesh.MaterialSlotBindings)
                {
                    slotsJson[std::to_string(slot)] = assetId.ToString();
                }
                componentTable["material_slots"] = std::move(slotsJson);
            }
            componentTables["mesh"] = std::move(componentTable);
        }

        void SerialiseCamera(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
        {
            if (!entity.HasComponent<Wayfinder::CameraComponent>())
            {
                return;
            }

            const auto& camera = entity.GetComponent<Wayfinder::CameraComponent>();
            nlohmann::json componentTable;
            componentTable["primary"] = camera.Primary;
            componentTable["target"] = WriteVector3(camera.Target);
            componentTable["up"] = WriteVector3(camera.Up);
            componentTable["fov"] = camera.FieldOfView;
            componentTable["projection"] = std::string{ToString(camera.Projection)};
            componentTables["camera"] = std::move(componentTable);
        }

        void SerialiseLight(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
        {
            if (!entity.HasComponent<Wayfinder::LightComponent>())
            {
                return;
            }

            const auto& light = entity.GetComponent<Wayfinder::LightComponent>();
            nlohmann::json componentTable;
            componentTable["type"] = std::string{ToString(light.Type)};
            componentTable["colour"] = WriteColour(light.Tint);
            componentTable["intensity"] = light.Intensity;
            componentTable["range"] = light.Range;
            componentTable["debug_draw"] = light.DebugDraw;
            componentTables["light"] = std::move(componentTable);
        }

        void SerialiseMaterial(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
        {
            if (!entity.HasComponent<Wayfinder::MaterialComponent>())
            {
                return;
            }

            const auto& material = entity.GetComponent<Wayfinder::MaterialComponent>();
            nlohmann::json componentTable;
            if (material.MaterialAssetId)
            {
                componentTable["material_id"] = material.MaterialAssetId->ToString();
            }

            if (!material.MaterialAssetId || material.HasBaseColourOverride)
            {
                componentTable["base_colour"] = WriteColour(material.BaseColour);
            }

            componentTables["material"] = std::move(componentTable);
        }

        void SerialiseRenderOverride(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
        {
            if (!entity.HasComponent<Wayfinder::RenderOverrideComponent>())
            {
                return;
            }

            const auto& renderOverride = entity.GetComponent<Wayfinder::RenderOverrideComponent>();
            nlohmann::json componentTable;
            if (renderOverride.Wireframe.has_value())
            {
                componentTable["wireframe"] = *renderOverride.Wireframe;
            }

            if (!componentTable.empty())
            {
                componentTables["render_override"] = std::move(componentTable);
            }
        }

        void SerialiseRenderable(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
        {
            if (!entity.HasComponent<Wayfinder::RenderableComponent>())
            {
                return;
            }

            const auto& renderable = entity.GetComponent<Wayfinder::RenderableComponent>();
            nlohmann::json componentTable;
            componentTable["visible"] = renderable.Visible;
            componentTable["layer"] = renderable.Layer.GetString();
            componentTable["sort_priority"] = static_cast<int64_t>(renderable.SortPriority);
            componentTables["renderable"] = std::move(componentTable);
        }

        void SerialisePostProcessVolume(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
        {
            if (!entity.HasComponent<Wayfinder::PostProcessVolumeComponent>())
            {
                return;
            }

            const auto& volume = entity.GetComponent<Wayfinder::PostProcessVolumeComponent>();
            nlohmann::json componentTable;
            componentTable["shape"] = std::string{ToString(volume.Shape)};
            componentTable["priority"] = static_cast<int64_t>(volume.Priority);
            componentTable["blend_distance"] = volume.BlendDistance;
            componentTable["dimensions"] = WriteVector3(volume.Dimensions);
            componentTable["radius"] = volume.Radius;

            if (!volume.Effects.empty())
            {
                nlohmann::json effectsArray = nlohmann::json::array();
                for (const auto& effect : volume.Effects)
                {
                    const Wayfinder::PostProcessRegistry* registry = Wayfinder::PostProcessRegistry::GetActiveInstance();
                    if (registry == nullptr)
                    {
                        break;
                    }
                    const Wayfinder::PostProcessEffectDesc* desc = registry->Find(effect.TypeId);
                    if (desc == nullptr || desc->Serialise == nullptr)
                    {
                        continue;
                    }

                    nlohmann::json effectTable;
                    effectTable["type"] = std::string{desc->Name};
                    if (!effect.Enabled)
                    {
                        effectTable["enabled"] = false;
                    }
                    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                    desc->Serialise(effectTable, effect.Payload);
                    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                    effectsArray.push_back(std::move(effectTable));
                }

                componentTable["effects"] = std::move(effectsArray);
            }

            componentTables["post_process_volume"] = std::move(componentTable);
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

        constexpr std::array<Wayfinder::SceneComponentRegistry::Entry, 9> K_ENTRIES = {{
            {.Key = "transform", .RegisterFn = &RegisterComponent<Wayfinder::TransformComponent>, .ApplyFn = &ApplyTransform, .SerialiseFn = &SerialiseTransform, .ValidateFn = &ValidateTransform},
            {.Key = "mesh", .RegisterFn = &RegisterComponent<Wayfinder::MeshComponent>, .ApplyFn = &ApplyMesh, .SerialiseFn = &SerialiseMesh, .ValidateFn = &ValidateMesh},
            {.Key = "camera", .RegisterFn = &RegisterComponent<Wayfinder::CameraComponent>, .ApplyFn = &ApplyCamera, .SerialiseFn = &SerialiseCamera, .ValidateFn = &ValidateCamera},
            {.Key = "light", .RegisterFn = &RegisterComponent<Wayfinder::LightComponent>, .ApplyFn = &ApplyLight, .SerialiseFn = &SerialiseLight, .ValidateFn = &ValidateLight},
            {.Key = "material", .RegisterFn = &RegisterComponent<Wayfinder::MaterialComponent>, .ApplyFn = &ApplyMaterial, .SerialiseFn = &SerialiseMaterial, .ValidateFn = &ValidateMaterial},
            {.Key = "renderable", .RegisterFn = &RegisterComponent<Wayfinder::RenderableComponent>, .ApplyFn = &ApplyRenderable, .SerialiseFn = &SerialiseRenderable, .ValidateFn = &ValidateRenderable},
            {.Key = "render_override",
                .RegisterFn = &RegisterComponent<Wayfinder::RenderOverrideComponent>,
                .ApplyFn = &ApplyRenderOverride,
                .SerialiseFn = &SerialiseRenderOverride,
                .ValidateFn = &ValidateRenderOverride},
            {.Key = "gameplay_tags", .RegisterFn = &RegisterComponent<Wayfinder::GameplayTagContainer>, .ApplyFn = &ApplyTags, .SerialiseFn = &SerialiseTags, .ValidateFn = &ValidateTags},
            {.Key = "post_process_volume",
                .RegisterFn = &RegisterComponent<Wayfinder::PostProcessVolumeComponent>,
                .ApplyFn = &ApplyPostProcessVolume,
                .SerialiseFn = &SerialisePostProcessVolume,
                .ValidateFn = &ValidatePostProcessVolume},
        }};
    } // anonymous namespace
}

namespace Wayfinder
{
    const SceneComponentRegistry& SceneComponentRegistry::Get()
    {
        static const SceneComponentRegistry REGISTRY;
        return REGISTRY;
    }

    std::span<const SceneComponentRegistry::Entry> SceneComponentRegistry::GetEntries()
    {
        return K_ENTRIES;
    }

    void SceneComponentRegistry::RegisterComponents(flecs::world& world) const
    {
        for (const Entry& entry : K_ENTRIES)
        {
            entry.RegisterFn(world);
        }
    }

    void SceneComponentRegistry::ApplyComponents(const nlohmann::json& componentTables, Entity& entity) const
    {
        for (const auto& [key, node] : componentTables.items())
        {
            const Entry* entry = Find(key);
            if (!entry || !entry->ApplyFn)
            {
                continue;
            }

            if (!node.is_object())
            {
                continue;
            }

            entry->ApplyFn(node, entity);
        }
    }

    void SceneComponentRegistry::SerialiseComponents(const Entity& entity, nlohmann::json& componentTables) const
    {
        for (const Entry& entry : K_ENTRIES)
        {
            if (entry.SerialiseFn)
            {
                entry.SerialiseFn(entity, componentTables);
            }
        }
    }

    bool SceneComponentRegistry::ValidateComponent(std::string_view key, const nlohmann::json& componentData, std::string& error) const
    {
        const Entry* entry = Find(key);
        if (!entry || !entry->ValidateFn)
        {
            error = "component is not registered for scene authoring";
            return false;
        }

        return entry->ValidateFn(componentData, error);
    }

    bool SceneComponentRegistry::IsRegistered(std::string_view key) const
    {
        const Entry* entry = Find(key);
        return entry != nullptr && entry->ApplyFn != nullptr && entry->ValidateFn != nullptr;
    }

    const SceneComponentRegistry::Entry* SceneComponentRegistry::Find(std::string_view key) const
    {
        for (const Entry& entry : K_ENTRIES)
        {
            if (entry.Key == key)
            {
                return &entry;
            }
        }

        return nullptr;
    }
} // namespace Wayfinder
