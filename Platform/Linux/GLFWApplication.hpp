#include <GLFW/glfw3.h>
#include "BaseApplication.hpp"
#include <map>

namespace My {
class GLFWApplication : public BaseApplication {
public:
    GLFWApplication(GfxConfiguration& config) : BaseApplication(config) {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

protected:
    GLFWwindow* m_window;

private:
    std::map<unsigned int, unsigned int> WindowHintConfig = {
        {GLFW_CONTEXT_VERSION_MAJOR, 4},
        {GLFW_CONTEXT_VERSION_MINOR, 5},
        {GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE},
        {GLFW_SAMPLES, 4}};
};
}  // namespace My
