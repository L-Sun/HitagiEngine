module asset;
import std;

namespace hitagi::asset {
Resource::Resource(const Resource& other)
    : m_Type(other.m_Type), m_Name(other.m_Name), m_UUID(utils::UUID::Create()) {
}

Resource& Resource::operator=(const Resource& rhs) {
    if (this != &rhs) {
        m_Type = rhs.m_Type;
        m_Name = rhs.m_Name;
    }
    return *this;
}

auto Resource::GetUniqueName() const noexcept -> std::pmr::string {
    return std::pmr::string{std::format("{}-{}", GetName(), GetUUID())};
}

}  // namespace hitagi::asset
