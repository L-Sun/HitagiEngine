#pragma once
#include "BaseApplication.hpp"

namespace Hitagi {

class EmptyApplication : public BaseApplication {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;
};
}  // namespace Hitagi
