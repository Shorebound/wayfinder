#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    class WAYFINDER_API Uuid
    {
    public:
        Uuid() = default;
        explicit Uuid(const std::array<std::uint8_t, 16>& bytes) : m_bytes(bytes) {}

        static Uuid Generate();
        static std::optional<Uuid> Parse(std::string_view text);

        bool IsNil() const;
        std::string ToString() const;

        const std::array<std::uint8_t, 16>& GetBytes() const { return m_bytes; }

        bool operator==(const Uuid&) const = default;

    private:
        std::array<std::uint8_t, 16> m_bytes{};
    };

    struct WAYFINDER_API SceneObjectId
    {
        Uuid Value{};

        SceneObjectId() = default;
        explicit SceneObjectId(const Uuid& value) : Value(value) {}

        static SceneObjectId Generate();
        static std::optional<SceneObjectId> Parse(std::string_view text);

        bool IsNil() const { return Value.IsNil(); }
        std::string ToString() const { return Value.ToString(); }

        bool operator==(const SceneObjectId&) const = default;
    };

    struct WAYFINDER_API AssetId
    {
        Uuid Value{};

        AssetId() = default;
        explicit AssetId(const Uuid& value) : Value(value) {}

        static AssetId Generate();
        static std::optional<AssetId> Parse(std::string_view text);

        bool IsNil() const { return Value.IsNil(); }
        std::string ToString() const { return Value.ToString(); }

        bool operator==(const AssetId&) const = default;
    };
}

namespace std
{
    template <>
    struct hash<Wayfinder::Uuid>
    {
        size_t operator()(const Wayfinder::Uuid& value) const noexcept;
    };

    template <>
    struct hash<Wayfinder::SceneObjectId>
    {
        size_t operator()(const Wayfinder::SceneObjectId& value) const noexcept;
    };

    template <>
    struct hash<Wayfinder::AssetId>
    {
        size_t operator()(const Wayfinder::AssetId& value) const noexcept;
    };
}
