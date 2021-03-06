#include "ShaderManager.hpp"

#include <spdlog/spdlog.h>

namespace Hitagi::Graphics {
int  ShaderManager::Initialize() { return 0; }
void ShaderManager::Finalize() {
    m_VertexShaders.clear();
    m_PixelShaders.clear();
}
void ShaderManager::Tick() {}

std::string TypeToString(const ShaderType& type) {
    switch (type) {
        case ShaderType::VERTEX:
            return "Vertex";
        case ShaderType::PIXEL:
            return "Pixel";
        case ShaderType::GEOMETRY:
            return "Geometry";
        case ShaderType::COMPUTE:
            return "Compute";
    }
    return "Unkown";
}

void ShaderManager::LoadShader(std::filesystem::path shaderPath, ShaderType type, std::string name) {
    auto data = g_FileIOManager->SyncOpenAndReadBinary(shaderPath);
    if (data.Empty()) {
        spdlog::get("GraphicsManager")->error("[ShaderManager] Give up loading shader.");
        return;
    }
    if (name.empty()) name = shaderPath.filename().string();
    switch (type) {
        case ShaderType::VERTEX:
            m_VertexShaders.emplace(name, std::make_shared<VertexShader>(data));
            break;
        case ShaderType::PIXEL:
            m_PixelShaders.emplace(name, std::make_shared<PixelShader>(data));
            break;
        case ShaderType::COMPUTE:
            m_ComputeShaders.emplace(name, std::make_shared<ComputeShader>(data));
        default:
            spdlog::get("GraphicsManager")->error("[ShaderManager] Unsupport shader type: {}", TypeToString(type));
    }
}

}  // namespace Hitagi::Graphics
