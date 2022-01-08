#pragma once

template <class... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };
// 显式推导指引（ C++20 起不需要）
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;