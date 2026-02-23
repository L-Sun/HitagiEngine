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

    friend auto format_as(const UUID& uuid) noexcept;
};

inline auto format_as(const UUID& uuid) noexcept {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::format(
            "{:02x}{:02x}{:02x}{:02x}-"
            "{:02x}{:02x}-"
            "{:02x}{:02x}-"
            "{:02x}{:02x}-"
            "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
            std::to_integer<std::uint8_t>(uuid.m_Data[I])...);
    }(std::make_index_sequence<16>{});
}
}  // namespace hitagi::utils
