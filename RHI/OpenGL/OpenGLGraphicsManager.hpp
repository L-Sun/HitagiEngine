#pragma once
#include "GraphicsManager.hpp"
#include "glad/glad.h"

namespace My {
class OpenGLGraphicsManager : public GraphicsManager {
public:
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

private:
};
}  // namespace My