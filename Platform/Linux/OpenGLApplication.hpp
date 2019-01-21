#pragma once
#include "GLFWApplication.hpp"

using namespace My;
class OpenGLApplication : public GLFWApplication {
public:
    OpenGLApplication(GfxConfiguration& config) : GLFWApplication(config) {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

};
