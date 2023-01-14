#include <hitagi/asset/resource.hpp>

#include <fmt/format.h>

namespace hitagi::asset {
Resource::Resource(const Resource& ohter)
    : m_Name(ohter.m_Name), m_Guid(xg::newGuid()) {
}

Resource& Resource::operator=(const Resource& rhs) {
    if (this != &rhs) {
        m_Name = rhs.m_Name;
    }
    return *this;
}

auto Resource::GetUniqueName() const noexcept -> std::pmr::string {
    return std::pmr::string{fmt::format("{}-{}", GetName(), GetGuid().str())};
}

}  // namespace hitagi::asset