#include <GLFW/glfw3.h>
#include "BaseApplication.hpp"

namespace My {
class GLFWApplication : public BaseApplication {
public:
    GLFWApplication(GfxConfiguration& config) : BaseApplication(config) {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

protected:
    GLFWwindow* m_window;
};
}  // namespace My
