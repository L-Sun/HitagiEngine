#include <hitagi/asset/resource.hpp>

namespace hitagi::asset {
Resource::Resource(const Resource& other)
    : name(other.name),
      cpu_buffer(other.cpu_buffer),
      gpu_resource(nullptr) {}

Resource& Resource::operator=(const Resource& rhs) {
    if (this != &rhs) {
        name       = rhs.name;
        cpu_buffer = rhs.cpu_buffer;
        dirty      = true;
    }

    return *this;
}

}  // namespace hitagi::asset