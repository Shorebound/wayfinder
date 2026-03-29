#include "OverrideReflection.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

namespace Wayfinder
{
    void WriteOverrideField(nlohmann::json& json, const std::string_view key, const Override<float>& field)
    {
        if (field.Active)
        {
            json[std::string{key}] = field.Value;
        }
    }

    void WriteOverrideField(nlohmann::json& json, const std::string_view key, const Override<Float3>& field)
    {
        if (field.Active)
        {
            json[std::string{key}] = nlohmann::json::array({field.Value.x, field.Value.y, field.Value.z});
        }
    }

    void ReadOverrideField(const nlohmann::json& json, const std::string_view key, Override<float>& field)
    {
        if (auto it = json.find(key); it != json.end() && it->is_number())
        {
            field = Override<float>::Set(it->get<float>());
        }
    }

    void ReadOverrideField(const nlohmann::json& json, const std::string_view key, Override<Float3>& field)
    {
        if (auto it = json.find(key); it != json.end() && it->is_array() && it->size() >= 3)
        {
            const auto& e0 = (*it)[0];
            const auto& e1 = (*it)[1];
            const auto& e2 = (*it)[2];
            if (!e0.is_number() || !e1.is_number() || !e2.is_number())
            {
                WAYFINDER_WARN(LogScene, "ReadOverrideField: key \"{}\" — expected three numeric array elements", key);
                return;
            }
            field = Override<Float3>::Set(Float3{e0.get<float>(), e1.get<float>(), e2.get<float>()});
        }
    }

} // namespace Wayfinder
