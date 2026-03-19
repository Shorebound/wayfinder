#include "ComponentRegistry.h"

#include "Components.h"
#include "entity/Entity.h"
#include "../core/GameplayTag.h"
#include "../core/GameplayTagRegistry.h"
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

    std::string ReadRenderLayer(const toml::table& table, const char* key, const std::string& fallback)
    {
        const auto layer = table[key].value<std::string>();
        if (!layer || layer->empty())
        {
            return fallback;
        }

        return *layer;
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
            && ValidateOptionalNumber(componentTable, "sort_priority", error);
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
        if (!registry)
        {
            return;
        }

        Wayfinder::GameplayTagContainer container;
        if (const toml::array* tags = componentTable["tags"].as_array())
        {
            for (const toml::node& node : *tags)
            {
                if (const auto str = node.value<std::string>())
                    container.AddTag(registry->RequestTag(*str));
            }
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
        componentTable.insert_or_assign("layer", renderable.Layer);
        componentTable.insert_or_assign("sort_priority", static_cast<int64_t>(renderable.SortPriority));
        componentTables.insert_or_assign("renderable", componentTable);
    }

    constexpr std::array<Wayfinder::SceneComponentRegistry::Entry, 7> kEntries = {{
        {"transform", &RegisterComponent<Wayfinder::TransformComponent>, &ApplyTransform, &SerializeTransform, &ValidateTransform},
        {"mesh", &RegisterComponent<Wayfinder::MeshComponent>, &ApplyMesh, &SerializeMesh, &ValidateMesh},
        {"camera", &RegisterComponent<Wayfinder::CameraComponent>, &ApplyCamera, &SerializeCamera, &ValidateCamera},
        {"light", &RegisterComponent<Wayfinder::LightComponent>, &ApplyLight, &SerializeLight, &ValidateLight},
        {"material", &RegisterComponent<Wayfinder::MaterialComponent>, &ApplyMaterial, &SerializeMaterial, &ValidateMaterial},
        {"renderable", &RegisterComponent<Wayfinder::RenderableComponent>, &ApplyRenderable, &SerializeRenderable, &ValidateRenderable},
        {"gameplay_tags", &RegisterComponent<Wayfinder::GameplayTagContainer>, &ApplyTags, &SerializeTags, &ValidateTags},
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