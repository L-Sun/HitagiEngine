#include <hitagi/utils/uuid.hpp>

#ifdef _WIN32
#include <objbase.h>
#endif

namespace hitagi::utils {

auto UUID::Create() -> UUID {
#ifdef _WIN32
    UUID uuid;
    GUID newId;
    CoCreateGuid(&newId);
    uuid.m_Data = {
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