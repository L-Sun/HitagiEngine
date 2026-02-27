export module utils:uuid;
import std;

export namespace hitagi::utils {
class UUID {
public:
    static auto Create() -> UUID;

    auto operator==(const UUID& other) const -> bool = default;
    auto operator!=(const UUID& other) const -> bool = default;

private:
    std::array<std::byte, 16> m_Data;

    friend struct std::formatter<hitagi::utils::UUID>;
};
}  // namespace hitagi::utils

export template <>
struct std::formatter<hitagi::utils::UUID> : std::formatter<std::string> {
    auto format(const hitagi::utils::UUID& uuid, std::format_context& ctx) const {
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
