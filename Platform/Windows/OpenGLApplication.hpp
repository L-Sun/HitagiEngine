#pragma once
#include "WindowsApplication.hpp"

using namespace My;
class OpenGLApplication : public WindowsApplication {
public:
    OpenGLApplication(GfxConfiguration& config) : WindowsApplication(config) {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

private:
    HDC   m_hDC;
    HGLRC m_RenderContext;
};
