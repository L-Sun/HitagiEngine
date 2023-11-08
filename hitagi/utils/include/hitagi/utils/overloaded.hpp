#pragma once

namespace hitagi::utils {

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

}  // namespace hitagi::utils
