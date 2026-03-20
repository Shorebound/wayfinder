#include "ComponentRegistry.h"

#include "Components.h"
#include "entity/Entity.h"
#include "../core/GameplayTag.h"
#include "../core/GameplayTagRegistry.h"
#include "../core/Log.h"
#include "../core/Subsystem.h"

#include <array>
#include <sstream>

namespace
{
    std::optional<Wayfinder::AssetId> ReadOptionalAssetId(const toml::table& table, const char* key)
    {
        const auto value = table[key].value<std::string>();
        if (!value)
        {
            return std::nullopt;
        }

        return Wayfinder::AssetId::Parse(*value);
    }

    template <typename T>
    void RegisterComponent(flecs::world& world)
    {
        world.component<T>();
    }

    float ReadFloat(const toml::table& table, const char* key, float fallback)
    {
        if (const auto value = table[key].value<double>())
        {
            return static_cast<float>(*value);
        }
        if (const auto value = table[key].value<int64_t>())
        {
            return static_cast<float>(*value);
        }
        return fallback;
    }

    float ReadArrayFloat(const toml::array& array, size_t index, float fallback)
    {
        if (index >= array.size())
        {
            return fallback;
        }

        if (const auto value = array[index].value<double>())
        {
            return static_cast<float>(*value);
        }
        if (const auto value = array[index].value<int64_t>())
        {
            return static_cast<float>(*value);
        }

        return fallback;
    }

    Wayfinder::Float3 ReadVector3(const toml::table& table, const char* key, const Wayfinder::Float3& fallback)
    {
        const toml::array* values = table[key].as_array();
        if (!values || values->size() != 3)
        {
            return fallback;
        }

        Wayfinder::Float3 result = fallback;
        result.x = ReadArrayFloat(*values, 0, result.x);
        result.y = ReadArrayFloat(*values, 1, result.y);
        result.z = ReadArrayFloat(*values, 2, result.z);
        return result;
    }

    toml::array WriteVector3(const Wayfinder::Float3& value)
    {
        toml::array result;
        result.push_back(value.x);
        result.push_back(value.y);
        result.push_back(value.z);
        return result;
    }

    Wayfinder::Color ReadColor(const toml::table& table, const char* key, const Wayfinder::Color& fallback)
    {
        const toml::array* values = table[key].as_array();
        if (!values || (values->size() != 3 && values->size() != 4))
        {
            return fallback;
        }

        Wayfinder::Color result = fallback;
        result.r = static_cast<uint8_t>(values->get(0)->value_or(static_cast<int64_t>(result.r)));
        result.g = static_cast<uint8_t>(values->get(1)->value_or(static_cast<int64_t>(result.g)));
        result.b = static_cast<uint8_t>(values->get(2)->value_or(static_cast<int64_t>(result.b)));
        result.a = static_cast<uint8_t>(values->size() == 4 ? values->get(3)->value_or(static_cast<int64_t>(result.a)) : result.a);
        return result;
    }

