#include "ComponentRegistry.h"

#include "Components.h"
#include "scene/entity/Entity.h"
#include "core/GameplayTag.h"
#include "core/GameplayTagRegistry.h"
#include "core/Log.h"
#include "core/Subsystem.h"

#include <array>
#include <sstream>

namespace
{
    std::optional<Wayfinder::AssetId> ReadOptionalAssetId(const nlohmann::json& data, const char* key)
    {
        if (!data.contains(key) || !data[key].is_string())
        {
            return std::nullopt;
        }

        return Wayfinder::AssetId::Parse(data[key].get<std::string>());
    }

    template <typename T>
    void RegisterComponent(flecs::world& world)
    {
        world.component<T>();
    }

    float ReadFloat(const nlohmann::json& data, const char* key, float fallback)
    {
        if (!data.contains(key)) return fallback;
        const auto& val = data[key];
        if (val.is_number()) return val.get<float>();
        return fallback;
    }

    float ReadArrayFloat(const nlohmann::json& arr, size_t index, float fallback)
    {
        if (index >= arr.size()) return fallback;
        const auto& val = arr[index];
        if (val.is_number()) return val.get<float>();
        return fallback;
    }

    Wayfinder::Float3 ReadVector3(const nlohmann::json& data, const char* key, const Wayfinder::Float3& fallback)
    {
        if (!data.contains(key)) return fallback;
        const auto& arr = data[key];
        if (!arr.is_array() || arr.size() != 3) return fallback;

        Wayfinder::Float3 result = fallback;
        result.x = ReadArrayFloat(arr, 0, result.x);
        result.y = ReadArrayFloat(arr, 1, result.y);
        result.z = ReadArrayFloat(arr, 2, result.z);
        return result;
    }

    nlohmann::json WriteVector3(const Wayfinder::Float3& value)
    {
        return nlohmann::json::array({value.x, value.y, value.z});
    }

    Wayfinder::Color ReadColor(const nlohmann::json& data, const char* key, const Wayfinder::Color& fallback)
    {
        if (!data.contains(key)) return fallback;
        const auto& arr = data[key];
        if (!arr.is_array() || (arr.size() != 3 && arr.size() != 4)) return fallback;

        Wayfinder::Color result = fallback;
        result.r = arr[0].is_number_integer() ? static_cast<uint8_t>(arr[0].get<int64_t>()) : result.r;
        result.g = arr[1].is_number_integer() ? static_cast<uint8_t>(arr[1].get<int64_t>()) : result.g;
        result.b = arr[2].is_number_integer() ? static_cast<uint8_t>(arr[2].get<int64_t>()) : result.b;
        result.a = (arr.size() == 4 && arr[3].is_number_integer()) ? static_cast<uint8_t>(arr[3].get<int64_t>()) : result.a;
        return result;
    }

    nlohmann::json WriteColor(const Wayfinder::Color& value)
    {
        return nlohmann::json::array({
            static_cast<int64_t>(value.r),
            static_cast<int64_t>(value.g),
            static_cast<int64_t>(value.b),
            static_cast<int64_t>(value.a)
        });
    }

    const char* ToString(Wayfinder::MeshPrimitive primitive)
    {
        switch (primitive)
        {
        case Wayfinder::MeshPrimitive::Cube:
            return "cube";
        }

        return "cube";
    }

    const char* ToString(Wayfinder::ProjectionMode projection)
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

    const char* ToString(Wayfinder::LightType type)
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

    Wayfinder::InternedString ReadRenderLayer(const nlohmann::json& data, const char* key, const Wayfinder::InternedString& fallback)
    {
        if (!data.contains(key) || !data[key].is_string())
        {
            return fallback;
        }

        const auto layer = data[key].get<std::string>();
        if (layer.empty())
        {
            return fallback;
        }

        return Wayfinder::InternedString::Intern(layer);
    }

    Wayfinder::MeshPrimitive ReadPrimitive(const nlohmann::json& data, const char* key, Wayfinder::MeshPrimitive fallback)
    {
        if (!data.contains(key) || !data[key].is_string())
        {
            return fallback;
        }

        const auto primitive = data[key].get<std::string>();
        if (primitive == "cube")
        {
            return Wayfinder::MeshPrimitive::Cube;
        }

        return fallback;
    }

