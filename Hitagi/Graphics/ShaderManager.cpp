#include <iostream>
#include "ShaderManager.hpp"

namespace Hitagi::Graphics {
void ShaderManager::LoadShader(std::filesystem::path shaderPath, ShaderType type, std::string name) {
    Core::Buffer data = g_FileIOManager->SyncOpenAndReadBinary(shaderPath);
    if (data.GetDataSize() == 0) {
        std::cerr << "[ShaderManager] Give up loading shader." << std::endl;
        return;
    }
    if (name.empty()) name = shaderPath.filename().replace_extension().u8string();
    switch (type) {
        case ShaderType::VERTEX:
            m_VertexShaders.insert({std::move(name), VertexShader(std::move(data))});
            break;
        case ShaderType::PIXEL:
            m_PixelShaders.insert({std::move(name), PixelShader(std::move(data))});
            break;
        default:
            std::cerr << "[ShaderManager] Unsupport shader type: " << type << std::endl;
    }
}
std::ostream& operator<<(std::ostream& out, const ShaderType& type) {
    switch (type) {
        case ShaderType::VERTEX:
            return out << "Vertex";
        case ShaderType::PIXEL:
            return out << "Pixel";
        case ShaderType::GEOMETRY:
            return out << "Geometry";
        default:
            break;
    }
    return out;
}
}  // namespace Hitagi::Graphics