    toml::array WriteColor(const Wayfinder::Color& value)
    {
        toml::array result;
        result.push_back(static_cast<int64_t>(value.r));
        result.push_back(static_cast<int64_t>(value.g));
        result.push_back(static_cast<int64_t>(value.b));
        result.push_back(static_cast<int64_t>(value.a));
        return result;
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

    Wayfinder::InternedString ReadRenderLayer(const toml::table& table, const char* key, const Wayfinder::InternedString& fallback)
    {
        const auto layer = table[key].value<std::string>();
        if (!layer || layer->empty())
        {
            return fallback;
        }

        return Wayfinder::InternedString::Intern(*layer);
    }

    Wayfinder::MeshPrimitive ReadPrimitive(const toml::table& table, const char* key, Wayfinder::MeshPrimitive fallback)
    {
        const auto primitive = table[key].value<std::string>();
        if (!primitive)
        {
            return fallback;
        }

        if (*primitive == "cube")
        {
            return Wayfinder::MeshPrimitive::Cube;
        }

        return fallback;
    }

    Wayfinder::ProjectionMode ReadProjection(const toml::table& table, const char* key, Wayfinder::ProjectionMode fallback)
    {
        const auto projection = table[key].value<std::string>();
        if (!projection)
        {
            return fallback;
        }

        if (*projection == "orthographic")
        {
            return Wayfinder::ProjectionMode::Orthographic;
        }

        return fallback;
    }

    Wayfinder::LightType ReadLightType(const toml::table& table, const char* key, Wayfinder::LightType fallback)
    {
        const auto type = table[key].value<std::string>();
        if (!type)
        {
            return fallback;
        }

        if (*type == "directional")
        {
            return Wayfinder::LightType::Directional;
        }

        if (*type == "point")
        {
            return Wayfinder::LightType::Point;
        }

        return fallback;
    }

    bool ValidateOptionalNonEmptyString(const toml::table& componentTable, const char* key, std::string& error)
    {
        if (!componentTable.contains(key))
        {
            return true;
        }

        const auto value = componentTable[key].value<std::string>();
        if (!value)
        {
            error = std::string("'") + key + "' must be a string";
            return false;
        }

        if (value->empty())
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

    bool IsNumberNode(const toml::node& node)
    {
        return node.is_integer() || node.is_floating_point();
    }

    bool ValidateOptionalBool(const toml::table& componentTable, const char* key, std::string& error)
    {
        const toml::node* node = componentTable.get(key);
        if (!node)
        {
            return true;
        }

        if (!node->is_boolean())
        {
            error = std::string{"field '"} + key + "' must be a boolean";
            return false;
        }

        return true;
    }

    bool ValidateOptionalAssetId(const toml::table& componentTable, const char* key, std::string& error)
    {
        const toml::node* node = componentTable.get(key);
        if (!node)
        {
            return true;
        }

        if (!node->is_string())
        {
            error = std::string{"field '"} + key + "' must be a UUID string";
            return false;
        }

        const auto assetId = Wayfinder::AssetId::Parse(node->value_or(std::string{}));
        if (!assetId)
        {
            error = std::string{"field '"} + key + "' must be a valid UUID";
            return false;
        }

        return true;
    }

    bool ValidateOptionalNumber(const toml::table& componentTable, const char* key, std::string& error)
    {
        const toml::node* node = componentTable.get(key);
        if (!node)
        {
            return true;
        }

        if (!IsNumberNode(*node))
        {
            error = std::string{"field '"} + key + "' must be numeric";
            return false;
        }

        return true;
    }

    bool ValidateOptionalInteger(const toml::table& componentTable, const char* key, std::string& error)
    {
        const toml::node* node = componentTable.get(key);
        if (!node) { return true; }

        if (!node->is_integer())
        {
            error = std::string{"field '"} + key + "' must be an integer";
            return false;
        }

        return true;
    }

    bool ValidateOptionalVector3(const toml::table& componentTable, const char* key, std::string& error)
    {
        const toml::node* node = componentTable.get(key);
        if (!node)
        {
            return true;
        }

        const toml::array* values = node->as_array();
        if (!values || values->size() != 3)
        {
            error = std::string{"field '"} + key + "' must be an array of 3 numbers";
            return false;
        }

        for (size_t index = 0; index < values->size(); ++index)
        {
            const toml::node* value = values->get(index);
            if (!value || !IsNumberNode(*value))
            {
                error = std::string{"field '"} + key + "' must be an array of 3 numbers";
                return false;
            }
        }

        return true;
    }

    bool ValidateOptionalColor(const toml::table& componentTable, const char* key, std::string& error)
    {
        const toml::node* node = componentTable.get(key);
        if (!node)
        {
            return true;
        }

        const toml::array* values = node->as_array();
        if (!values || (values->size() != 3 && values->size() != 4))
        {
            error = std::string{"field '"} + key + "' must be an array of 3 or 4 integers";
            return false;
        }

        for (size_t index = 0; index < values->size(); ++index)
        {
            const toml::node* value = values->get(index);
            if (!value || !value->is_integer())
            {
                error = std::string{"field '"} + key + "' must be an array of 3 or 4 integers";
                return false;
            }
        }

        return true;
    }

    bool ValidateOptionalEnumValue(
        const toml::table& componentTable,
        const char* key,
        std::initializer_list<std::string_view> acceptedValues,
        std::string& error)
    {
        const auto value = componentTable[key].value<std::string>();
        if (!value)
        {
            return !componentTable.contains(key);
        }

        for (const std::string_view acceptedValue : acceptedValues)
        {
            if (*value == acceptedValue)
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

    bool ValidateTransform(const toml::table& componentTable, std::string& error)
    {
        return ValidateOptionalVector3(componentTable, "position", error)
            && ValidateOptionalVector3(componentTable, "rotation", error)
            && ValidateOptionalVector3(componentTable, "scale", error);
    }

    bool ValidateMesh(const toml::table& componentTable, std::string& error)
    {
        return ValidateOptionalEnumValue(componentTable, "primitive", {"cube"}, error)
            && ValidateOptionalVector3(componentTable, "dimensions", error);
    }

    bool ValidateCamera(const toml::table& componentTable, std::string& error)
    {
        return ValidateOptionalBool(componentTable, "primary", error)
            && ValidateOptionalVector3(componentTable, "target", error)
            && ValidateOptionalVector3(componentTable, "up", error)
            && ValidateOptionalNumber(componentTable, "fov", error)
            && ValidateOptionalEnumValue(componentTable, "projection", {"perspective", "orthographic"}, error);
    }

    bool ValidateLight(const toml::table& componentTable, std::string& error)
    {
        return ValidateOptionalEnumValue(componentTable, "type", {"point", "directional"}, error)
            && ValidateOptionalColor(componentTable, "color", error)
            && ValidateOptionalNumber(componentTable, "intensity", error)
            && ValidateOptionalNumber(componentTable, "range", error)
            && ValidateOptionalBool(componentTable, "debug_draw", error);
    }

    bool ValidateMaterial(const toml::table& componentTable, std::string& error)
    {
        return ValidateOptionalAssetId(componentTable, "material_id", error)
            && ValidateOptionalColor(componentTable, "base_color", error)
            && ValidateOptionalBool(componentTable, "wireframe", error);
    }

    bool ValidateRenderable(const toml::table& componentTable, std::string& error)
    {
        return ValidateOptionalBool(componentTable, "visible", error)
            && ValidateOptionalNonEmptyString(componentTable, "layer", error)
            && ValidateOptionalInteger(componentTable, "sort_priority", error);
    }

    bool ValidateEffectParameter(std::string_view key, const toml::node& node, std::string& error)
    {
        if (node.is_integer() || node.is_floating_point()) { return true; }

        if (const toml::array* arr = node.as_array())
        {
            if (arr->size() < 3 || arr->size() > 4)
            {
                error = std::string("effect parameter '") + std::string(key) + "' array must have 3 or 4 elements";
                return false;
            }

            bool allInts = true;
            for (size_t i = 0; i < arr->size(); ++i)
            {
                if (!arr->get(i)->is_integer() && !arr->get(i)->is_floating_point())
                {
                    error = std::string("effect parameter '") + std::string(key) + "' array elements must be numbers";
                    return false;
                }
                if (!arr->get(i)->is_integer()) { allInts = false; }
            }

            // 4-element arrays are only valid as Color (all integers r,g,b,a).
            // Float3 only reads 3 elements, so a 4-element float array would silently lose data.
            if (arr->size() == 4 && !allInts)
            {
                error = std::string("effect parameter '") + std::string(key)
                    + "' 4-element arrays must be all integers (Color r,g,b,a)";
                return false;
            }

            // 3-element all-integer arrays are ambiguous: ReadEffectParam treats them
            // as Color (r,g,b with a=255), not Float3. Require at least one float
            // for Float3 values (e.g. [1.0, 2.0, 3.0]).
            if (arr->size() == 3 && allInts)
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

    bool ValidatePostProcessVolume(const toml::table& componentTable, std::string& error)
    {
        if (!ValidateOptionalEnumValue(componentTable, "shape", {"global", "box", "sphere"}, error))
            return false;

        if (!ValidateOptionalInteger(componentTable, "priority", error))
            return false;

        if (!ValidateOptionalNumber(componentTable, "blend_distance", error))
            return false;

        if (!ValidateOptionalVector3(componentTable, "dimensions", error))
            return false;

        if (!ValidateOptionalNumber(componentTable, "radius", error))
            return false;

        // Validate the effects array if present
        const toml::node* effectsNode = componentTable.get("effects");
        if (effectsNode)
        {
            const toml::array* effectsArray = effectsNode->as_array();
            if (!effectsArray)
            {
                error = "'effects' must be an array of tables";
                return false;
            }

            for (size_t i = 0; i < effectsArray->size(); ++i)
            {
                const toml::table* effectTable = effectsArray->get(i)->as_table();
                if (!effectTable)
                {
                    error = "each entry in 'effects' must be a table";
                    return false;
                }

                const auto* typeNode = effectTable->get("type");
                if (!typeNode || !typeNode->is_string() || typeNode->value_or(std::string{}).empty())
                {
                    error = "each effect must have a non-empty 'type' string";
                    return false;
                }

                for (const auto& [key, node] : *effectTable)
                {
                    if (key == "type") { continue; }
                    if (key == "enabled")
                    {
                        if (!node.is_boolean())
                        {
                            error = "effect 'enabled' must be a boolean";
                            return false;
                        }
                        continue;
                    }
                    if (!ValidateEffectParameter(key.str(), node, error)) { return false; }
                }
            }
        }

        return true;
    }

    void ApplyTransform(const toml::table& componentTable, Wayfinder::Entity& entity)
    {
        Wayfinder::TransformComponent transform;
        transform.Position = ReadVector3(componentTable, "position", transform.Position);
        transform.Rotation = ReadVector3(componentTable, "rotation", transform.Rotation);
        transform.Scale = ReadVector3(componentTable, "scale", transform.Scale);
        entity.AddComponent<Wayfinder::TransformComponent>(transform);
    }

    void ApplyMesh(const toml::table& componentTable, Wayfinder::Entity& entity)
    {
        Wayfinder::MeshComponent mesh;
        mesh.Primitive = ReadPrimitive(componentTable, "primitive", mesh.Primitive);
        mesh.Dimensions = ReadVector3(componentTable, "dimensions", mesh.Dimensions);
        entity.AddComponent<Wayfinder::MeshComponent>(mesh);
    }

    void ApplyCamera(const toml::table& componentTable, Wayfinder::Entity& entity)
    {
        Wayfinder::CameraComponent camera;
        camera.Primary = componentTable["primary"].value_or(camera.Primary);
        camera.Target = ReadVector3(componentTable, "target", camera.Target);
        camera.Up = ReadVector3(componentTable, "up", camera.Up);
        camera.FieldOfView = ReadFloat(componentTable, "fov", camera.FieldOfView);
        camera.Projection = ReadProjection(componentTable, "projection", camera.Projection);
        entity.AddComponent<Wayfinder::CameraComponent>(camera);
    }

    void ApplyLight(const toml::table& componentTable, Wayfinder::Entity& entity)
    {
        Wayfinder::LightComponent light;
        light.Type = ReadLightType(componentTable, "type", light.Type);
        light.Tint = ReadColor(componentTable, "color", light.Tint);
        light.Intensity = ReadFloat(componentTable, "intensity", light.Intensity);
        light.Range = ReadFloat(componentTable, "range", light.Range);
        light.DebugDraw = componentTable["debug_draw"].value_or(light.DebugDraw);
        entity.AddComponent<Wayfinder::LightComponent>(light);
    }

    void ApplyMaterial(const toml::table& componentTable, Wayfinder::Entity& entity)
    {
        Wayfinder::MaterialComponent material;
        material.MaterialAssetId = ReadOptionalAssetId(componentTable, "material_id");
        material.HasBaseColorOverride = componentTable.contains("base_color");
        material.HasWireframeOverride = componentTable.contains("wireframe");
        if (material.HasBaseColorOverride)
        {
            material.BaseColor = ReadColor(componentTable, "base_color", material.BaseColor);
        }

        if (material.HasWireframeOverride)
        {
            material.Wireframe = componentTable["wireframe"].value_or(material.Wireframe);
        }

        entity.AddComponent<Wayfinder::MaterialComponent>(material);
    }

    void ApplyRenderable(const toml::table& componentTable, Wayfinder::Entity& entity)
    {
        Wayfinder::RenderableComponent renderable;
        renderable.Visible = componentTable["visible"].value_or(renderable.Visible);
        renderable.Layer = ReadRenderLayer(componentTable, "layer", renderable.Layer);

        const int64_t sortPriority = componentTable["sort_priority"].value_or(static_cast<int64_t>(renderable.SortPriority));
        renderable.SortPriority = ClampToByte(sortPriority);
        entity.AddComponent<Wayfinder::RenderableComponent>(renderable);
    }

    Wayfinder::PostProcessVolumeShape ReadVolumeShape(const toml::table& table, const char* key, Wayfinder::PostProcessVolumeShape fallback)
    {
        const auto value = table[key].value<std::string>();
        if (!value) return fallback;
        if (*value == "global") return Wayfinder::PostProcessVolumeShape::Global;
        if (*value == "box") return Wayfinder::PostProcessVolumeShape::Box;
        if (*value == "sphere") return Wayfinder::PostProcessVolumeShape::Sphere;
        return fallback;
    }

    // ── GameplayTagContainer ────────────────────────────────

    bool ValidateTags(const toml::table& componentTable, std::string& error)
    {
        const toml::node* node = componentTable.get("tags");
        if (!node)
            return true;

        const toml::array* values = node->as_array();
        if (!values)
        {
            error = "'tags' must be an array of strings";
            return false;
        }

        for (size_t i = 0; i < values->size(); ++i)
        {
            if (!values->get(i)->is_string())
            {
                error = "'tags' array element #" + std::to_string(i) + " must be a string";
                return false;
            }
        }

        return true;
    }

    void ApplyTags(const toml::table& componentTable, Wayfinder::Entity& entity)
    {
        // In non-Game contexts (e.g. waypoint, tests) the subsystem collection may not be bound.
        // Use Find() to avoid asserting and simply skip tag application if no registry is available.
        auto* registry = Wayfinder::GameSubsystems::Find<Wayfinder::GameplayTagRegistry>();

        Wayfinder::GameplayTagContainer container;
        if (registry)
        {
            if (const toml::array* tags = componentTable["tags"].as_array())
            {
                for (const toml::node& node : *tags)
                {
                    if (const auto str = node.value<std::string>())
                        container.AddTag(registry->RequestTag(*str));
                }
            }
        }
        else if (const toml::array* tags = componentTable["tags"].as_array(); tags && !tags->empty())
        {
            Wayfinder::LogScene.GetLogger()->LogFormat(
                Wayfinder::LogVerbosity::Warning,
                "Entity specifies {0} tag(s) but no GameplayTagRegistry is available — tags will be ignored.",
                tags->size());
        }
        entity.AddComponent<Wayfinder::GameplayTagContainer>(container);
    }

    void SerializeTags(const Wayfinder::Entity& entity, toml::table& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::GameplayTagContainer>())
            return;

        const auto& container = entity.GetComponent<Wayfinder::GameplayTagContainer>();
        if (container.IsEmpty())
            return;

        toml::array arr;
        for (const auto& tag : container)
            arr.push_back(tag.GetName());

        toml::table t;
        t.insert_or_assign("tags", std::move(arr));
        componentTables.insert_or_assign("gameplay_tags", std::move(t));
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

    Wayfinder::PostProcessParamValue ReadEffectParam(const toml::node& node)
    {
        if (node.is_floating_point()) return static_cast<float>(node.value_or(0.0));
        if (node.is_integer()) return static_cast<int32_t>(node.value_or(int64_t{0}));

        if (const toml::array* arr = node.as_array())
        {
            // All-integer arrays → Color; otherwise → Float3
            bool allInts = true;
            for (size_t i = 0; i < arr->size(); ++i)
            {
                if (!arr->get(i)->is_integer()) { allInts = false; break; }
            }

            if (allInts)
            {
                Wayfinder::Color c;
                c.r = static_cast<uint8_t>(arr->get(0)->value_or(int64_t{0}));
                c.g = static_cast<uint8_t>(arr->get(1)->value_or(int64_t{0}));
                c.b = static_cast<uint8_t>(arr->get(2)->value_or(int64_t{0}));
                c.a = arr->size() >= 4 ? static_cast<uint8_t>(arr->get(3)->value_or(int64_t{255})) : 255;
                return c;
            }

            if (arr->size() >= 3)
            {
                return Wayfinder::Float3{
                    ReadArrayFloat(*arr, 0, 0.0f),
                    ReadArrayFloat(*arr, 1, 0.0f),
                    ReadArrayFloat(*arr, 2, 0.0f)};
            }
        }

        return 0.0f;
    }

    Wayfinder::PostProcessEffect ReadEffect(const toml::table& effectTable)
    {
        Wayfinder::PostProcessEffect effect;
        effect.Type = effectTable["type"].value_or(std::string{});
        effect.Enabled = effectTable["enabled"].value_or(true);

        for (const auto& [key, node] : effectTable)
        {
            if (key == "type" || key == "enabled") { continue; }
            effect.Parameters[std::string(key.str())] = ReadEffectParam(node);
        }

        return effect;
    }

    void ApplyPostProcessVolume(const toml::table& componentTable, Wayfinder::Entity& entity)
    {
        Wayfinder::PostProcessVolumeComponent volume;
        volume.Shape = ReadVolumeShape(componentTable, "shape", volume.Shape);
        volume.Priority = static_cast<int>(componentTable["priority"].value_or(static_cast<int64_t>(volume.Priority)));
        volume.BlendDistance = ReadFloat(componentTable, "blend_distance", volume.BlendDistance);
        volume.Dimensions = ReadVector3(componentTable, "dimensions", volume.Dimensions);
        volume.Radius = ReadFloat(componentTable, "radius", volume.Radius);

        if (const toml::array* effectsArray = componentTable["effects"].as_array())
        {
            for (size_t i = 0; i < effectsArray->size(); ++i)
            {
                if (const toml::table* effectTable = effectsArray->get(i)->as_table())
                {
                    volume.Effects.push_back(ReadEffect(*effectTable));
                }
            }
        }

        entity.AddComponent<Wayfinder::PostProcessVolumeComponent>(volume);
    }

    void SerializeTransform(const Wayfinder::Entity& entity, toml::table& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::TransformComponent>())
        {
            return;
        }

        const Wayfinder::TransformComponent& transform = entity.GetComponent<Wayfinder::TransformComponent>();
        toml::table componentTable;
        componentTable.insert_or_assign("position", WriteVector3(transform.Position));
        componentTable.insert_or_assign("rotation", WriteVector3(transform.Rotation));
        componentTable.insert_or_assign("scale", WriteVector3(transform.Scale));
        componentTables.insert_or_assign("transform", componentTable);
    }

    void SerializeMesh(const Wayfinder::Entity& entity, toml::table& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::MeshComponent>())
        {
            return;
        }

        const Wayfinder::MeshComponent& mesh = entity.GetComponent<Wayfinder::MeshComponent>();
        toml::table componentTable;
        componentTable.insert_or_assign("primitive", std::string{ToString(mesh.Primitive)});
        componentTable.insert_or_assign("dimensions", WriteVector3(mesh.Dimensions));
        componentTables.insert_or_assign("mesh", componentTable);
    }

    void SerializeCamera(const Wayfinder::Entity& entity, toml::table& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::CameraComponent>())
        {
            return;
        }

        const Wayfinder::CameraComponent& camera = entity.GetComponent<Wayfinder::CameraComponent>();
        toml::table componentTable;
        componentTable.insert_or_assign("primary", camera.Primary);
        componentTable.insert_or_assign("target", WriteVector3(camera.Target));
        componentTable.insert_or_assign("up", WriteVector3(camera.Up));
        componentTable.insert_or_assign("fov", camera.FieldOfView);
        componentTable.insert_or_assign("projection", std::string{ToString(camera.Projection)});
        componentTables.insert_or_assign("camera", componentTable);
    }

    void SerializeLight(const Wayfinder::Entity& entity, toml::table& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::LightComponent>())
        {
            return;
        }

        const Wayfinder::LightComponent& light = entity.GetComponent<Wayfinder::LightComponent>();
        toml::table componentTable;
        componentTable.insert_or_assign("type", std::string{ToString(light.Type)});
        componentTable.insert_or_assign("color", WriteColor(light.Tint));
        componentTable.insert_or_assign("intensity", light.Intensity);
        componentTable.insert_or_assign("range", light.Range);
        componentTable.insert_or_assign("debug_draw", light.DebugDraw);
        componentTables.insert_or_assign("light", componentTable);
    }

    void SerializeMaterial(const Wayfinder::Entity& entity, toml::table& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::MaterialComponent>())
        {
            return;
        }

        const Wayfinder::MaterialComponent& material = entity.GetComponent<Wayfinder::MaterialComponent>();
        toml::table componentTable;
        if (material.MaterialAssetId)
        {
            componentTable.insert_or_assign("material_id", material.MaterialAssetId->ToString());
        }

        if (!material.MaterialAssetId || material.HasBaseColorOverride)
        {
            componentTable.insert_or_assign("base_color", WriteColor(material.BaseColor));
        }

        if (!material.MaterialAssetId || material.HasWireframeOverride)
        {
            componentTable.insert_or_assign("wireframe", material.Wireframe);
        }

        componentTables.insert_or_assign("material", componentTable);
    }

    void SerializeRenderable(const Wayfinder::Entity& entity, toml::table& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::RenderableComponent>())
        {
            return;
        }

        const Wayfinder::RenderableComponent& renderable = entity.GetComponent<Wayfinder::RenderableComponent>();
        toml::table componentTable;
        componentTable.insert_or_assign("visible", renderable.Visible);
        componentTable.insert_or_assign("layer", renderable.Layer.GetString());
        componentTable.insert_or_assign("sort_priority", static_cast<int64_t>(renderable.SortPriority));
        componentTables.insert_or_assign("renderable", componentTable);
    }

