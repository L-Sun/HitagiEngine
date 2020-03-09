#pragma once
#include "GLFWApplication.hpp"

namespace Hitagi {
class OpenGLApplication : public GLFWApplication {
public:
    OpenGLApplication(GfxConfiguration& config) : GLFWApplication(config) {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();
};
}  // namespace Hitagi
