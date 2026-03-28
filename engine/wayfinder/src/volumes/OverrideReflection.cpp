#include "OverrideReflection.h"

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
            field = Override<Float3>::Set(Float3{(*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>()});
        }
    }

} // namespace Wayfinder
