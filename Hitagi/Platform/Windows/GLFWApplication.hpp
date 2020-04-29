#pragma once
#include <map>

#include <GLFW/glfw3.h>

#include "BaseApplication.hpp"

namespace Hitagi {
class GLFWApplication : public BaseApplication {
public:
    GLFWApplication(GfxConfiguration& config) : BaseApplication(config) {}
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;
    void UpdateInputState() final;

    GLFWwindow* GetWindow() { return m_Window; }

private:
    static void ProcessScroll(GLFWwindow*, double, double);

    GLFWwindow* m_Window;
};
}  // namespace Hitagi
