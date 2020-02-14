#include <iostream>
#include <filesystem>

namespace My {

struct GfxConfiguration {
    GfxConfiguration(std::string_view appName = "MyGameEngine", uint32_t r = 8, uint32_t g = 8, uint32_t b = 8,
                     uint32_t a = 8, uint32_t d = 24, uint32_t s = 0, uint32_t msaa = 0, uint32_t width = 1920,
                     uint32_t height = 1080, std::string_view fontFace = "Asset/Fonts/Hasklig-Light.otf")
        : appName(appName),
          redBits(r),
          greenBits(g),
          blueBits(b),
          alphaBits(a),
          depthBits(d),
          stencilBits(s),
          msaaSample(msaa),
          screenWidth(width),
          screenHeight(height),
          fontFace(fontFace) {}

    std::string           appName;
    uint32_t              redBits;
    uint32_t              greenBits;
    uint32_t              blueBits;
    uint32_t              alphaBits;
    uint32_t              depthBits;
    uint32_t              stencilBits;
    uint32_t              msaaSample;
    uint32_t              screenWidth;
    uint32_t              screenHeight;
    std::filesystem::path fontFace;

    friend std::ostream& operator<<(std::ostream& out, const GfxConfiguration& conf) {
        out << "GfxConfiguration:"
            << "Name:" << conf.appName << "\n"
            << "R:" << conf.redBits << "\n"
            << "G:" << conf.greenBits << "\n"
            << "B:" << conf.blueBits << "\n"
            << "A:" << conf.alphaBits << "\n"
            << "D:" << conf.depthBits << "\n"
            << "S:" << conf.stencilBits << "\n"
            << "M:" << conf.msaaSample << "\n"
            << "W:" << conf.screenWidth << "\n"
            << "H:" << conf.screenHeight << "\n"
            << std::endl;
        return out;
    }
};

}  // namespace My
