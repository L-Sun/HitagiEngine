#pragma once
#include <array>
#include <cstddef>
#include <fmt/format.h>

namespace hitagi::utils {
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
    return fmt::format("{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                       uuid.m_Data[0], uuid.m_Data[1], uuid.m_Data[2], uuid.m_Data[3],
                       uuid.m_Data[4], uuid.m_Data[5], uuid.m_Data[6], uuid.m_Data[7],
                       uuid.m_Data[8], uuid.m_Data[9], uuid.m_Data[10], uuid.m_Data[11],
                       uuid.m_Data[12], uuid.m_Data[13], uuid.m_Data[14], uuid.m_Data[15]);
}

}  // namespace hitagi::utils