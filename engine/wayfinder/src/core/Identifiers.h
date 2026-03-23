#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <random>
#include <string>
#include <string_view>

#include "wayfinder_exports.h"

namespace Wayfinder
{
    /// FNV-1a 64-bit hash — simple, fast, constexpr, excellent distribution.
    constexpr auto Fnv1a64(std::string_view str) -> uint64_t
    {
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (auto c : str)
        {
            hash ^= static_cast<uint64_t>(c);
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }

    struct StringHash
    {
        uint64_t Value{};

        constexpr StringHash() = default;
        constexpr explicit StringHash(uint64_t v) : Value(v) {}
        constexpr explicit StringHash(std::string_view str) : Value(Fnv1a64(str)) {}

        constexpr bool IsValid() const
        {
            return Value != 0;
        }
        constexpr explicit operator bool() const
        {
            return IsValid();
        }

        constexpr auto operator==(const StringHash&) const -> bool = default;
        constexpr auto operator<=>(const StringHash&) const = default;
    };
}

template<>
struct std::hash<Wayfinder::StringHash>
{
    auto operator()(Wayfinder::StringHash h) const noexcept -> size_t
    {
        return std::hash<uint64_t>{}(h.Value);
    }
};

template<>
struct std::formatter<Wayfinder::StringHash> : std::formatter<uint64_t>
{
    auto format(Wayfinder::StringHash h, auto& ctx) const
    {
        return std::formatter<uint64_t>::format(h.Value, ctx);
    }
};

namespace Wayfinder
{
    /// 128-bit UUID (v4 random). Header-only, constexpr-friendly, thread-safe generation.
    class WAYFINDER_API Uuid
    {
    public:
        constexpr Uuid() = default;
        constexpr explicit Uuid(std::array<uint8_t, 16> bytes) : m_bytes(bytes) {}

        static auto Generate() -> Uuid
        {
            thread_local std::mt19937_64 sRng([]
                {
                    std::random_device rd;
                    return rd();
                }());

            std::uniform_int_distribution<uint64_t> dist;
            uint64_t a = dist(sRng);
            uint64_t b = dist(sRng);

            std::array<uint8_t, 16> bytes{};
            std::memcpy(bytes.data(), &a, 8);
            std::memcpy(bytes.data() + 8, &b, 8);

            // RFC 4122 v4 variant bits
            bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
            bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

            return Uuid(bytes);
        }

        static auto Parse(std::string_view text) -> std::optional<Uuid>
        {
            if (text.size() != 36)
            {
                return std::nullopt;
            }

            constexpr size_t kDashPositions[] = {8, 13, 18, 23};
            for (auto pos : kDashPositions)
            {
                if (text[pos] != '-')
                {
                    return std::nullopt;
                }
            }

            auto hexVal = [](char c) -> int
            {
                if (c >= '0' && c <= '9')
                {
                    return c - '0';
                }
                if (c >= 'a' && c <= 'f')
                {
                    return 10 + (c - 'a');
                }
                if (c >= 'A' && c <= 'F')
                {
                    return 10 + (c - 'A');
                }
                return -1;
            };

            std::array<uint8_t, 16> bytes{};
            size_t byteIdx = 0;

            for (size_t i = 0; i < text.size();)
            {
                if (text[i] == '-')
                {
                    ++i;
                    continue;
                }

                if (i + 1 >= text.size() || byteIdx >= bytes.size())
                {
                    return std::nullopt;
                }

                int hi = hexVal(text[i]);
                int lo = hexVal(text[i + 1]);
                if (hi < 0 || lo < 0)
                {
                    return std::nullopt;
                }

                bytes[byteIdx++] = static_cast<uint8_t>((hi << 4) | lo);
                i += 2;
            }

            if (byteIdx != bytes.size())
            {
                return std::nullopt;
            }

            return Uuid(bytes);
        }

        auto ToString() const -> std::string
        {
            constexpr char kHex[] = "0123456789abcdef";

            std::string result;
            result.reserve(36);

            for (size_t i = 0; i < m_bytes.size(); ++i)
            {
                if (i == 4 || i == 6 || i == 8 || i == 10)
                {
                    result.push_back('-');
                }
                result.push_back(kHex[(m_bytes[i] >> 4) & 0x0F]);
                result.push_back(kHex[m_bytes[i] & 0x0F]);
            }

            return result;
        }

        constexpr bool IsNil() const
        {
            return std::all_of(m_bytes.begin(), m_bytes.end(),
                [](uint8_t b)
                {
                    return b == 0;
                });
        }

        constexpr explicit operator bool() const
        {
            return !IsNil();
        }

        constexpr auto GetBytes() const -> const std::array<uint8_t, 16>&
        {
            return m_bytes;
        }

        constexpr auto operator==(const Uuid&) const -> bool = default;
        constexpr auto operator<=>(const Uuid&) const = default;

    private:
        std::array<uint8_t, 16> m_bytes{};
    };
}

template<>
struct std::hash<Wayfinder::Uuid>
{
    auto operator()(const Wayfinder::Uuid& id) const noexcept -> size_t
    {
        const auto& b = id.GetBytes();
        uint64_t a, c;
        std::memcpy(&a, b.data(), 8);
        std::memcpy(&c, b.data() + 8, 8);
        a ^= c + 0x9e3779b97f4a7c15ULL + (a << 12) + (a >> 4);
        return static_cast<size_t>(a);
    }
};

template<>
struct std::formatter<Wayfinder::Uuid> : std::formatter<std::string>
{
    auto format(const Wayfinder::Uuid& id, auto& ctx) const
    {
        return std::formatter<std::string>::format(id.ToString(), ctx);
    }
};

namespace Wayfinder
{
    /// Zero-cost typed wrapper over Uuid. Prevents mixing SceneObjectId and AssetId at compile time.
    /// New ID domains are a single `using` alias away — no boilerplate.
    template<typename Tag>
    struct TypedId
    {
        Uuid Value{};

        TypedId() = default;
        explicit TypedId(Uuid id) : Value(id) {}

        static auto Generate() -> TypedId
        {
            return TypedId(Uuid::Generate());
        }

        static auto Parse(std::string_view text) -> std::optional<TypedId>
        {
            if (auto id = Uuid::Parse(text))
            {
                return TypedId(*id);
            }
            return std::nullopt;
        }

        bool IsNil() const
        {
            return Value.IsNil();
        }
        auto ToString() const -> std::string
        {
            return Value.ToString();
        }

        explicit operator bool() const
        {
            return !IsNil();
        }

        auto operator==(const TypedId&) const -> bool = default;
        auto operator<=>(const TypedId&) const = default;
    };
}

template<typename Tag>
struct std::hash<Wayfinder::TypedId<Tag>>
{
    auto operator()(const Wayfinder::TypedId<Tag>& id) const noexcept -> size_t
    {
        return std::hash<Wayfinder::Uuid>{}(id.Value);
    }
};

template<typename Tag>
struct std::formatter<Wayfinder::TypedId<Tag>> : std::formatter<Wayfinder::Uuid>
{
    auto format(const Wayfinder::TypedId<Tag>& id, auto& ctx) const
    {
        return std::formatter<Wayfinder::Uuid>::format(id.Value, ctx);
    }
};

namespace Wayfinder
{
    // Tag types — add new ID domains trivially by adding a tag + using alias.
    struct SceneObjectIdTag {};
    struct AssetIdTag {};

    using SceneObjectId = TypedId<SceneObjectIdTag>;
    using AssetId = TypedId<AssetIdTag>;
}
