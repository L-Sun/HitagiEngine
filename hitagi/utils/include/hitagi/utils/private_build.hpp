#pragma once

#include <memory>
namespace utils {
template <typename T>
class enable_private_make_shared_build {
public:
    template <typename Builder>
    static std::shared_ptr<T> Create(const Builder& builder) {
        struct CreateTemp : public T {
            CreateTemp(const Builder& builder) : T(builder) {}
        };

        return std::dynamic_pointer_cast<T>(std::make_shared<CreateTemp>(builder));
    }
};

template <typename T>
class enable_private_allocate_shared_build {
public:
    template <typename Builder, typename Alloc>
    static std::shared_ptr<T> Create(const Builder& builder, const Alloc& alloc) {
        struct CreateTemp : public T {
            CreateTemp(const Builder& builder) : T(builder) {}
        };

        return std::dynamic_pointer_cast<T>(std::allocate_shared<CreateTemp>(alloc, builder));
    }
};
}  // namespace utils