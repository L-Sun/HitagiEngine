#pragma once
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/buffer.hpp>

#include <cstdint>
#include <type_traits>
#include <set>

namespace hitagi::gfx::backend {
struct Resource {
    virtual ~Resource() = default;
    template <typename T>
    T* GetBackend() {
        return reinterpret_cast<T*>(this);
    }
    std::uint64_t fence_value = 0;
};
}  // namespace hitagi::gfx::backend

namespace hitagi::resource {
struct Resource {
    Resource(std::size_t buffer_size = 0, std::string_view name = "") : name(name), cpu_buffer(buffer_size) {}
    Resource(core::Buffer cpu_buffer, std::string_view name = "") : name(name), cpu_buffer(std::move(cpu_buffer)) {}

    Resource(const Resource& other);
    Resource& operator=(const Resource& rhs);

    Resource(Resource&& rhs)            = default;
    Resource& operator=(Resource&& rhs) = default;

    std::pmr::string                        name;
    bool                                    dirty = true;
    core::Buffer                            cpu_buffer{};
    std::unique_ptr<gfx::backend::Resource> gpu_resource;

    // TODO
    // for more efficient update gpu resource, we mark cpu buffer per 1 kB if the region is modified!
    // constexpr static std::size_t dirty_range_size = 4_kB;
    // std::pmr::set<std::size_t>   buffer_range_dirty;
};
}  // namespace hitagi::resource