    Wayfinder::ProjectionMode ReadProjection(const nlohmann::json& data, const char* key, Wayfinder::ProjectionMode fallback)
    {
        if (!data.contains(key) || !data[key].is_string())
        {
            return fallback;
        }

        const auto projection = data[key].get<std::string>();
        if (projection == "orthographic")
        {
            return Wayfinder::ProjectionMode::Orthographic;
        }

        return fallback;
    }

    Wayfinder::LightType ReadLightType(const nlohmann::json& data, const char* key, Wayfinder::LightType fallback)
    {
        if (!data.contains(key) || !data[key].is_string())
        {
            return fallback;
        }

        const auto type = data[key].get<std::string>();
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

    bool ValidateOptionalNonEmptyString(const nlohmann::json& data, const char* key, std::string& error)
    {
        if (!data.contains(key))
        {
            return true;
        }

        const auto& node = data[key];
        if (!node.is_string())
        {
            error = std::string("'") + key + "' must be a string";
            return false;
        }

        if (node.get<std::string>().empty())
        {
            error = std::string("'") + key + "' must not be empty";
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

    bool ValidateOptionalBool(const nlohmann::json& data, const char* key, std::string& error)
    {
        if (!data.contains(key))
        {
            return true;
        }

        if (!data[key].is_boolean())
        {
            error = std::string{"field '"} + key + "' must be a boolean";
            return false;
        }

        return true;
    }

    bool ValidateOptionalAssetId(const nlohmann::json& data, const char* key, std::string& error)
    {
        if (!data.contains(key))
        {
            return true;
        }

        const auto& node = data[key];
        if (!node.is_string())
        {
            error = std::string{"field '"} + key + "' must be a UUID string";
            return false;
        }

        const auto assetId = Wayfinder::AssetId::Parse(node.get<std::string>());
        if (!assetId)
        {
            error = std::string{"field '"} + key + "' must be a valid UUID";
            return false;
        }

        return true;
    }

    bool ValidateOptionalNumber(const nlohmann::json& data, const char* key, std::string& error)
    {
        if (!data.contains(key))
        {
            return true;
        }

        if (!IsNumberNode(data[key]))
        {
            error = std::string{"field '"} + key + "' must be numeric";
            return false;
        }

        return true;
    }

    bool ValidateOptionalInteger(const nlohmann::json& data, const char* key, std::string& error)
    {
        if (!data.contains(key)) { return true; }

        if (!data[key].is_number_integer())
        {
            error = std::string{"field '"} + key + "' must be an integer";
            return false;
        }

        return true;
    }

    bool ValidateOptionalVector3(const nlohmann::json& data, const char* key, std::string& error)
    {
        if (!data.contains(key))
        {
            return true;
        }

        const auto& node = data[key];
        if (!node.is_array() || node.size() != 3)
        {
            error = std::string{"field '"} + key + "' must be an array of 3 numbers";
            return false;
        }

        for (size_t index = 0; index < node.size(); ++index)
        {
            if (!IsNumberNode(node[index]))
            {
                error = std::string{"field '"} + key + "' must be an array of 3 numbers";
                return false;
            }
        }

        return true;
    }

    bool ValidateOptionalColor(const nlohmann::json& data, const char* key, std::string& error)
    {
        if (!data.contains(key))
        {
            return true;
        }

        const auto& node = data[key];
        if (!node.is_array() || (node.size() != 3 && node.size() != 4))
        {
            error = std::string{"field '"} + key + "' must be an array of 3 or 4 integers";
            return false;
        }

        for (size_t index = 0; index < node.size(); ++index)
        {
            if (!node[index].is_number_integer())
            {
                error = std::string{"field '"} + key + "' must be an array of 3 or 4 integers";
                return false;
            }
        }

        return true;
    }

    bool ValidateOptionalEnumValue(
        const nlohmann::json& data,
        const char* key,
        std::initializer_list<std::string_view> acceptedValues,
        std::string& error)
    {
        if (!data.contains(key))
        {
            return true;
        }

        const auto& node = data[key];
        if (!node.is_string())
        {
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
        return ValidateOptionalVector3(data, "position", error)
            && ValidateOptionalVector3(data, "rotation", error)
            && ValidateOptionalVector3(data, "scale", error);
    }

    bool ValidateMesh(const nlohmann::json& data, std::string& error)
    {
        return ValidateOptionalEnumValue(data, "primitive", {"cube"}, error)
            && ValidateOptionalVector3(data, "dimensions", error);
    }

    bool ValidateCamera(const nlohmann::json& data, std::string& error)
    {
        return ValidateOptionalBool(data, "primary", error)
            && ValidateOptionalVector3(data, "target", error)
            && ValidateOptionalVector3(data, "up", error)
            && ValidateOptionalNumber(data, "fov", error)
            && ValidateOptionalEnumValue(data, "projection", {"perspective", "orthographic"}, error);
    }

    bool ValidateLight(const nlohmann::json& data, std::string& error)
    {
        return ValidateOptionalEnumValue(data, "type", {"point", "directional"}, error)
            && ValidateOptionalColor(data, "color", error)
            && ValidateOptionalNumber(data, "intensity", error)
            && ValidateOptionalNumber(data, "range", error)
            && ValidateOptionalBool(data, "debug_draw", error);
    }

    bool ValidateMaterial(const nlohmann::json& data, std::string& error)
    {
        return ValidateOptionalAssetId(data, "material_id", error)
            && ValidateOptionalColor(data, "base_color", error)
            && ValidateOptionalBool(data, "wireframe", error);
    }

    bool ValidateRenderable(const nlohmann::json& data, std::string& error)
    {
        return ValidateOptionalBool(data, "visible", error)
            && ValidateOptionalNonEmptyString(data, "layer", error)
            && ValidateOptionalInteger(data, "sort_priority", error);
    }

    bool ValidateEffectParameter(std::string_view key, const nlohmann::json& node, std::string& error)
    {
        if (node.is_number_integer() || node.is_number_float()) { return true; }

        if (node.is_array())
        {
            if (node.size() < 3 || node.size() > 4)
            {
                error = std::string("effect parameter '") + std::string(key) + "' array must have 3 or 4 elements";
                return false;
            }

            bool allInts = true;
            for (size_t i = 0; i < node.size(); ++i)
            {
                if (!node[i].is_number_integer() && !node[i].is_number_float())
                {
                    error = std::string("effect parameter '") + std::string(key) + "' array elements must be numbers";
                    return false;
                }
                if (!node[i].is_number_integer()) { allInts = false; }
            }

            // 4-element arrays are only valid as Color (all integers r,g,b,a).
            // Float3 only reads 3 elements, so a 4-element float array would silently lose data.
            if (node.size() == 4 && !allInts)
            {
                error = std::string("effect parameter '") + std::string(key)
                    + "' 4-element arrays must be all integers (Color r,g,b,a)";
                return false;
            }

            // 3-element all-integer arrays are ambiguous: ReadEffectParam treats them
            // as Color (r,g,b with a=255), not Float3. Require at least one float
            // for Float3 values (e.g. [1.0, 2.0, 3.0]).
            if (node.size() == 3 && allInts)
            {
                error = std::string("effect parameter '") + std::string(key)
                    + "' 3-element all-integer arrays are interpreted as Color, not Float3; use floats for Float3 (e.g. [1.0, 2.0, 3.0])";
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
            return false;

        if (!ValidateOptionalInteger(data, "priority", error))
            return false;

        if (!ValidateOptionalNumber(data, "blend_distance", error))
            return false;

        if (!ValidateOptionalVector3(data, "dimensions", error))
            return false;

        if (!ValidateOptionalNumber(data, "radius", error))
            return false;

        // Validate the effects array if present
        if (data.contains("effects"))
        {
            const auto& effectsNode = data["effects"];
            if (!effectsNode.is_array())
            {
                error = "'effects' must be an array of tables";
                return false;
            }

            for (size_t i = 0; i < effectsNode.size(); ++i)
            {
                const auto& effectEntry = effectsNode[i];
                if (!effectEntry.is_object())
                {
                    error = "each entry in 'effects' must be a table";
                    return false;
                }

                if (!effectEntry.contains("type") || !effectEntry["type"].is_string()
                    || effectEntry["type"].get<std::string>().empty())
                {
                    error = "each effect must have a non-empty 'type' string";
                    return false;
                }

                for (const auto& [key, value] : effectEntry.items())
                {
                    if (key == "type") { continue; }
                    if (key == "enabled")
                    {
                        if (!value.is_boolean())
                        {
                            error = "effect 'enabled' must be a boolean";
                            return false;
                        }
                        continue;
                    }
                    if (!ValidateEffectParameter(key, value, error)) { return false; }
                }
            }
        }

        return true;
    }

    void ApplyTransform(const nlohmann::json& data, Wayfinder::Entity& entity)
    {
        Wayfinder::TransformComponent transform;
        transform.Position = ReadVector3(data, "position", transform.Position);
        transform.Rotation = ReadVector3(data, "rotation", transform.Rotation);
        transform.Scale = ReadVector3(data, "scale", transform.Scale);
        entity.AddComponent<Wayfinder::TransformComponent>(transform);
    }

    void ApplyMesh(const nlohmann::json& data, Wayfinder::Entity& entity)
    {
        Wayfinder::MeshComponent mesh;
        mesh.Primitive = ReadPrimitive(data, "primitive", mesh.Primitive);
        mesh.Dimensions = ReadVector3(data, "dimensions", mesh.Dimensions);
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
        light.Tint = ReadColor(data, "color", light.Tint);
        light.Intensity = ReadFloat(data, "intensity", light.Intensity);
        light.Range = ReadFloat(data, "range", light.Range);
        light.DebugDraw = data.value("debug_draw", light.DebugDraw);
        entity.AddComponent<Wayfinder::LightComponent>(light);
    }

    void ApplyMaterial(const nlohmann::json& data, Wayfinder::Entity& entity)
    {
        Wayfinder::MaterialComponent material;
        material.MaterialAssetId = ReadOptionalAssetId(data, "material_id");
        material.HasBaseColorOverride = data.contains("base_color");
        material.HasWireframeOverride = data.contains("wireframe");
        if (material.HasBaseColorOverride)
        {
            material.BaseColor = ReadColor(data, "base_color", material.BaseColor);
        }

        if (material.HasWireframeOverride)
        {
            material.Wireframe = data.value("wireframe", material.Wireframe);
        }

        entity.AddComponent<Wayfinder::MaterialComponent>(material);
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

    Wayfinder::PostProcessVolumeShape ReadVolumeShape(const nlohmann::json& data, const char* key, Wayfinder::PostProcessVolumeShape fallback)
    {
        if (!data.contains(key) || !data[key].is_string()) return fallback;
        const auto value = data[key].get<std::string>();
        if (value == "global") return Wayfinder::PostProcessVolumeShape::Global;
        if (value == "box") return Wayfinder::PostProcessVolumeShape::Box;
        if (value == "sphere") return Wayfinder::PostProcessVolumeShape::Sphere;
        return fallback;
    }

    // ── GameplayTagContainer ────────────────────────────────

    bool ValidateTags(const nlohmann::json& data, std::string& error)
    {
        if (!data.contains("tags"))
            return true;

        const auto& node = data["tags"];
        if (!node.is_array())
        {
            error = "'tags' must be an array of strings";
            return false;
        }

        for (size_t i = 0; i < node.size(); ++i)
        {
            if (!node[i].is_string())
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
            if (data.contains("tags") && data["tags"].is_array())
            {
                for (const auto& node : data["tags"])
                {
                    if (node.is_string())
                        container.AddTag(registry->RequestTag(node.get<std::string>()));
                }
            }
        }
        else if (data.contains("tags") && data["tags"].is_array() && !data["tags"].empty())
        {
            Wayfinder::LogScene.GetLogger()->LogFormat(
                Wayfinder::LogVerbosity::Warning,
                "Entity specifies {0} tag(s) but no GameplayTagRegistry is available — tags will be ignored.",
                data["tags"].size());
        }
        entity.AddComponent<Wayfinder::GameplayTagContainer>(container);
    }

    void SerializeTags(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::GameplayTagContainer>())
            return;

        const auto& container = entity.GetComponent<Wayfinder::GameplayTagContainer>();
        if (container.IsEmpty())
            return;

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& tag : container)
            arr.push_back(tag.GetName());

        nlohmann::json t;
        t["tags"] = std::move(arr);
        componentTables["gameplay_tags"] = std::move(t);
    }

    // ── PostProcessVolumeComponent ──────────────────────────

    const char* ToString(Wayfinder::PostProcessVolumeShape shape)
    {
        switch (shape)
        {
        case Wayfinder::PostProcessVolumeShape::Global: return "global";
        case Wayfinder::PostProcessVolumeShape::Box: return "box";
        case Wayfinder::PostProcessVolumeShape::Sphere: return "sphere";
        }
        return "global";
    }

    Wayfinder::PostProcessParamValue ReadEffectParam(const nlohmann::json& node)
    {
        if (node.is_number_float()) return node.get<float>();
        if (node.is_number_integer()) return static_cast<int32_t>(node.get<int64_t>());

        if (node.is_array())
        {
            // All-integer arrays → Color; otherwise → Float3
            bool allInts = true;
            for (size_t i = 0; i < node.size(); ++i)
            {
                if (!node[i].is_number_integer()) { allInts = false; break; }
            }

            if (allInts)
            {
                Wayfinder::Color c;
                c.r = node[0].is_number_integer() ? static_cast<uint8_t>(node[0].get<int64_t>()) : 0;
                c.g = node[1].is_number_integer() ? static_cast<uint8_t>(node[1].get<int64_t>()) : 0;
                c.b = node[2].is_number_integer() ? static_cast<uint8_t>(node[2].get<int64_t>()) : 0;
                c.a = (node.size() >= 4 && node[3].is_number_integer()) ? static_cast<uint8_t>(node[3].get<int64_t>()) : 255;
                return c;
            }

            if (node.size() >= 3)
            {
                return Wayfinder::Float3{
                    ReadArrayFloat(node, 0, 0.0f),
                    ReadArrayFloat(node, 1, 0.0f),
                    ReadArrayFloat(node, 2, 0.0f)};
            }
        }

        return 0.0f;
    }

    Wayfinder::PostProcessEffect ReadEffect(const nlohmann::json& effectData)
    {
        Wayfinder::PostProcessEffect effect;
        effect.Type = effectData.value("type", std::string{});
        effect.Enabled = effectData.value("enabled", true);

        for (const auto& [key, value] : effectData.items())
        {
            if (key == "type" || key == "enabled") { continue; }
            effect.Parameters[key] = ReadEffectParam(value);
        }

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

        if (data.contains("effects") && data["effects"].is_array())
        {
            const auto& effectsArray = data["effects"];
            for (size_t i = 0; i < effectsArray.size(); ++i)
            {
                if (effectsArray[i].is_object())
                {
                    volume.Effects.push_back(ReadEffect(effectsArray[i]));
                }
            }
        }

        entity.AddComponent<Wayfinder::PostProcessVolumeComponent>(volume);
    }

    void SerializeTransform(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::TransformComponent>())
        {
            return;
        }

        const Wayfinder::TransformComponent& transform = entity.GetComponent<Wayfinder::TransformComponent>();
        nlohmann::json componentTable;
        componentTable["position"] = WriteVector3(transform.Position);
        componentTable["rotation"] = WriteVector3(transform.Rotation);
        componentTable["scale"] = WriteVector3(transform.Scale);
        componentTables["transform"] = std::move(componentTable);
    }

    void SerializeMesh(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::MeshComponent>())
        {
            return;
        }

        const Wayfinder::MeshComponent& mesh = entity.GetComponent<Wayfinder::MeshComponent>();
        nlohmann::json componentTable;
        componentTable["primitive"] = std::string{ToString(mesh.Primitive)};
        componentTable["dimensions"] = WriteVector3(mesh.Dimensions);
        componentTables["mesh"] = std::move(componentTable);
    }

    void SerializeCamera(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::CameraComponent>())
        {
            return;
        }

        const Wayfinder::CameraComponent& camera = entity.GetComponent<Wayfinder::CameraComponent>();
        nlohmann::json componentTable;
        componentTable["primary"] = camera.Primary;
        componentTable["target"] = WriteVector3(camera.Target);
        componentTable["up"] = WriteVector3(camera.Up);
        componentTable["fov"] = camera.FieldOfView;
        componentTable["projection"] = std::string{ToString(camera.Projection)};
        componentTables["camera"] = std::move(componentTable);
    }

    void SerializeLight(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::LightComponent>())
        {
            return;
        }

        const Wayfinder::LightComponent& light = entity.GetComponent<Wayfinder::LightComponent>();
        nlohmann::json componentTable;
        componentTable["type"] = std::string{ToString(light.Type)};
        componentTable["color"] = WriteColor(light.Tint);
        componentTable["intensity"] = light.Intensity;
        componentTable["range"] = light.Range;
        componentTable["debug_draw"] = light.DebugDraw;
        componentTables["light"] = std::move(componentTable);
    }

    void SerializeMaterial(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::MaterialComponent>())
        {
            return;
        }

        const Wayfinder::MaterialComponent& material = entity.GetComponent<Wayfinder::MaterialComponent>();
        nlohmann::json componentTable;
        if (material.MaterialAssetId)
        {
            componentTable["material_id"] = material.MaterialAssetId->ToString();
        }

        if (!material.MaterialAssetId || material.HasBaseColorOverride)
        {
            componentTable["base_color"] = WriteColor(material.BaseColor);
        }

        if (!material.MaterialAssetId || material.HasWireframeOverride)
        {
            componentTable["wireframe"] = material.Wireframe;
        }

        componentTables["material"] = std::move(componentTable);
    }

