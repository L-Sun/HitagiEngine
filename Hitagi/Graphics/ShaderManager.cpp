#include <iostream>
#include "ShaderManager.hpp"

namespace Hitagi::Graphics {
std::string TypeToString(const ShaderType& type) {
    switch (type) {
        case ShaderType::VERTEX:
            return "Vertex";
        case ShaderType::PIXEL:
            return "Pixel";
        case ShaderType::GEOMETRY:
            return "Geometry";
    }
    return "Unkown";
}

void ShaderManager::LoadShader(std::filesystem::path shaderPath, ShaderType type, std::string name) {
    Core::Buffer data = g_FileIOManager->SyncOpenAndReadBinary(shaderPath);
    if (data.GetDataSize() == 0) {
        spdlog::get("GraphicsManager")->error("[ShaderManager] Give up loading shader.");
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
            spdlog::get("GraphicsManager")->error("[ShaderManager] Unsupport shader type: {}", TypeToString(type));
    }
}

}  // namespace Hitagi::Graphics
