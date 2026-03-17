#include "Identifiers.h"

#include <random>

namespace
{
    constexpr char kHexDigits[] = "0123456789abcdef";

    int HexToInt(const char value)
    {
        if (value >= '0' && value <= '9')
        {
            return value - '0';
        }

        if (value >= 'a' && value <= 'f')
        {
            return 10 + (value - 'a');
        }

        if (value >= 'A' && value <= 'F')
        {
            return 10 + (value - 'A');
        }

        return -1;
    }
}

namespace Wayfinder
{
    Uuid Uuid::Generate()
    {
        static std::random_device randomDevice;
        static std::mt19937_64 generator(randomDevice());
        static std::uniform_int_distribution<unsigned int> distribution(0, 255);

        std::array<std::uint8_t, 16> bytes{};
        for (std::uint8_t& byte : bytes)
        {
            byte = static_cast<std::uint8_t>(distribution(generator));
        }

        bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0F) | 0x40);
        bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3F) | 0x80);
        return Uuid(bytes);
    }

    std::optional<Uuid> Uuid::Parse(std::string_view text)
    {
        if (text.size() != 36)
        {
            return std::nullopt;
        }

        if (text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-')
        {
            return std::nullopt;
        }

        std::array<std::uint8_t, 16> bytes{};
        size_t byteIndex = 0;

        for (size_t index = 0; index < text.size();)
        {
            if (text[index] == '-')
            {
                ++index;
                continue;
            }

            if (index + 1 >= text.size() || byteIndex >= bytes.size())
            {
                return std::nullopt;
            }

            const int highNibble = HexToInt(text[index]);
            const int lowNibble = HexToInt(text[index + 1]);
            if (highNibble < 0 || lowNibble < 0)
            {
                return std::nullopt;
            }

            bytes[byteIndex] = static_cast<std::uint8_t>((highNibble << 4) | lowNibble);
            ++byteIndex;
            index += 2;
        }

        if (byteIndex != bytes.size())
        {
            return std::nullopt;
        }

        return Uuid(bytes);
    }

    bool Uuid::IsNil() const
    {
        for (const std::uint8_t byte : m_bytes)
        {
            if (byte != 0)
            {
                return false;
            }
        }

        return true;
    }

    std::string Uuid::ToString() const
    {
        std::string result;
        result.reserve(36);

        for (size_t index = 0; index < m_bytes.size(); ++index)
        {
            if (index == 4 || index == 6 || index == 8 || index == 10)
            {
                result.push_back('-');
            }

            result.push_back(kHexDigits[(m_bytes[index] >> 4) & 0x0F]);
            result.push_back(kHexDigits[m_bytes[index] & 0x0F]);
        }

        return result;
    }

    SceneObjectId SceneObjectId::Generate()
    {
        return SceneObjectId(Uuid::Generate());
    }

    std::optional<SceneObjectId> SceneObjectId::Parse(std::string_view text)
    {
        const std::optional<Uuid> parsed = Uuid::Parse(text);
        if (!parsed)
        {
            return std::nullopt;
        }

        return SceneObjectId(*parsed);
    }

    AssetId AssetId::Generate()
    {
        return AssetId(Uuid::Generate());
    }

    std::optional<AssetId> AssetId::Parse(std::string_view text)
    {
        const std::optional<Uuid> parsed = Uuid::Parse(text);
        if (!parsed)
        {
            return std::nullopt;
        }

        return AssetId(*parsed);
    }
}

namespace std
{
    size_t hash<Wayfinder::Uuid>::operator()(const Wayfinder::Uuid& value) const noexcept
    {
        size_t hashValue = 1469598103934665603ull;
        for (const std::uint8_t byte : value.GetBytes())
        {
            hashValue ^= byte;
            hashValue *= 1099511628211ull;
        }

        return hashValue;
    }

    size_t hash<Wayfinder::SceneObjectId>::operator()(const Wayfinder::SceneObjectId& value) const noexcept
    {
        return hash<Wayfinder::Uuid>{}(value.Value);
    }

    size_t hash<Wayfinder::AssetId>::operator()(const Wayfinder::AssetId& value) const noexcept
    {
        return hash<Wayfinder::Uuid>{}(value.Value);
    }
}
