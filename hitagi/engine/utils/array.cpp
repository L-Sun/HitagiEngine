export module utils:array;
import std;

export namespace hitagi::utils {

template <typename T, std::size_t N>
constexpr std::array<T, N> create_array(T&& value) {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, N>{(static_cast<void>(I), std::forward<T>(value))...};
    }(std::make_index_sequence<N>{});
}

template <typename T, std::size_t N, typename... Args>
constexpr std::array<T, N> create_array_inplace(Args&&... args) {
    static_assert(std::is_constructible_v<T, Args...>, "Can not construct array inplace");

    auto construct_fn = [&](std::size_t i) -> T {
        return T{std::forward<Args>(args)...};
    };

    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, N>{{construct_fn(I)...}};
    }(std::make_index_sequence<N>{});
}

}  // namespace hitagi::utils
