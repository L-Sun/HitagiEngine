export module utils:uuid;
#ifdef _WIN32
import interop.win32;
#endif
import std;

export namespace hitagi::utils {

class UUID {
public:
    static auto Create() -> UUID;

    auto operator==(const UUID& other) const -> bool = default;
    auto operator!=(const UUID& other) const -> bool = default;
    auto operator<=>(const UUID& other) const        = default;

private:
    std::array<std::byte, 16> m_Data;

    friend struct std::formatter<hitagi::utils::UUID>;
};

}  // namespace hitagi::utils

template <>
struct std::formatter<hitagi::utils::UUID> : std::formatter<std::string> {
    inline auto format(const hitagi::utils::UUID& uuid, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                return std::format(
                    "{:02x}{:02x}{:02x}{:02x}-"
                    "{:02x}{:02x}-"
                    "{:02x}{:02x}-"
                    "{:02x}{:02x}-"
                    "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                    std::to_integer<std::uint8_t>(uuid.m_Data[I])...);
            }(std::make_index_sequence<16>{}),
            ctx);
    }
};
namespace hitagi::utils {

auto UUID::Create() -> UUID {
    UUID uuid;

#ifdef _WIN32
    GUID newId;
    CoCreateGuid(&newId);
    uuid.m_Data = std::array<std::byte, 16>{
        static_cast<std::byte>(newId.Data1 >> 24),
        static_cast<std::byte>(newId.Data1 >> 16),
        static_cast<std::byte>(newId.Data1 >> 8),
        static_cast<std::byte>(newId.Data1),

        static_cast<std::byte>(newId.Data2 >> 8),
        static_cast<std::byte>(newId.Data2),

        static_cast<std::byte>(newId.Data3 >> 8),
        static_cast<std::byte>(newId.Data3),

        static_cast<std::byte>(newId.Data4[0]),
        static_cast<std::byte>(newId.Data4[1]),
        static_cast<std::byte>(newId.Data4[2]),
        static_cast<std::byte>(newId.Data4[3]),
        static_cast<std::byte>(newId.Data4[4]),
        static_cast<std::byte>(newId.Data4[5]),
        static_cast<std::byte>(newId.Data4[6]),
        static_cast<std::byte>(newId.Data4[7]),
    };
#endif
    return uuid;
}

}  // namespace hitagi::utils
