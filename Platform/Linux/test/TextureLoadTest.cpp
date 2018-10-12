#include <iostream>
#include "glad/glad.h"
#include "OpenGL/OpenGLGraphicsManager.hpp"
#include "MemoryManager.hpp"
#include "AssetLoader.hpp"
#include "BmpParser.hpp"
#include "GLFWApplication.hpp"

using namespace std;
using namespace My;

namespace My {
class TestGraphicsManager : public OpenGLGraphicsManager {
public:
    using OpenGLGraphicsManager::OpenGLGraphicsManager;
    void DrawBitmap(const Image imgae[], int32_t index);

private:
};
class TestApplication : public GLFWApplication {
public:
    using GLFWApplication::GLFWApplication;
    virtual int  Initialize();
    virtual void OnDraw();
    GLuint       readFboId;
    unsigned int tex;

private:
    Image m_Image[2];
};

GfxConfiguration config("Texture Load Test (Windows)", 8, 8, 8, 8, 32, 0, 0,
                        1024, 512);
IApplication* g_pApp = static_cast<IApplication*>(new TestApplication(config));
GraphicsManager* g_pGraphicsManager =
    static_cast<GraphicsManager*>(new TestGraphicsManager);
MemoryManager* g_pMemoryManager =
    static_cast<MemoryManager*>(new MemoryManager);

}  // namespace My

int My::TestApplication::Initialize() {
    int result;
    result = GLFWApplication::Initialize();

    if (result != 0) {
        std::cout << "application initialization failed!" << std::endl;
    } else {
        AssetLoader asset_loader;
        BmpParser   parser;
        Buffer buf = asset_loader.SyncOpenAndRead("Textures/icelogo-color.bmp",
                                                  AssetLoader::MY_OPEN_BINARY);
        m_Image[0] = parser.Parse(buf);
    }

    result = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (!result) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    } else {
        result = 0;
    }
    glViewport(0, 0, m_Config.screenWidth, m_Config.screenHeight);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_Image[0].Width, m_Image[0].Height,
                 0, GL_BGRA, GL_UNSIGNED_BYTE,
                 static_cast<void*>(m_Image[0].data));

    readFboId = 0;
    glGenFramebuffers(1, &readFboId);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFboId);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    return result;
}

void TestApplication::OnDraw() {
    dynamic_cast<TestGraphicsManager*>(g_pGraphicsManager)
        ->DrawBitmap(m_Image, 0);
}

void TestGraphicsManager::DrawBitmap(const Image img[], int32_t index) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      reinterpret_cast<TestApplication*>(g_pApp)->readFboId);
    glBlitFramebuffer(0, 0, 512, 512, 0, 0, 512, 512, GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}