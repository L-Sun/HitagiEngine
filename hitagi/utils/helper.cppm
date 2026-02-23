export module utils:helper;

import std;

export constexpr std::size_t operator""_kB(unsigned long long val) { return val << 10; }

export namespace hitagi::utils {

constexpr std::size_t align(size_t x, size_t a) {
    return (x + a - 1) & ~(a - 1);
}

[[noreturn]] inline void unreachable() {
#if defined(__GNUC__)  // GCC, Clang, ICC
    __builtin_unreachable();
#elif defined(_MSC_VER)  // MSVC
    __assume(false);
#endif
}

constexpr inline auto add_parentheses(std::string_view str) noexcept {
    return str.empty() ? "" : std::format("({})", str);
}

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
