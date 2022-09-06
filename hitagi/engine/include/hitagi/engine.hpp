#pragma once
#include <hitagi/core/runtime_module.hpp>

#include <vector>
#include <set>
#include <concepts>

namespace hitagi {
class Engine : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;

    inline std::string_view GetName() const noexcept final { return "Engine"; }
};

}  // namespace hitagi