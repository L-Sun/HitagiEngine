#include <hitagi/asset/resource.hpp>

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

}  // namespace hitagi::asset