    void WriteEffectParam(toml::table& table, const std::string& key, const Wayfinder::PostProcessParamValue& value)
    {
        std::visit([&](const auto& v)
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, float>)
                table.insert_or_assign(key, static_cast<double>(v));
            else if constexpr (std::is_same_v<T, int32_t>)
                table.insert_or_assign(key, static_cast<int64_t>(v));
            else if constexpr (std::is_same_v<T, Wayfinder::Float3>)
                table.insert_or_assign(key, WriteVector3(v));
            else if constexpr (std::is_same_v<T, Wayfinder::Color>)
                table.insert_or_assign(key, WriteColor(v));
        }, value);
    }

    void SerializePostProcessVolume(const Wayfinder::Entity& entity, toml::table& componentTables)
    {
        if (!entity.HasComponent<Wayfinder::PostProcessVolumeComponent>())
        {
            return;
        }

        const Wayfinder::PostProcessVolumeComponent& volume = entity.GetComponent<Wayfinder::PostProcessVolumeComponent>();
        toml::table componentTable;
        componentTable.insert_or_assign("shape", std::string{ToString(volume.Shape)});
        componentTable.insert_or_assign("priority", static_cast<int64_t>(volume.Priority));
        componentTable.insert_or_assign("blend_distance", volume.BlendDistance);
        componentTable.insert_or_assign("dimensions", WriteVector3(volume.Dimensions));
        componentTable.insert_or_assign("radius", volume.Radius);

        if (!volume.Effects.empty())
        {
            toml::array effectsArray;
            for (const auto& effect : volume.Effects)
            {
                toml::table effectTable;
                effectTable.insert_or_assign("type", effect.Type);
                if (!effect.Enabled) { effectTable.insert_or_assign("enabled", false); }

                for (const auto& [key, value] : effect.Parameters)
                {
                    WriteEffectParam(effectTable, key, value);
                }

                effectsArray.push_back(effectTable);
            }

            componentTable.insert_or_assign("effects", effectsArray);
        }

        componentTables.insert_or_assign("post_process_volume", componentTable);
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

    void SceneComponentRegistry::ApplyComponents(const toml::table& componentTables, Entity& entity) const
    {
        for (const auto& [key, node] : componentTables)
        {
            const Entry* entry = Find(key.str());
            if (!entry || !entry->ApplyFn)
            {
                continue;
            }

            const toml::table* componentTable = node.as_table();
            if (!componentTable)
            {
                continue;
            }

            entry->ApplyFn(*componentTable, entity);
        }
    }

    void SceneComponentRegistry::SerializeComponents(const Entity& entity, toml::table& componentTables) const
    {
        for (const Entry& entry : kEntries)
        {
            if (entry.SerializeFn)
            {
                entry.SerializeFn(entity, componentTables);
            }
        }
    }

    bool SceneComponentRegistry::ValidateComponent(std::string_view key, const toml::table& componentTable, std::string& error) const
    {
        const Entry* entry = Find(key);
        if (!entry || !entry->ValidateFn)
        {
            error = "component is not registered for scene authoring";
            return false;
        }

        return entry->ValidateFn(componentTable, error);
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