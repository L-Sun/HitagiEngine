#pragma once

#include <memory>
namespace hitagi::utils {
template <typename T>
class enable_private_make_shared {
public:
    template <typename... Args>
    static auto Create(Args&&... args) -> std::shared_ptr<T> {
        struct CreateTemp : public T {
            CreateTemp(Args&&... args) : T(std::forward<Args>(args)...) {}
        };
        return std::static_pointer_cast<T>(std::make_shared<CreateTemp>(std::forward<Args>(args)...));
    }
};

template <typename T>
class enable_private_allocate_shared {
public:
    template <typename Alloc, typename... Args>
    static std::shared_ptr<T> Create(Alloc&& alloc, Args&&... args) {
        struct CreateTemp : public T {
            using T::T;
        };
        return std::static_pointer_cast<T>(std::allocate_shared<CreateTemp>(std::forward<Alloc>(alloc), std::forward<Args>(args)...));
    }
};
}  // namespace hitagi::utils