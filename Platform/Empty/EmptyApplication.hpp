#pragma once
#include "BaseApplication.hpp"

namespace My {

class EmptyApplication : public BaseApplication {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;
};
}  // namespace My