    void SerializeRenderable(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::RenderableComponent>())
        {
            return;
        }

        const Wayfinder::RenderableComponent& renderable = entity.GetComponent<Wayfinder::RenderableComponent>();
        nlohmann::json componentTable;
        componentTable["visible"] = renderable.Visible;
        componentTable["layer"] = renderable.Layer.GetString();
        componentTable["sort_priority"] = static_cast<int64_t>(renderable.SortPriority);
        componentTables["renderable"] = std::move(componentTable);
    }

    void WriteEffectParam(nlohmann::json& obj, const std::string& key, const Wayfinder::PostProcessParamValue& value)
    {
        std::visit([&](const auto& v)
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, float>)
                obj[key] = v;
            else if constexpr (std::is_same_v<T, int32_t>)
                obj[key] = static_cast<int64_t>(v);
            else if constexpr (std::is_same_v<T, Wayfinder::Float3>)
                obj[key] = WriteVector3(v);
            else if constexpr (std::is_same_v<T, Wayfinder::Color>)
                obj[key] = WriteColor(v);
        }, value);
    }

    void SerializePostProcessVolume(const Wayfinder::Entity& entity, nlohmann::json& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::PostProcessVolumeComponent>())
        {
            return;
        }

        const Wayfinder::PostProcessVolumeComponent& volume = entity.GetComponent<Wayfinder::PostProcessVolumeComponent>();
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
                nlohmann::json effectTable;
                effectTable["type"] = effect.Type;
                if (!effect.Enabled) { effectTable["enabled"] = false; }

                for (const auto& [key, value] : effect.Parameters)
                {
                    WriteEffectParam(effectTable, key, value);
                }

                effectsArray.push_back(std::move(effectTable));
            }

            componentTable["effects"] = std::move(effectsArray);
        }

        componentTables["post_process_volume"] = std::move(componentTable);
    }

    constexpr std::array<Wayfinder::SceneComponentRegistry::Entry, 8> kEntries = {{
        {"transform", &RegisterComponent<Wayfinder::TransformComponent>, &ApplyTransform, &SerializeTransform, &ValidateTransform},
        {"mesh", &RegisterComponent<Wayfinder::MeshComponent>, &ApplyMesh, &SerializeMesh, &ValidateMesh},
        {"camera", &RegisterComponent<Wayfinder::CameraComponent>, &ApplyCamera, &SerializeCamera, &ValidateCamera},
        {"light", &RegisterComponent<Wayfinder::LightComponent>, &ApplyLight, &SerializeLight, &ValidateLight},
        {"material", &RegisterComponent<Wayfinder::MaterialComponent>, &ApplyMaterial, &SerializeMaterial, &ValidateMaterial},
        {"renderable", &RegisterComponent<Wayfinder::RenderableComponent>, &ApplyRenderable, &SerializeRenderable, &ValidateRenderable},
        {"gameplay_tags", &RegisterComponent<Wayfinder::GameplayTagContainer>, &ApplyTags, &SerializeTags, &ValidateTags},
        {"post_process_volume", &RegisterComponent<Wayfinder::PostProcessVolumeComponent>, &ApplyPostProcessVolume, &SerializePostProcessVolume, &ValidatePostProcessVolume},
    }};
}

namespace Wayfinder
{
    const SceneComponentRegistry& SceneComponentRegistry::Get()
    {
        static const SceneComponentRegistry registry;
        return registry;
    }

    std::span<const SceneComponentRegistry::Entry> SceneComponentRegistry::GetEntries()
    {
        return kEntries;
    }

    void SceneComponentRegistry::RegisterComponents(flecs::world& world) const
    {
        for (const Entry& entry : kEntries)
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

    void SceneComponentRegistry::SerializeComponents(const Entity& entity, nlohmann::json& componentTables) const
    {
        for (const Entry& entry : kEntries)
        {
            if (entry.SerializeFn)
            {
                entry.SerializeFn(entity, componentTables);
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
        for (const Entry& entry : kEntries)
        {
            if (entry.Key == key)
            {
                return &entry;
            }
        }

        return nullptr;
    }
} // namespace Wayfinder